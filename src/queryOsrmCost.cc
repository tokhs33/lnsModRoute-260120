#include <sstream>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <utility>
#include <future>
#include <memory>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cassert>
#include <cpp-httplib/httplib.h>
#include <gason/gason.h>
#include <lnsModRoute.h>
#include <costCache.h>

#define OSRM_RES_DISTANCE   "distances"
#define OSRM_RES_DURATION   "durations"
#define OSRM_MAX_LOCATIONS  100

// #define CHECK_COST_CACHE

extern CCostCache g_costCache;

extern std::string logNow();

class CTaskOsrm {
public:
    std::vector<int> m_sources;
    std::vector<int> m_destinations;
    std::string m_query;

    CTaskOsrm(const std::vector<int>& sources, const std::vector<int>& destinations, const std::string& query)
        : m_sources(sources), m_destinations(destinations), m_query(query) {}
};

std::string makeOsrmSelectedIndexParams(const char *name, const std::vector<int>& selected)
{
    if (selected.size() == 0) {
        return "";
    }

    std::ostringstream oss;
    oss << "&" << name << "=" << selected[0];
    for (int i = 1; i < selected.size(); i++) {
        oss << ";" << selected[i];
    }
    return oss.str();
}

std::shared_ptr<CTaskOsrm> queryCostOsrmQuery(
    const std::string& routePath,
    const std::string& query,
    const std::vector<int>& sources,
    const std::vector<int>& destinations)
{
    httplib::Client httpClient(routePath.c_str());
    auto res = httpClient.Get(query.c_str());
    if (!res) {
        auto err = res.error();
        throw std::runtime_error("Fail to connect to OSRM");
    } else if (res->status != 200) {
        throw std::runtime_error("Fail to get cost from OSRM");
    }

    return std::make_shared<CTaskOsrm>(sources, destinations, res->body.c_str());
}

void parseOsrmResponse(
    const std::string& q_response,
    const size_t baseVehicle,
    const size_t nodeCount,
    const std::vector<int>& sources,
    const std::vector<int>& destinations,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse((char*) q_response.c_str(), &endptr, &value, allocator);
    if (status != JSON_OK) {
        throw std::runtime_error("Fail to parse OSRM response");
    }
    if (value.getTag() != JSON_OBJECT || !value.toNode()) {
        throw std::runtime_error("Invalid OSRM response format");
    }
    for (auto item : value) {
        if (strcmp(item->key, OSRM_RES_DISTANCE) == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                size_t rowIdx = 0;
                for (auto row : item->value) {
                    if (row->value.getTag() == JSON_ARRAY && row->value.toNode()) {
                        size_t baseIdx = (sources[rowIdx] + baseVehicle) * (nodeCount + 1) + baseVehicle;
                        size_t colIdx = 0;
                        for (auto col : row->value) {
                            distMatrix[baseIdx + destinations[colIdx++]] = col->value.getTag() == JSON_NULL ? INT_MAX : static_cast<int64_t>(std::ceil(col->value.toNumber()));
                        }
                        rowIdx++;
                    }
                }
            }
        } else if (strcmp(item->key, OSRM_RES_DURATION) == 0) {                    
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                size_t rowIdx = 0;
                for (auto row : item->value) {
                    if (row->value.getTag() == JSON_ARRAY && row->value.toNode()) {
                        size_t baseIdx = (sources[rowIdx] + baseVehicle) * (nodeCount + 1) + baseVehicle;
                        size_t colIdx = 0;
                        for (auto col : row->value) {
                            timeMatrix[baseIdx + destinations[colIdx++]] = col->value.getTag() == JSON_NULL ? INT_MAX : static_cast<int64_t>(std::ceil(col->value.toNumber()));
                        }
                        rowIdx++;
                    }
                }
            }
        }
    }
}

