#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <map>
#include <utility>
#include <cassert>
#include <cpp-httplib/httplib.h>
#include <gason/gason.h>
#include <lnsModRoute.h>
#include <costCache.h>

/*
캐싱 기본 전략
m_stationCache이 있을 때에는 이것만 사용
m_stationCache이 없을때에는 아래의 방법으로 처리
vehicle이 제일 먼저 갈 수 있는 node는 무조건 조회한다.
: start_loc (onboarding 인 경우는 destination_loc)

onboard, waiting 인 경우는 캐싱에 있는 내용을 우선, 없으면 조회 후 업데이트
(onboard + waiting) * (onboard + waiting) 은 최대한 캐싱 처리

new 인 경우는 (onboard + waiting + new) -> (new), (new) -> (onboard + waiting - new) 로 무조건 조회
new 는 요청할 경우 들어오는데 기본적으로 많지 않고,
아직 확정이 된 경우가 아니기 때문에 캐싱할 필요가 없음.
*/

extern std::string logNow();

CCostCache::CCostCache(std::chrono::seconds maxAge)
    : m_maxAge(maxAge)
{
}

CCostCache::~CCostCache()
{
}

void CCostCache::clear()
{
    m_mapId.clear();
    m_expirationTimes.clear();
    m_distCache.clear();
    m_timeCache.clear();
    m_lastLocHash.clear();
    m_lastDistMatrix.clear();
    m_lastTimeMatrix.clear();
}

bool CCostCache::checkChangedItem(const ModRequest &modRequest, std::vector<int>& changed)
{
    changed.clear();

    if (!modRequest.locHash.empty() && m_lastLocHash == modRequest.locHash) {
        // 이전에 처리한 것과 같은 요청이면 변경된 것이 없음
        return false;
    }

    if (!m_stationCache.empty()) {
        return checkForStationCache(modRequest, changed);
    } else {
        return checkForLocalCache(modRequest, changed);
    }
}

bool CCostCache::checkForStationCache(const ModRequest &modRequest, std::vector<int>& changed)
{
    int idx = 0;
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++, idx++) {
        auto &onboard = modRequest.onboardDemands[i];
        if (isEdgeCached(onboard.destinationLoc.station_id)) {
            continue;
        }
        changed.push_back(idx);
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++, idx++) {
        auto &waiting = modRequest.onboardWaitingDemands[i];
        if (isEdgeCached(waiting.startLoc.station_id) && isEdgeCached(waiting.destinationLoc.station_id)) {
            continue;
        }
        changed.push_back(idx);
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++, idx++) {
        auto &newDemand = modRequest.newDemands[i];
        if (isEdgeCached(newDemand.startLoc.station_id) && isEdgeCached(newDemand.destinationLoc.station_id)) {
            continue;
        }
        changed.push_back(idx);
    }

    return true;
}

bool CCostCache::checkForLocalCache(const ModRequest &modRequest, std::vector<int>& changed)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();

    // onboard + waiting 목록만 cache에서 처리
    int idx = 0;
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++, idx++) {
        auto it = m_mapId.find(modRequest.onboardDemands[i].id);
        if (it == m_mapId.end()) {
            changed.push_back(idx);
        } else if (m_expirationTimes[it->second] < now) {
            changed.push_back(idx);
        }
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++, idx++) {
        auto it = m_mapId.find(modRequest.onboardWaitingDemands[i].id);
        if (it == m_mapId.end()) {
            changed.push_back(idx);
        } else if (m_expirationTimes[it->second] < now) {
            changed.push_back(idx);
        }
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++, idx++) {
        changed.push_back(idx);
    }

    // std::cout << logNow() << " costCache changed size: " << changed.size() << std::endl;

    return true;
}

void CCostCache::updateCacheAndCost(const ModRequest &modRequest, size_t nodeCount, const std::vector<int>& changed, std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix)
{
    if (!modRequest.locHash.empty() && modRequest.locHash == m_lastLocHash) {
        distMatrix = m_lastDistMatrix;
        timeMatrix = m_lastTimeMatrix;
        return;
    }

    // auto start = std::chrono::high_resolution_clock::now();

    if (!m_stationCache.empty()) {
        updateForStationCache(modRequest, nodeCount, changed, distMatrix, timeMatrix);
    } else {
        updateForLocalCache(modRequest, nodeCount, changed, distMatrix, timeMatrix);
    }

    // auto end = std::chrono::high_resolution_clock::now();
    // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // std::cout << logNow() << " updateCacheAndCost nodeCount=" << nodeCount << "  changed=" << changed.size() << "  duration=" << duration << " ms" << std::endl;
}

void CCostCache::updateForStationCache(const ModRequest &modRequest, size_t nodeCount, const std::vector<int>& changed, std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix)
{
    std::vector<std::string> cacheStation(modRequest.onboardDemands.size() + 2 * modRequest.onboardWaitingDemands.size() + 2 * modRequest.newDemands.size());
    size_t idx = 0;
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++, idx++) {
        auto &onboard = modRequest.onboardDemands[i];
        if (isEdgeCached(onboard.destinationLoc.station_id)) {
            cacheStation[idx] = onboard.destinationLoc.station_id;
        }
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++, idx+=2) {
        auto &waiting = modRequest.onboardWaitingDemands[i];
        if (isEdgeCached(waiting.startLoc.station_id)) {
            cacheStation[idx] = waiting.startLoc.station_id;
        }
        if (isEdgeCached(waiting.destinationLoc.station_id)) {
            cacheStation[idx + 1] = waiting.destinationLoc.station_id;
        }
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++, idx+=2) {
        auto &newDemand = modRequest.newDemands[i];
        if (isEdgeCached(newDemand.startLoc.station_id)) {
            cacheStation[idx] = newDemand.startLoc.station_id;
        }
        if (isEdgeCached(newDemand.destinationLoc.station_id)) {
            cacheStation[idx + 1] = newDemand.destinationLoc.station_id;
        }
    }
    size_t base = modRequest.vehicleLocs.size() + 1;
    for (size_t i = 0; i < cacheStation.size(); i++) {
        if (cacheStation[i].empty()) {
            continue;
        }
        for (size_t j = 0; j < cacheStation.size(); j++) {
            if (cacheStation[j].empty()) {
                continue;
            }
            auto& cacheValue = m_stationCache[std::make_pair(cacheStation[i], cacheStation[j])];
            distMatrix[(i + base) * (nodeCount + 1) + (j + base)] = cacheValue.first;
            timeMatrix[(i + base) * (nodeCount + 1) + (j + base)] = cacheValue.second;
        }
    }

    if (!modRequest.locHash.empty()) {
        m_lastLocHash = modRequest.locHash;
        m_lastDistMatrix = distMatrix;
        m_lastTimeMatrix = timeMatrix;
    }
}