void queryCostOsrmTask(
    const std::string& routePath,
    const size_t routeTasks,
    const size_t baseVehicle,
    const size_t nodeCount,
    std::deque<std::shared_ptr<CTaskOsrm>>& tasks,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    if (tasks.empty()) {
        if (showLog) {
            std::cout << logNow() << " queryCostOsrmTask empty task" << std::endl;
        }
        return;
    }

    size_t taskCount = tasks.size();
    auto start = std::chrono::high_resolution_clock::now();

    std::list<std::future<std::shared_ptr<CTaskOsrm>>> futures;

    size_t runSize = std::min(routeTasks, taskCount);
    for (size_t i = 0; i < runSize; i++) {
        auto task = tasks.front();
        futures.push_back(std::async(std::launch::async,
            queryCostOsrmQuery,
            routePath, task->m_query, task->m_sources, task->m_destinations));
        tasks.pop_front();
    }

    while (!futures.empty()) {
        for (auto it = futures.begin(); it != futures.end(); ) {
            auto status = it->wait_for(std::chrono::milliseconds(1));
            if (status == std::future_status::ready) {
                if (!tasks.empty()) {
                    auto task = tasks.front();
                    futures.push_back(std::async(std::launch::async,
                        queryCostOsrmQuery,
                        routePath, task->m_query, task->m_sources, task->m_destinations));
                    tasks.pop_front();
                }
                auto result = it->get();
                parseOsrmResponse(result->m_query, baseVehicle, nodeCount, result->m_sources, result->m_destinations, distMatrix, timeMatrix);
                it = futures.erase(it); // 완료된 future는 삭제
            } else {
                ++it;
            }
        }
    }

    if (showLog) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << logNow() << " queryCostOsrmTask[" << taskCount << "] " << duration << " ms" << std::endl;
    }
}

void makeTaskOsrmIndex(
    const std::string& url,
    const std::vector<int>& sources,
    const size_t sourceCount,
    const std::vector<int>& destinations,
    const size_t destinationCount,
    std::deque<std::shared_ptr<CTaskOsrm>>& tasks)
{
    if (sourceCount == 0 || destinationCount == 0) {
        return;
    }

    for (size_t s = 0; s < sourceCount; s += OSRM_MAX_LOCATIONS) {
        std::vector<int> sub_sources(sources.begin() + s, sources.begin() + std::min(sourceCount, s + OSRM_MAX_LOCATIONS));
        std::string q_source = makeOsrmSelectedIndexParams("sources", sub_sources);
        for (size_t d = 0; d < destinationCount; d += OSRM_MAX_LOCATIONS) {
            std::vector<int> sub_destinations(destinations.begin() + d, destinations.begin() + std::min(destinationCount, d + OSRM_MAX_LOCATIONS));
            std::string q_dest = makeOsrmSelectedIndexParams("destinations", sub_destinations);
            std::string query = url + q_source + q_dest;
            tasks.emplace_back(std::make_shared<CTaskOsrm>(sub_sources, sub_destinations, query));
        }
    }
}

void queryCostOsrmAll(
    const ModRequest& modRequest,
    const std::string& routePath,
    const size_t routeTasks,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "/table/v1/driving/";
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicleLocs = modRequest.vehicleLocs[i];
        oss << ((i == 0) ? "" : ";") << vehicleLocs.location.lng << "," << vehicleLocs.location.lat;
    }
    for (auto& onboardDemand : modRequest.onboardDemands) {
        oss << ";" << onboardDemand.destinationLoc.lng << "," << onboardDemand.destinationLoc.lat;
    }
    for (auto& waitingDemand : modRequest.onboardWaitingDemands) {
        oss << ";" << waitingDemand.startLoc.lng << "," << waitingDemand.startLoc.lat;
        oss << ";" << waitingDemand.destinationLoc.lng << "," << waitingDemand.destinationLoc.lat;
    }
    for (auto& newDemand : modRequest.newDemands) {
        oss << ";" << newDemand.startLoc.lng << "," << newDemand.startLoc.lat;
        oss << ";" << newDemand.destinationLoc.lng << "," << newDemand.destinationLoc.lat;
    }
    oss << "?annotations=distance,duration";

    std::vector<int> index(nodeCount);
    for (int i = 0; i < nodeCount; i++) {
        index[i] = i;
    }

    std::deque<std::shared_ptr<CTaskOsrm>> tasks;
    std::string url = oss.str();
    for (size_t s = 0; s < nodeCount; s += OSRM_MAX_LOCATIONS) {
        std::vector<int> sources(index.begin() + s, index.begin() + std::min(nodeCount, s + OSRM_MAX_LOCATIONS));
        for (size_t d = 0; d < nodeCount; d += OSRM_MAX_LOCATIONS) {
            std::vector<int> destinations(index.begin() + d, index.begin() + std::min(nodeCount, d + OSRM_MAX_LOCATIONS));
            makeTaskOsrmIndex(url, sources, sources.size(), destinations, destinations.size(), tasks);
        }
    }

    queryCostOsrmTask(routePath, routeTasks, 1, nodeCount, tasks, distMatrix, timeMatrix, showLog);
}