void CCostCache::updateForLocalCache(const ModRequest &modRequest, size_t nodeCount, const std::vector<int>& changed, std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 현재 onboard + waiting 에 있는 것은 조회해서 cost array에 입력
    // changed 에 있는 것은 cost array에 있는 것을 cache에 업데이트
    // 오래된 항목은 캐시에서 삭제
    size_t onboardBase = modRequest.vehicleLocs.size() + 1;
    size_t waitingBase = onboardBase + modRequest.onboardDemands.size();

    std::vector<int> cacheIdx(modRequest.onboardDemands.size() + 2 * modRequest.onboardWaitingDemands.size(), -1);
    if (cacheIdx.size() == 0) {
        return;
    }
    std::vector<int> expiredIdx(cacheIdx.size(), -1);

    // 캐싱에 있는 dist, time 값으로 distMatrix, timeMatrix 채우기 위한 작업
    size_t onboardSizeInChange = modRequest.onboardDemands.size();
    size_t waitingSizeInChange = onboardSizeInChange + modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto it = m_mapId.find(modRequest.onboardDemands[i].id);
        if (it != m_mapId.end()) {
            cacheIdx[i] = it->second + 1;   // destination_loc
        }
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto it = m_mapId.find(modRequest.onboardWaitingDemands[i].id);
        if (it != m_mapId.end()) {
            cacheIdx[2 * i + onboardSizeInChange] = it->second;    // start_loc
            cacheIdx[2 * i + onboardSizeInChange + 1] = it->second + 1;    // destination_loc
        }
    }
    // changed에 있는 항목중에서 만약 현재 캐시에 있는 것이면 expiredIdx에 추가
    for (size_t i = 0; i < changed.size(); i++) {
        int c = changed[i];
        if (c < onboardSizeInChange) {
            if (cacheIdx[c] != -1) {
                expiredIdx[c] = cacheIdx[c];
            }
        } else if (c < waitingSizeInChange) {
            c -= onboardSizeInChange;
            if (cacheIdx[2 * c + onboardSizeInChange] != -1) {
                expiredIdx[2 * c + onboardSizeInChange] = cacheIdx[2 * c + onboardSizeInChange];
            }
            if (cacheIdx[2 * c + onboardSizeInChange + 1] != -1) {
                expiredIdx[2 * c + onboardSizeInChange + 1] = cacheIdx[2 * c + onboardSizeInChange + 1];
            }
        }
    }
    size_t base = modRequest.vehicleLocs.size() + 1;
    for (size_t i = 0; i < cacheIdx.size(); i++) {
        if (cacheIdx[i] == -1 || expiredIdx[i] != -1) {
            // from이 changed에 있는 것(캐시에 없거나 만료된 것)으로
            // 이미 dist, time 조회한 값이 있음
            continue;
        }
        for (size_t j = 0; j < cacheIdx.size(); j++) {
            if (cacheIdx[j] == -1 || expiredIdx[j] != -1) {
                // to가 changed에 있는 것(캐시에 없거나 만료된 것)으로
                // 이미 dist, time 조회한 값이 있음
                continue;
            }
            distMatrix[(i + base) * (nodeCount + 1) + (j + base)] = m_distCache[cacheIdx[i]][cacheIdx[j]];
            timeMatrix[(i + base) * (nodeCount + 1) + (j + base)] = m_timeCache[cacheIdx[i]][cacheIdx[j]];
        }
    }
    if (changed.size() > 0) {
        auto expirayAt = std::chrono::steady_clock::now() + m_maxAge;
        // changed에 해당 하는 dist, time 값을 캐싱에 업데이트 해야 됨
        // 기존에 없던 것(cacheIdx[i] == -1)이면 추가로 캐싱에 넣어야 됨
        // 만료된 것(expiredIdx[i] != -1)이면 기존의 항목을 업데이트하고 만료시간을 갱신
        std::vector<size_t> changedIdx(changed.size());
        size_t cur_size = m_distCache.size();
        size_t unchanged_size = cur_size;
        for (size_t i = 0; i < changed.size(); i++) {
            int c = changed[i];
            if (c < onboardSizeInChange) {
                // changed at onboard
                if (cacheIdx[c] == -1) {
                    cacheIdx[c] = cur_size + 1;
                    m_mapId[modRequest.onboardDemands[c].id] = cur_size;
                    changedIdx[i] = cur_size + 1;
                    cur_size += 2;
                } else {
                    assert(expiredIdx[c] == cacheIdx[c]);
                    changedIdx[i] = cacheIdx[c];
                    m_expirationTimes[cacheIdx[c]] = expirayAt;
                }
            } else if (c < waitingSizeInChange) {
                // changed at waiting
                c -= onboardSizeInChange;
                auto c_i = 2 * c + onboardSizeInChange;
                if (cacheIdx[c_i] == -1) {
                    assert(cacheIdx[c_i + 1] == -1);
                    cacheIdx[c_i] = cur_size;
                    cacheIdx[c_i + 1] = cur_size + 1;
                    m_mapId[modRequest.onboardWaitingDemands[c].id] = cur_size;
                    changedIdx[i] = cur_size;
                    cur_size += 2;
                } else {
                    assert(cacheIdx[c_i + 1] != -1);
                    assert(expiredIdx[c_i] == cacheIdx[c_i]);
                    assert(expiredIdx[c_i + 1] == cacheIdx[c_i + 1]);
                    changedIdx[i] = cacheIdx[c_i];
                    m_expirationTimes[cacheIdx[c_i]] = expirayAt;
                    m_expirationTimes[cacheIdx[c_i + 1]] = expirayAt;
                }
            }
        }
        if (cur_size > unchanged_size) {
            // 캐시에 새로운 값이 추가되었으므로 캐시의 크기를 변경
            m_distCache.resize(cur_size);
            for (auto& r_dist : m_distCache) {
                r_dist.resize(cur_size);
            }
            m_timeCache.resize(cur_size);
            for (auto& r_time : m_timeCache) {
                r_time.resize(cur_size);
            }
            m_expirationTimes.resize(cur_size, expirayAt);
        }
        // from changed to (onboard + waiting) 인 것을 먼저 처리
        for (size_t i = 0; i < changed.size(); i++) {
            int c = changed[i];
            if (c < onboardSizeInChange) {
                // onboard
                size_t cacheRowIdx = changedIdx[i];    // destination_loc
                auto& distCacheRow = m_distCache[cacheRowIdx];
                auto& timeCacheRow = m_timeCache[cacheRowIdx];
                size_t colIdx = (c + onboardBase) * (nodeCount + 1) + onboardBase;
                for (size_t j = 0; j < cacheIdx.size(); j++) {
                    assert(cacheIdx[j] != -1);  // changed 까지 설정되어 있어야 됨
                    distCacheRow[cacheIdx[j]] = distMatrix[colIdx + j];
                    timeCacheRow[cacheIdx[j]] = timeMatrix[colIdx + j];
                }
            } else if (c < waitingSizeInChange) {
                // waiting
                for (size_t r_loc = 0; r_loc < 2; r_loc++) {
                    // r_loc == 0: start_loc, r_loc == 1: destination_loc
                    size_t cacheRowIdx = changedIdx[i] + r_loc;
                    auto& distCacheRow = m_distCache[cacheRowIdx];
                    auto& timeCacheRow = m_timeCache[cacheRowIdx];
                    size_t colIdx = (2 * (c - onboardSizeInChange) + r_loc + waitingBase) * (nodeCount + 1) + onboardBase;
                    for (size_t j = 0; j < cacheIdx.size(); j++) {
                        assert(cacheIdx[j] != -1);  // changed 까지 설정되어 있어야 됨
                        distCacheRow[cacheIdx[j]] = distMatrix[colIdx + j];
                        timeCacheRow[cacheIdx[j]] = timeMatrix[colIdx + j];
                    }
                }
            }
        }
        // from (onboard + waiting - changed) to changed 인 것을 처리
        for (size_t i = 0; i < cacheIdx.size(); i++) {
            // cache에서 찾을 수 없었거나 (cacheIdx[i] == -1)
            // 신규로 추가된 것이거나 (cacheIdx[i] >= unchanged_size)
            // 캐싱이 만료된 것이면 (expiredAt[i] != -1) changed 라고 판단하고 pass
            if (cacheIdx[i] == -1 || cacheIdx[i] >= unchanged_size || expiredIdx[i] != -1) {
                continue;
            }
            int cacheRowIdx = cacheIdx[i];
            auto& distCacheRow = m_distCache[cacheRowIdx];
            auto& timeCacheRow = m_timeCache[cacheRowIdx];

            size_t costRowIdx = (i + onboardBase) * (nodeCount + 1);
            for (auto c : changed) {
                if (c < onboardSizeInChange) {
                    // onboard
                    size_t costColIdx = costRowIdx + (c + onboardBase);
                    distCacheRow[cacheIdx[c]] = distMatrix[costColIdx];
                    timeCacheRow[cacheIdx[c]] = timeMatrix[costColIdx];
                } else if (c < waitingSizeInChange) {
                    // waiting
                    int cacheColIdx = 2 * (c - onboardSizeInChange) + onboardSizeInChange;
                    size_t costColIdx = costRowIdx + (2 * (c - onboardSizeInChange) + waitingBase);
                    distCacheRow[cacheIdx[cacheColIdx]] = distMatrix[costColIdx];
                    distCacheRow[cacheIdx[cacheColIdx + 1]] = distMatrix[costColIdx + 1];
                    timeCacheRow[cacheIdx[cacheColIdx]] = timeMatrix[costColIdx];
                    timeCacheRow[cacheIdx[cacheColIdx + 1]] = timeMatrix[costColIdx + 1];
                }
            }
        }
    }

    if (!modRequest.locHash.empty()) {
        m_lastLocHash = modRequest.locHash;
        m_lastDistMatrix = distMatrix;
        m_lastTimeMatrix = timeMatrix;
    }

    // 캐시에서 앞에서부터 now 보다 오래된 항목의 위치 검색
    auto now = std::chrono::steady_clock::now();
    size_t expired_size = 0;
    for (size_t i = 0; i < m_expirationTimes.size(); i++) {
        if (m_expirationTimes[i] >= now) {
            expired_size = i;
            break;
        }
    }
    int del_size = (int)(expired_size / 2) * 2;
    if (del_size == 0) {
        return;
    }

    // 삭제할 위치에 속하는 value는 삭제하고, 그렇지 않은 value는 del_size 만큼 뺌
    for (auto it = m_mapId.begin(); it != m_mapId.end(); ) {
        if (it->second < del_size) {
            it = m_mapId.erase(it);
        } else {
            it->second -= del_size;
            ++it;
        }
    }
    // 캐시의 모든 값들을 del_size 만큼 앞에서 삭제
    m_distCache.erase(m_distCache.begin(), m_distCache.begin() + del_size);
    for (auto& r_dist : m_distCache) {
        r_dist.erase(r_dist.begin(), r_dist.begin() + del_size);
    }
    m_timeCache.erase(m_timeCache.begin(), m_timeCache.begin() + del_size);
    for (auto& r_time : m_timeCache) {
        r_time.erase(r_time.begin(), r_time.begin() + del_size);
    }
    m_expirationTimes.erase(m_expirationTimes.begin(), m_expirationTimes.begin() + del_size);
}