void functionOsrmCostFromVehicle(
    const ModRequest& modRequest,
    const std::string& url,
    const std::vector<Location>& locs,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::unordered_map<std::string, int>& demandIdToIdx,
    const std::unordered_map<std::string, int>& supplyIdToIdx,
    std::deque<std::shared_ptr<CTaskOsrm>>& tasks)
{
    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);
#ifdef CHECK_COST_VEHICLE_ONLY_ASSIGNED
    std::set<int> newDemandsDestinations;
    size_t baseIdx = modRequest.vehicleLocs.size() + modRequest.onboardDemands.size() + 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        auto& newDemandStartLoc = modRequest.newDemands[i].startLoc;
        assert(modRequest.newDemands[i].startLoc.station_id == locs[baseIdx + i * 2].station_id);
        if (!newDemandStartLoc.station_id.empty()) {
            auto it = stationToIdx.find(makeStationKey(newDemandStartLoc.station_id, -1));
            if (it != stationToIdx.end()) {
                newDemandsDestinations.insert(it->second);
            } else {
                newDemandsDestinations.insert(baseIdx + i * 2); // start loc
            }
        } else {
            newDemandsDestinations.insert(baseIdx + i * 2); // start loc
        }
    }
    std::set<std::string> notAssignedVehicles;
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicle = modRequest.vehicleLocs[i];
        notAssignedVehicles.insert(vehicle.supplyIdx);
    }
    for (size_t i = 0; i < modRequest.assigned.size(); i++) {
        auto& assigned = modRequest.assigned[i];
        auto it = notAssignedVehicles.find(assigned.supplyIdx);
        if (it != notAssignedVehicles.end()) {
            notAssignedVehicles.erase(it);
        }
    }
    if (!notAssignedVehicles.empty()) {
        // not assigned vehicle -> new_demand 조회
        size_t sourceCount = 0;
        for (std::string supplyIdx: notAssignedVehicles) {
            auto it = supplyIdToIdx.find(supplyIdx);
            if (it != supplyIdToIdx.end()) {
                sources[sourceCount++] = it->second;
            }
        }
        size_t destCount = newDemandsDestinations.size();
        std::copy(newDemandsDestinations.begin(), newDemandsDestinations.end(), destinations.begin());

        makeTaskOsrmIndex(url, sources, sourceCount, destinations, destCount, tasks);
    }

    // TODO: waitingDemand, onboardDemand에는 assigned 되어 있다고 하지만 assigned에 없는 경우는 나중에 처리
    // for each assigned vehicle -> new_demand + assigned demand
    for (auto assigned : modRequest.assigned) {
        auto it = supplyIdToIdx.find(assigned.supplyIdx);
        if (it != supplyIdToIdx.end()) {
            std::set<int> assignedDestinations(newDemandsDestinations);
            sources[0] = it->second; // vehicle location
            for (auto& routeOrder : assigned.routeOrder) {
                auto demandIt = demandIdToIdx.find(routeOrder.first);
                if (demandIt == demandIdToIdx.end()) {
                    continue;
                }
                auto& loc = locs[demandIt->second];
                if (!loc.station_id.empty()) {
                    auto it = stationToIdx.find(makeStationKey(loc.station_id, -1));
                    if (it != stationToIdx.end()) {
                        assignedDestinations.insert(it->second);
                    } else {
                        assignedDestinations.insert(demandIt->second);
                    }
                } else {
                    assignedDestinations.insert(demandIt->second);
                }
            }
            size_t destCount = assignedDestinations.size();
            std::copy(assignedDestinations.begin(), assignedDestinations.end(), destinations.begin());
            makeTaskOsrmIndex(url, sources, 1, destinations, destCount, tasks);
        }
    }