void CCostCache::setMaxAge(std::chrono::seconds maxAge)
{
    m_maxAge = maxAge;
}

void CCostCache::addEdge(std::string fromNode, std::string toNode, int64_t dist, int64_t time)
{
    m_stationCache[std::make_pair(fromNode, toNode)] = std::make_pair(dist, time);
}

bool CCostCache::getEdge(std::string fromNode, std::string toNode, int64_t& dist, int64_t& time)
{
    auto it = m_stationCache.find(std::make_pair(fromNode, toNode));
    if (it != m_stationCache.end()) {
        dist = it->second.first;
        time = it->second.second;
        return true;
    }
    return false;
}

bool CCostCache::isEdgeCached(std::string fromNode)
{
    if (fromNode.empty()) {
        return false;
    }
    auto it = m_stationCache.find(std::make_pair(fromNode, fromNode));
    return it != m_stationCache.end();
}


void CCostCache::loadStationCache(const std::string& cacheDir, const std::string& cacheKey)
{
    std::filesystem::path fullPath = std::filesystem::path(cacheDir) / std::filesystem::path(cacheKey);
    if (!std::filesystem::exists(fullPath)) {
        throw std::runtime_error("Cache key not found");
    }

    loadStationCache(fullPath.string());
}

void CCostCache::loadStationCache(const std::string& fullPath)
{
    std::filesystem::path path(fullPath);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Cache file not found");
    }

    auto lastWriteTime = std::filesystem::last_write_time(path);

    if (fullPath == m_lastLoadedCachePath && lastWriteTime == m_lastLoadedCacheTime) {
        return;
    }

    std::ifstream fs(fullPath);
    if (!fs.is_open()) {
        throw std::runtime_error("Failed to open cache file");
    }

    std::unordered_map<std::pair<std::string, std::string>, std::pair<int64_t, int64_t>, pair_hash> stationCache;
    std::string line;
    while(std::getline(fs, line)) {
        std::istringstream iss(line);
        std::string fromNode, toNode;
        int64_t dist, time;
        iss >> fromNode >> toNode >> dist >> time;
        auto key = std::make_pair(fromNode, toNode);
        auto value = std::make_pair(dist, time);
        stationCache[key] = value;
    }

    m_stationCache.swap(stationCache);

    // 로드된 파일 정보 업데이트
    m_lastLoadedCachePath = fullPath;
    m_lastLoadedCacheTime = lastWriteTime;
}