#else
    // vehicle -> start_loc 조회
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        sources[i] = i;
    }
    size_t destCount = 0;
    for (auto it = stationToIdx.begin(); it != stationToIdx.end(); it++) {
        destinations[destCount++] = it->second;
    }

    makeTaskOsrmIndex(url, sources, modRequest.vehicleLocs.size(), destinations, destCount, tasks);
#endif
}

void functionOsrmCostToNewChanged(
    const ModRequest& modRequest,
    const std::string& url,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::vector<int>& changed,
    std::deque<std::shared_ptr<CTaskOsrm>>& tasks)
{
    if (changed.empty()) {
        return;
    }

    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);

    auto insertLocationIdx = [&](const Location& loc, std::set<int>& set, size_t idx) {
        if (!loc.station_id.empty()) {
            auto it = stationToIdx.find(makeStationKey(loc.station_id, -1));
            if (it != stationToIdx.end()) {
                set.insert(it->second);
            } else {
                set.insert(idx);
            }
        } else {
            set.insert(idx);
        }
    };

    // (onboard + waiting + new) * change (= change + new)
    std::set<int> sourceSet;
    size_t onboardBase = modRequest.vehicleLocs.size();
    size_t waitingBase = onboardBase + modRequest.onboardDemands.size();
    size_t newBase = waitingBase + 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& loc = modRequest.onboardDemands[i].destinationLoc;
        insertLocationIdx(loc, sourceSet, i + onboardBase);
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& startLoc = modRequest.onboardWaitingDemands[i].startLoc;
        insertLocationIdx(startLoc, sourceSet, 2 * i + waitingBase);

        auto& destLoc = modRequest.onboardWaitingDemands[i].destinationLoc;
        insertLocationIdx(destLoc, sourceSet, 2 * i + 1 + waitingBase);
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        auto& startLoc = modRequest.newDemands[i].startLoc;
        insertLocationIdx(startLoc, sourceSet, 2 * i + newBase);

        auto& destLoc = modRequest.newDemands[i].destinationLoc;
        insertLocationIdx(destLoc, sourceSet, 2 * i + newBase + 1);
    }

    std::set<int> destSet;
    size_t onboardSizeInChange = modRequest.onboardDemands.size();
    size_t waitingSizeInChange = onboardSizeInChange + modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < changed.size(); i++) {
        auto v = changed[i];
        if (v < onboardSizeInChange) {
            auto &loc = modRequest.onboardDemands[v].destinationLoc;
            insertLocationIdx(loc, destSet, v + onboardBase);
        } else if (v < waitingSizeInChange) {
            auto &startLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc;
            insertLocationIdx(startLoc, destSet, 2 * (v - onboardSizeInChange) + waitingBase);

            auto &destLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc;
            insertLocationIdx(destLoc, destSet, 2 * (v - onboardSizeInChange) + waitingBase + 1);
        } else {
            auto &startLoc = modRequest.newDemands[v - waitingSizeInChange].startLoc;
            insertLocationIdx(startLoc, destSet, 2 * (v - waitingSizeInChange) + newBase);

            auto &destLoc = modRequest.newDemands[v - waitingSizeInChange].destinationLoc;
            insertLocationIdx(destLoc, destSet, 2 * (v - waitingSizeInChange) + newBase + 1);
        }
    }
    std::copy(sourceSet.begin(), sourceSet.end(), sources.begin());
    std::copy(destSet.begin(), destSet.end(), destinations.begin());
    makeTaskOsrmIndex(url, sources, sourceSet.size(), destinations, destSet.size(), tasks);
}

void functionOsrmCostFromNewChanged(
    const ModRequest& modRequest,
    const std::string& url,
    const std::vector<Location>& locs,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::vector<int>& changed,
    const std::vector<int>& notChanged,
    std::deque<std::shared_ptr<CTaskOsrm>>& tasks)
{
    if (changed.empty() || notChanged.empty()) {
        return;
    }

    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);

    // (new + changed) * (onboard + waiting - changed) 조회
    size_t onboardBase = modRequest.vehicleLocs.size();
    size_t waitingBase = onboardBase + modRequest.onboardDemands.size();
    size_t newBase =  waitingBase + 2 * modRequest.onboardWaitingDemands.size();
    size_t onboardSizeInChange = modRequest.onboardDemands.size();
    size_t waitingSizeInChange = onboardSizeInChange + modRequest.onboardWaitingDemands.size();

    auto insertLocationIdx = [&](const Location& loc, std::set<int>& set, size_t idx) {
        if (!loc.station_id.empty()) {
            auto it = stationToIdx.find(makeStationKey(loc.station_id, -1));
            if (it != stationToIdx.end()) {
                set.insert(it->second);
            } else {
                set.insert(idx);
            }
        } else {
            set.insert(idx);
        }
    };

    std::set<int> sourceSet;
    for (size_t i = 0; i < changed.size(); i++) {
        auto v = changed[i];
        if (v < onboardSizeInChange) {
            auto &loc = modRequest.onboardDemands[v].destinationLoc;
            insertLocationIdx(loc, sourceSet, v + onboardBase);
        } else if (v < waitingSizeInChange) {
            auto &startLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc;
            insertLocationIdx(startLoc, sourceSet, 2 * (v - onboardSizeInChange) + waitingBase);

            auto &destLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc;
            insertLocationIdx(destLoc, sourceSet, 2 * (v - onboardSizeInChange) + waitingBase + 1);
        } else {
            auto &startLoc = modRequest.newDemands[v - waitingSizeInChange].startLoc;
            insertLocationIdx(startLoc, sourceSet, 2 * (v - waitingSizeInChange) + newBase);

            auto &destLoc = modRequest.newDemands[v - waitingSizeInChange].destinationLoc;
            insertLocationIdx(destLoc, sourceSet, 2 * (v - waitingSizeInChange) + newBase + 1);
        }
    }

    std::set<int> destSet;
    for (size_t i = 0; i < notChanged.size(); i++) {
        auto v = notChanged[i];
        if (v < onboardSizeInChange) {
            auto &loc = modRequest.onboardDemands[v].destinationLoc;
            insertLocationIdx(loc, destSet, v + onboardBase);
        } else if (v < waitingSizeInChange) {
            auto &startLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc;
            insertLocationIdx(startLoc, destSet, 2 * (v - onboardSizeInChange) + waitingBase);

            auto &destLoc = modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc;
            insertLocationIdx(destLoc, destSet, 2 * (v - onboardSizeInChange) + waitingBase + 1);
        } else {
            auto &startLoc = modRequest.newDemands[v - waitingSizeInChange].startLoc;
            insertLocationIdx(startLoc, destSet, 2 * (v - waitingSizeInChange) + newBase);

            auto &destLoc = modRequest.newDemands[v - waitingSizeInChange].destinationLoc;
            insertLocationIdx(destLoc, destSet, 2 * (v - waitingSizeInChange) + newBase + 1);
        }
    }

    std::copy(sourceSet.begin(), sourceSet.end(), sources.begin());
    std::copy(destSet.begin(), destSet.end(), destinations.begin());
    makeTaskOsrmIndex(url, sources, sourceSet.size(), destinations, destSet.size(), tasks);
}