void CCostCache::exportStationCache(const std::string& path)
{
    std::ofstream fs(path);
    if (!fs.is_open()) {
        throw std::runtime_error("Failed to open cache file");
    }

    for (auto& [key, value] : m_stationCache) {
        fs << key.first << " " << key.second << " " << value.first << " " << value.second << std::endl;
    }
}

void CCostCache::clearStationCache()
{
    m_stationCache.clear();
    m_lastLoadedCachePath.clear();
}

CCostCache g_costCache;


void pushStationToIdx(StationToIdxMap& stationToIdx, const std::string& stationId, int direction, int idx)
{
    if (stationId.empty()) {
        // stationId < 0 은 station 정보가 없다는 것이므로, 캐싱이 불가능 (하려면 위치 정보를 이용해야 되는데, 아직은 처리 안함)
        // 캐싱을 할 수 없어서 stationToIdx 에 값을 무조건 넣어야 하는 방식으로 -idx 값을 키로로 넣어서 처리
        // 그러면 원래 stationId가 있는 경우나 없는 경우 모두 stationToIdx에 값이 들어가게 됨
        // stationId 값을 바꾸는 것을 해보려고 했는데 const ModRequest 조건 때문에 바꾸지 않는 것으로 함        
        stationToIdx[std::make_pair(std::to_string(-idx), -1)] = idx;
    } else {
        auto it = stationToIdx.find(makeStationKey(stationId, direction));
        if (it == stationToIdx.end()) {
            stationToIdx[makeStationKey(stationId, direction)] = idx;
        }
    }
}

int updateFromVehicleCostMatrixWithStationCache(
    const ModRequest& modRequest,
    const size_t nodeCount,
    StationToIdxMap stationToIdx,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    size_t baseVehicle = 1; // 0 = ghost depot 
    // 여기에서 stationToIdx를 이용하는 이유는 위에서 FromVehicle 은
    // cost cache를 사용하지 못하기 때문에 vehicle에서 모든 request 에 대해 cost를 직접 조회해야 되는데
    // 이때 최대한 정류장 중복을 막기 위해 사용하는 것이다
    // 조회한 이후에 정류장 중복 처리를 아래와 같이 해야 함

    // 아래의 과정은 i 번째 항목이 cache 되어 있는 지 확인해서 cache 되어 있으면 해당 cache의 내용을 복사해오는 과정
    size_t baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto &destinationLoc = modRequest.onboardDemands[i].destinationLoc;
        if (destinationLoc.station_id.empty()) {
            continue;
        }
        auto cacheIdx = stationToIdx[makeStationKey(destinationLoc.station_id, destinationLoc.direction)];
        if (cacheIdx != (baseIdx + i)) {
            // cacheIdx 의 내용을 baseIdx + i 로 복사
            for (int j = 0; j < modRequest.vehicleLocs.size(); j++) {
                int src_idx = (j + baseVehicle) * (nodeCount + 1) + (cacheIdx + baseVehicle);
                int dest_idx = (j + baseVehicle) * (nodeCount + 1) + (baseIdx + i + baseVehicle);
#ifdef CHECK_COST_VEHICLE_ONLY_ASSIGNED
                if (modRequest.vehicleLocs[j].supplyIdx != modRequest.onboardDemands[i].supplyIdx) {
                    distMatrix[dest_idx] = INT_MAX;
                    timeMatrix[dest_idx] = INT_MAX;
                } else {
                    distMatrix[dest_idx] = distMatrix[src_idx];
                    timeMatrix[dest_idx] = timeMatrix[src_idx];
                }
#else
                distMatrix[dest_idx] = distMatrix[src_idx];
                timeMatrix[dest_idx] = timeMatrix[src_idx];
#endif
            }
        }
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto &startLoc = modRequest.onboardWaitingDemands[i].startLoc;
        if (startLoc.station_id.empty()) {
            continue;
        }
        auto cacheIdx = stationToIdx[makeStationKey(startLoc.station_id, startLoc.direction)];
        if (cacheIdx != (baseIdx + i * 2)) {
            // cacheIdx의 내용을 baseIdx + i * 2 로 복사
            for (int j = 0; j < modRequest.vehicleLocs.size(); j++) {
                int src_idx = (j + baseVehicle) * (nodeCount + 1) + (cacheIdx + baseVehicle);
                int dest_idx = (j + baseVehicle) * (nodeCount + 1) + (baseIdx + i * 2 + baseVehicle);
#ifdef CHECK_COST_VEHICLE_ONLY_ASSIGNED
                if (modRequest.vehicleLocs[j].supplyIdx != modRequest.onboardWaitingDemands[i].supplyIdx) {
                    distMatrix[dest_idx] = INT_MAX;
                    timeMatrix[dest_idx] = INT_MAX;
                } else {
                    distMatrix[dest_idx] = distMatrix[src_idx];
                    timeMatrix[dest_idx] = timeMatrix[src_idx];
                }
#else
                distMatrix[dest_idx] = distMatrix[src_idx];
                timeMatrix[dest_idx] = timeMatrix[src_idx];
#endif
            }
        }
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto &startLoc = modRequest.newDemands[i].startLoc;
        if (startLoc.station_id.empty()) {
            continue;
        }
        auto cacheIdx = stationToIdx[makeStationKey(startLoc.station_id, startLoc.direction)];
        if (cacheIdx != (baseIdx + i * 2)) {
            // cacheIdx의 내용을 baseIdx + i * 2 로 복사
            for (int j = 0; j < modRequest.vehicleLocs.size(); j++) {
                int src_idx = (j + baseVehicle) * (nodeCount + 1) + (cacheIdx + baseVehicle);
                int dest_idx = (j + baseVehicle) * (nodeCount + 1) + (baseIdx + i * 2 + baseVehicle);
                distMatrix[dest_idx] = distMatrix[src_idx];
                timeMatrix[dest_idx] = timeMatrix[src_idx];
            }
        }
    }

    return 0;
}