int queryCostOsrmNotInCache(
    const ModRequest &modRequest,
    const std::string& routePath,
    const size_t routeTasks,
    size_t nodeCount,
    const std::vector<int>& changed,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    StationToIdxMap stationToIdx;
    std::unordered_map<std::string, int> demandIdToIdx;
    std::unordered_map<std::string, int> supplyIdToIdx;
    std::vector<Location> locs(nodeCount);
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8);
    oss << "/table/v1/driving/";
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicleLocs = modRequest.vehicleLocs[i];
        locs[i] = vehicleLocs.location;
        supplyIdToIdx[vehicleLocs.supplyIdx] = i;
        oss << ((i == 0) ? "" : ";") << vehicleLocs.location.lng << "," << vehicleLocs.location.lat;
    }
    size_t baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        locs[baseIdx + i] = onboardDemand.destinationLoc;
        oss << ";" << onboardDemand.destinationLoc.lng << "," << onboardDemand.destinationLoc.lat;
        pushStationToIdx(stationToIdx, onboardDemand.destinationLoc.station_id, -1, baseIdx + i);
        demandIdToIdx[onboardDemand.id] = baseIdx + i;
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        locs[baseIdx + i * 2] = waitingDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = waitingDemand.destinationLoc;
        oss << ";" << waitingDemand.startLoc.lng << "," << waitingDemand.startLoc.lat;
        oss << ";" << waitingDemand.destinationLoc.lng << "," << waitingDemand.destinationLoc.lat;
        pushStationToIdx(stationToIdx, waitingDemand.startLoc.station_id, -1, baseIdx + i * 2);
        pushStationToIdx(stationToIdx, waitingDemand.destinationLoc.station_id, -1, baseIdx + i * 2 + 1);
        demandIdToIdx[waitingDemand.id] = baseIdx + i * 2;
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto& newDemand = modRequest.newDemands[i];
        locs[baseIdx + i * 2] = newDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = newDemand.destinationLoc;
        oss << ";" << newDemand.startLoc.lng << "," << newDemand.startLoc.lat;
        oss << ";" << newDemand.destinationLoc.lng << "," << newDemand.destinationLoc.lat;
        pushStationToIdx(stationToIdx, newDemand.startLoc.station_id, -1, baseIdx + i * 2);
        pushStationToIdx(stationToIdx, newDemand.destinationLoc.station_id, -1, baseIdx + i * 2 + 1);
        demandIdToIdx[newDemand.id] = baseIdx + i * 2;
    }
    oss << "?annotations=distance,duration";
    std::string url = oss.str();

    // changed가 아닌 not changed 목록을 찾기 위한 작업
    // changed인덱스는 onboard + waiting 내에서의 index 이것을 query cost 할 때의 index로 변환하는 것이 중요
    std::vector<int> notChanged(modRequest.onboardDemands.size() + modRequest.onboardWaitingDemands.size() + modRequest.newDemands.size(), 0);
    for (auto c : changed) {
        notChanged[c] = -1;
    }
    size_t notChangedCount = 0;
    for (size_t i = 0; i < notChanged.size(); i++) {
        if (notChanged[i] == 0) {
            notChanged[notChangedCount++] = i;
        }
    }
    notChanged.resize(notChangedCount);

    size_t baseVehicle = 1; // 0 = ghost depot

    std::deque<std::shared_ptr<CTaskOsrm>> tasks;
    functionOsrmCostFromVehicle(modRequest, url, locs, nodeCount, stationToIdx, demandIdToIdx, supplyIdToIdx, tasks);
    functionOsrmCostToNewChanged(modRequest, url, nodeCount, stationToIdx, changed, tasks);
    functionOsrmCostFromNewChanged(modRequest, url, locs, nodeCount, stationToIdx, changed, notChanged, tasks);

    queryCostOsrmTask(routePath, routeTasks, baseVehicle, nodeCount, tasks, distMatrix, timeMatrix, showLog);

    updateFromVehicleCostMatrixWithStationCache(modRequest, nodeCount, stationToIdx, distMatrix, timeMatrix);
    updateChangedCostMatrixWithStationCache(modRequest, nodeCount, changed, notChanged, stationToIdx, distMatrix, timeMatrix);

    return 0;
}


int queryCostOsrmReset()
{
    g_costCache.clear();
    return 0;
}