void copyCostMatrixFromIndexToIndex(
    const size_t nodeCount,
    const size_t baseVehicle,
    const int fromIdx,
    const int fromCacheIdx,
    const int toIdx,
    const Location& toLoc,
    StationToIdxMap& stationToIdx,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    int toCacheIdx;
    if (toLoc.station_id.empty()) {
        toCacheIdx = toIdx;
    } else {
        toCacheIdx = stationToIdx[makeStationKey(toLoc.station_id, toLoc.direction)];
    }
    if (fromIdx == fromCacheIdx && toIdx == toCacheIdx) {
        return;
    }
    int src_idx = (baseVehicle + fromCacheIdx) * (nodeCount + 1) + (baseVehicle + toCacheIdx);
    int dest_idx = (baseVehicle + fromIdx) * (nodeCount + 1) + (baseVehicle + toIdx);
    distMatrix[dest_idx] = distMatrix[src_idx];
    timeMatrix[dest_idx] = timeMatrix[src_idx];
}


int updateChangedCostMatrixWithStationCache(
    const ModRequest& modRequest,
    const size_t nodeCount,
    const std::vector<int>& changed,
    const std::vector<int>& notChanged,
    StationToIdxMap& stationToIdx,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    size_t baseVehicle = 1; // 0 = ghost depot 
    size_t onboardBase = modRequest.vehicleLocs.size();
    size_t waitingBase = onboardBase + modRequest.onboardDemands.size();
    size_t newBase = waitingBase + 2 * modRequest.onboardWaitingDemands.size();
    size_t onboardSizeInChange = modRequest.onboardDemands.size();
    size_t waitingSizeInChange = onboardSizeInChange + modRequest.onboardWaitingDemands.size();
    
    auto copyCostMatrixFromIdxChanged = [&](int _fromIdx, const Location& _fromLoc, const std::vector<int>& _targetChanged) {
        int fromCacheIdx;
        int toIdx;
        if (_fromLoc.station_id.empty()) {
            fromCacheIdx = _fromIdx;
        } else {
            fromCacheIdx = stationToIdx[makeStationKey(_fromLoc.station_id, _fromLoc.direction)];
        }
        for (auto v : _targetChanged) {
            if (v < onboardSizeInChange) {
                toIdx = v + onboardBase;
                auto& toLoc = modRequest.onboardDemands[v].destinationLoc;
                copyCostMatrixFromIndexToIndex(nodeCount, baseVehicle, _fromIdx, fromCacheIdx, toIdx, toLoc, stationToIdx, distMatrix, timeMatrix);
            } else if (v < waitingSizeInChange) {
                toIdx = 2 * (v - onboardSizeInChange) + waitingBase;
                auto& toStartLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc;
                copyCostMatrixFromIndexToIndex(nodeCount, baseVehicle, _fromIdx, fromCacheIdx, toIdx, toStartLoc, stationToIdx, distMatrix, timeMatrix);
                toIdx++;
                auto& toDestLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc;
                copyCostMatrixFromIndexToIndex(nodeCount, baseVehicle, _fromIdx, fromCacheIdx, toIdx, toDestLoc, stationToIdx, distMatrix, timeMatrix);
            } else {
                toIdx = 2 * (v - waitingSizeInChange) + newBase;
                auto& toStartLoc = modRequest.newDemands[v - waitingSizeInChange].startLoc;
                copyCostMatrixFromIndexToIndex(nodeCount, baseVehicle, _fromIdx, fromCacheIdx, toIdx, toStartLoc, stationToIdx, distMatrix, timeMatrix);
                toIdx++;
                auto& toDestLoc = modRequest.newDemands[v - waitingSizeInChange].destinationLoc;
                copyCostMatrixFromIndexToIndex(nodeCount, baseVehicle, _fromIdx, fromCacheIdx, toIdx, toDestLoc, stationToIdx, distMatrix, timeMatrix);
            }
        }
    };

    // (onboard + waiting + new) * changed
    int fromIdx;
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        fromIdx = i + onboardBase;
        auto& fromLoc = modRequest.onboardDemands[i].destinationLoc;
        copyCostMatrixFromIdxChanged(fromIdx, fromLoc, changed);
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        fromIdx = 2 * i + waitingBase;
        auto& fromStartLoc = modRequest.onboardWaitingDemands[i].startLoc;
        copyCostMatrixFromIdxChanged(fromIdx, fromStartLoc, changed);
        fromIdx++;
        auto& fromDestLoc = modRequest.onboardWaitingDemands[i].destinationLoc;
        copyCostMatrixFromIdxChanged(fromIdx, fromDestLoc, changed);
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        fromIdx = 2 * i + newBase;
        auto& fromStartLoc = modRequest.newDemands[i].startLoc;
        copyCostMatrixFromIdxChanged(fromIdx, fromStartLoc, changed);
        fromIdx++;
        auto& fromDestLoc = modRequest.newDemands[i].destinationLoc;
        copyCostMatrixFromIdxChanged(fromIdx, fromDestLoc, changed);
    }

    // change * (not_changed)
    if (!notChanged.empty()) {
        for (auto v : changed) {
            if (v < onboardSizeInChange) {
                fromIdx = v + onboardBase;
                auto& fromLoc = modRequest.onboardDemands[v].destinationLoc;
                copyCostMatrixFromIdxChanged(fromIdx, fromLoc, notChanged);
            } else if (v < waitingSizeInChange) {
                fromIdx = 2 * (v - onboardSizeInChange) + waitingBase;
                auto& fromStartLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc;
                copyCostMatrixFromIdxChanged(fromIdx, fromStartLoc, notChanged);
                fromIdx++;
                auto& fromDestLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc;
                copyCostMatrixFromIdxChanged(fromIdx, fromDestLoc, notChanged);
            } else {
                fromIdx = 2 * (v - waitingSizeInChange) + newBase;
                auto& fromStartLoc = modRequest.newDemands[v - waitingSizeInChange].startLoc;
                copyCostMatrixFromIdxChanged(fromIdx, fromStartLoc, notChanged);
                fromIdx++;
                auto& fromDestLoc = modRequest.newDemands[v - waitingSizeInChange].destinationLoc;
                copyCostMatrixFromIdxChanged(fromIdx, fromDestLoc, notChanged);
            }
        }
    }

    return 0;
}