#ifdef CHECK_COST_CACHE
void testCostOsrmCache(
    const ModRequest& modRequest,
    const std::string& routePath,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    std::vector<int64_t> checkDistMatrix(distMatrix.size());
    std::vector<int64_t> checkTimeMatrix(timeMatrix.size());
    queryCostOsrmAll(modRequest, routePath, 4, nodeCount, checkDistMatrix, checkTimeMatrix, true);
    // check vehicle -> start_loc
    size_t baseVehicle = 1;
    size_t baseOnboard = baseVehicle + modRequest.vehicleLocs.size();
    size_t baseWaiting = baseOnboard + modRequest.onboardDemands.size();
    size_t baseNewDemand = baseWaiting + 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        size_t rowIdx = baseVehicle + i;
        for (size_t j = 0; j < modRequest.onboardDemands.size(); j++) {
            size_t matrixIdx = rowIdx * (nodeCount + 1) + baseOnboard + j;
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
        }
        for (size_t j = 0; j < modRequest.onboardWaitingDemands.size(); j++) {
            size_t matrixIdx = rowIdx * (nodeCount + 1) + baseWaiting + 2 * j;
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
        }
        for (size_t j = 0; j < modRequest.newDemands.size(); j++) {
            size_t matrixIdx = rowIdx * (nodeCount + 1) + baseNewDemand + 2 * j;
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
        }
    }
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        size_t rowIdx = baseOnboard + i;
        for (size_t j = 0; j < modRequest.onboardDemands.size(); j++) {
            size_t matrixIdx = rowIdx * (nodeCount + 1) + baseOnboard + j;
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
        }
        for (size_t j = 0; j < modRequest.onboardWaitingDemands.size(); j++) {
            size_t matrixIdx = rowIdx * (nodeCount + 1) + baseWaiting + 2 * j;   // start_loc
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            matrixIdx++;    // destination_loc
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
        }
        for (size_t j = 0; j < modRequest.newDemands.size(); j++) {
            size_t matrixIdx = rowIdx * (nodeCount + 1) + baseNewDemand + 2 * j; // start_loc
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            matrixIdx++;    // destination_loc
            assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
            assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
        }
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        for (int r_loc = 0; r_loc < 2; r_loc++) {
            size_t rowIdx = baseWaiting + 2 * i + r_loc;
            for (size_t j = 0; j < modRequest.onboardDemands.size(); j++) {
                size_t matrixIdx = rowIdx * (nodeCount + 1) + baseOnboard + j;
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            }
            for (size_t j = 0; j < modRequest.onboardWaitingDemands.size(); j++) {
                size_t matrixIdx = rowIdx * (nodeCount + 1) + baseWaiting + 2 * j;   // start_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
                matrixIdx++;    // destination_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            }
            for (size_t j = 0; j < modRequest.newDemands.size(); j++) {
                size_t matrixIdx = rowIdx * (nodeCount + 1) + baseNewDemand + 2 * j; // start_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
                matrixIdx++;    // destination_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            }
        }
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        for (int r_loc = 0; r_loc < 2; r_loc++) {
            size_t rowIdx = baseNewDemand + 2 * i + r_loc;
            for (size_t j = 0; j < modRequest.onboardDemands.size(); j++) {
                size_t matrixIdx = rowIdx * (nodeCount + 1) + baseOnboard + j;
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            }
            for (size_t j = 0; j < modRequest.onboardWaitingDemands.size(); j++) {
                size_t matrixIdx = rowIdx * (nodeCount + 1) + baseWaiting + 2 * j;   // start_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
                matrixIdx++;    // destination_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            }
            for (size_t j = 0; j < modRequest.newDemands.size(); j++) {
                size_t matrixIdx = rowIdx * (nodeCount + 1) + baseNewDemand + 2 * j; // start_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
                matrixIdx++;    // destination_loc
                assert(distMatrix[matrixIdx] == checkDistMatrix[matrixIdx]);
                assert(timeMatrix[matrixIdx] == checkTimeMatrix[matrixIdx]);
            }
        }
    }
}
#endif

int queryCostOsrm(
    const ModRequest& modRequest,
    const std::string& routePath,
    const int nRouteTasks,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    // 어떤 경우이든 caching 기능을 사용하도록 수정
    // if (nodeCount <= 100) {
    //     return queryCostOsrmAll(modRequest, routePath, nodeCount, distMatrix, timeMatrixm, showLog);
    // }

    std::vector<int> changed;
    if (g_costCache.checkChangedItem(modRequest, changed)) {
        queryCostOsrmNotInCache(modRequest, routePath, nRouteTasks, nodeCount, changed, distMatrix, timeMatrix, showLog);
    }
    g_costCache.updateCacheAndCost(modRequest, nodeCount, changed, distMatrix, timeMatrix);

#ifdef CHECK_COST_CACHE
    testCostOsrmCache(modRequest, routePath, nodeCount, distMatrix, timeMatrix);
#endif

    return 0;
}
