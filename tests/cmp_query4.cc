#include <sstream>
#include <vector>
#include <list>
#include <deque>
#include <utility>
#include <future>
#include <memory>
#include <chrono>
#include <cassert>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <cpp-httplib/httplib.h>
#include <gason/gason.h>
#include <lnsModRoute.h>
#include <costCache.h>
#include "test_utility.h"

#define VALHALLA_MAX_LOCATIONS 50

class CTaskValhalla {
public:
    std::vector<int> m_sources;
    std::vector<int> m_destinations;
    std::string m_body;

    CTaskValhalla(const std::vector<int>& sources, const std::vector<int>& destinations, const std::string& body)
        : m_sources(sources), m_destinations(destinations), m_body(body) {}
};

std::string makeValhallaIndexLocs(const char *name, const std::vector<Location>& locs, const std::vector<int>& selected)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8) << "\"" << name << "\":[";
    if (selected.size() != 0) {
        oss << "{\"lat\":" << locs[selected[0]].lat << ",\"lon\":" << locs[selected[0]].lng;
        if (locs[selected[0]].direction != -1) oss << ",\"heading\":" << locs[selected[0]].direction;
        oss << "}";
        for (int i = 1; i < selected.size(); i++) {
            oss << ",{\"lat\":" << locs[selected[i]].lat << ",\"lon\":" << locs[selected[i]].lng;
            if (locs[selected[i]].direction != -1) oss << ",\"heading\":" << locs[selected[i]].direction;
            oss << "}";
        }
    }
    oss << "]";
    return oss.str();
}

std::shared_ptr<CTaskValhalla> queryCostValhallaBody(
    const std::string& routePath,
    const std::string& q_body,
    const std::vector<int>& sources,
    const std::vector<int>& destinations)
{
    httplib::Client httpClient(routePath.c_str());
    std::string url = "/sources_to_targets";

    auto res = httpClient.Post(url.c_str(), q_body.c_str(), "application/json");
    if (!res) {
        auto err = res.error();
        throw std::runtime_error("Fail to connect to Valhalla");
    } else if(res->status != 200) {
        throw std::runtime_error("Fail to get cost from Valhalla");
    }

    return std::make_shared<CTaskValhalla>(sources, destinations, res->body.c_str());
}

void parseVallhallaRespose(
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
        throw std::runtime_error("Fail to parse Valhalla response");
    }
    if (value.getTag() != JSON_OBJECT || !value.toNode()) {
        throw std::runtime_error("Invalid Valhalla response format");
    }

    bool bSearch = true;
    int search_from = 113;  // vehicle 113
    int search_to = 11 + 115; // onboard 11

    for (auto item : value) {
        if (strcmp(item->key, "sources") == 0) {

        } else if (strcmp(item->key, "targets") == 0) {

        }

        if (strcmp(item->key, "sources_to_targets") == 0) {
            if (item->value.getTag() == JSON_ARRAY && item->value.toNode()) {
                for (auto row : item->value) {
                    if (row->value.getTag() == JSON_ARRAY && row->value.toNode()) {
                        for (auto col : row->value) {
                            if (col->value.getTag() != JSON_OBJECT || !col->value.toNode()) {
                                throw std::runtime_error("Invalid Valhalla response format");
                            }
                            int64_t distance_value = 0;
                            int64_t time_value = 0;
                            int to_index = -1;
                            int from_index = -1; 
                            for (auto cost : col->value) {
                                if (strcmp(cost->key, "distance") == 0) {
                                    distance_value = cost->value.getTag() == JSON_NULL ? INT_MAX : static_cast<int64_t>(std::ceil(cost->value.toNumber() * 1000.0));
                                } else if (strcmp(cost->key, "time") == 0) {
                                    time_value = cost->value.getTag() == JSON_NULL ? INT_MAX : static_cast<int64_t>(std::ceil(cost->value.toNumber()));
                                } else if (strcmp(cost->key, "to_index") == 0) {
                                    to_index = (int) cost->value.toNumber();
                                } else if (strcmp(cost->key, "from_index") == 0) {
                                    from_index = (int) cost->value.toNumber();
                                }
                            }
                            if (to_index != -1 && from_index != -1) {
                                int idx = (sources[from_index] + baseVehicle) * (nodeCount + 1) + (destinations[to_index] + baseVehicle);
                                distMatrix[idx] = distance_value;
                                timeMatrix[idx] = time_value;
                                if (bSearch) {
                                    if (sources[from_index] == search_from && destinations[to_index] == search_to) {
                                        std::cout << "from_index: " << from_index << ", to_index: " << to_index << std::endl;
                                        std::cout << "dist: " << distance_value << ", time: " << time_value << std::endl;
                                        bSearch = false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void queryCostValhallaTask(
    const std::string& routePath,
    const size_t routeTasks,
    const size_t baseVehicle,
    const size_t nodeCount,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    if (tasks.empty()) {
        if (showLog) {
            std::cout << logNow() << " queryCostValhallaTask empty task" << std::endl;
        }
        return;
    }

    size_t taskCount = tasks.size();
    auto start = std::chrono::high_resolution_clock::now();

    std::list<std::future<std::shared_ptr<CTaskValhalla>>> futures;

    size_t runSize = std::min(routeTasks, taskCount);
    for (size_t i = 0; i < runSize; i++) {
        auto task = tasks.front();
        futures.push_back(std::async(std::launch::async,
            queryCostValhallaBody,
            routePath, task->m_body, task->m_sources, task->m_destinations));
        tasks.pop_front();
    }

    while (!futures.empty()) {
        for (auto it = futures.begin(); it != futures.end(); ) {
            auto status = it->wait_for(std::chrono::milliseconds(1));
            if (status == std::future_status::ready) {
                if (!tasks.empty()) {
                    auto task = tasks.front();
                    futures.push_back(std::async(std::launch::async,
                        queryCostValhallaBody,
                        routePath, task->m_body, task->m_sources, task->m_destinations));
                    tasks.pop_front();
                }
                auto result = it->get();
                parseVallhallaRespose(result->m_body, baseVehicle, nodeCount, result->m_sources, result->m_destinations, distMatrix, timeMatrix);
                it = futures.erase(it); // 완료된 future는 삭제
            } else {
                ++it;
            }
        }
    }

    if (showLog) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << logNow() << " queryCostValhallaTask[" << taskCount << "] " << duration << " ms" << std::endl;
    }
}

void makeTaskValhallaIndex(
    const std::vector<Location> locs,
    const std::vector<int>& sources,
    const size_t sourceCount,
    const std::vector<int>& destinations,
    const size_t destinationCount,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
{
    if (sourceCount == 0 || destinationCount == 0) {
        return;
    }

    bool bSearch = true;
    int search_from = 113;  // vehicle 113
    int search_to = 11 + 115; // onboard 11

    for (size_t s = 0; s < sourceCount; s += VALHALLA_MAX_LOCATIONS) {
        std::vector<int> sub_sources(sources.begin() + s, sources.begin() + std::min(s + VALHALLA_MAX_LOCATIONS, sourceCount));
        std::string q_source = makeValhallaIndexLocs("sources", locs, sub_sources);
        for (size_t d = 0; d < destinationCount; d += VALHALLA_MAX_LOCATIONS) {
            std::vector<int> sub_destinations(destinations.begin() + d, destinations.begin() + std::min(d + VALHALLA_MAX_LOCATIONS, destinationCount));
            std::string q_dest = makeValhallaIndexLocs("targets", locs, sub_destinations);
            std::string q_body = "{" + q_source + "," + q_dest + ",\"costing\":\"auto\",\"date_time\":{\"type\":1,\"value\":\"" + reqDateTime + "\"}}";
            if (bSearch) {
                auto it_source = std::find(sub_sources.begin(), sub_sources.end(), search_from);
                auto it_dest = std::find(sub_destinations.begin(), sub_destinations.end(), search_to);
                if (it_source != sub_sources.end() && it_dest != sub_destinations.end()) {
                    std::cout << std::endl;
                    std::cout << "source: (" << locs[*it_source].lat << "," << locs[*it_source].lng << ")" << std::endl;
                    std::cout << "dest  : (" << locs[*it_dest].lat << "," << locs[*it_dest].lng << ")" << std::endl;
                    std::cout << q_body << std::endl;
                    bSearch = false;
                }
            }
            tasks.emplace_back(std::make_shared<CTaskValhalla>(sub_sources, sub_destinations, q_body));
        }
    }

    
}

std::string getReqDateTime(const std::optional<std::string>& dateTime)
{
    if (dateTime.has_value()) {
        return dateTime.value();
    }
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_c);
    std::ostringstream oss;
    oss << std::put_time(now_tm, "%Y-%m-%dT%H:%M");
    return oss.str();
}

void queryCostValhallaAll(
    const ModRequest& modRequest,
    const std::string& routePath,
    const size_t routeTasks,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    std::vector<Location> locs(nodeCount);
    size_t baseIdx = 0;
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicleLocs = modRequest.vehicleLocs[i];
        locs[i] = vehicleLocs.location;
    }
    baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        locs[baseIdx + i] = onboardDemand.destinationLoc;
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        locs[baseIdx + i * 2] = waitingDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = waitingDemand.destinationLoc;
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto& newDemand = modRequest.newDemands[i];
        locs[baseIdx + i * 2] = newDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = newDemand.destinationLoc;
    }

    std::vector<int> index(nodeCount);
    for (int i = 0; i < nodeCount; i++) {
        index[i] = i;
    }

    std::string reqDateTime = getReqDateTime(modRequest.dateTime);

    std::deque<std::shared_ptr<CTaskValhalla>> tasks;
    for (size_t s = 0; s < nodeCount; s += VALHALLA_MAX_LOCATIONS) {
        std::vector<int> sources(index.begin() + s, index.begin() + std::min(nodeCount, s + VALHALLA_MAX_LOCATIONS));
        for (size_t d = 0; d < nodeCount; d += VALHALLA_MAX_LOCATIONS) {
            std::vector<int> destinations(index.begin() + d, index.begin() + std::min(nodeCount, d + VALHALLA_MAX_LOCATIONS));
            makeTaskValhallaIndex(locs, sources, sources.size(), destinations, destinations.size(), reqDateTime, tasks);
        }
    }

    queryCostValhallaTask(routePath, routeTasks, 1, nodeCount, tasks, distMatrix, timeMatrix, showLog);
}

void functionValhallaCostFromVehicle(
    const ModRequest& modRequest,
    const std::vector<Location> locs,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
{
    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);

    // vehicle -> start_loc 조회
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        sources[i] = i;
    }
    size_t destCount = 0;
    for (auto it = stationToIdx.begin(); it != stationToIdx.end(); it++) {
        destinations[destCount++] = it->second;
    }
    /*
    size_t nodeBase = modRequest.vehicleLocs.size();
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        destinations[destCount++] = i + nodeBase;
    }
    nodeBase += modRequest.onboardDemands.size();
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        destinations[destCount++] = 2 * i + nodeBase; // start_loc of 
    }
    nodeBase += 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        destinations[destCount++] = 2 * i + nodeBase; // start_loc
    }
    */
    makeTaskValhallaIndex(locs, sources, modRequest.vehicleLocs.size(), destinations, destCount, reqDateTime, tasks);
}

void functionValhallaCostFromVehicle_cmp(
    const ModRequest& modRequest,
    const std::vector<Location> locs,
    size_t nodeCount,
    StationToIdxMap& stationToIdx,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
{
    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);

    // vehicle -> start_loc 조회
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        sources[i] = i;
    }
    size_t destCount = 0;
    /*
    for (auto it = stationToIdx.begin(); it != stationToIdx.end(); it++) {
        destinations[destCount++] = it->second;
    }
    */
    size_t nodeBase = modRequest.vehicleLocs.size();
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        destinations[destCount++] = i + nodeBase;
    }
    nodeBase += modRequest.onboardDemands.size();
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        destinations[destCount++] = 2 * i + nodeBase; // start_loc of 
    }
    nodeBase += 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        destinations[destCount++] = 2 * i + nodeBase; // start_loc
    }
    makeTaskValhallaIndex(locs, sources, modRequest.vehicleLocs.size(), destinations, destCount, reqDateTime, tasks);
}

void functionValhallaCostToNewChanged(
    const ModRequest& modRequest,
    const std::vector<Location> locs,
    size_t nodeCount,
    const std::vector<int>& changed,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
{
    if (changed.empty()) {
        return;
    }

    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);

    // (onboard + waiting + new) * changed (= change + new)
    size_t sourceCount = 0;
    size_t onboardBase = modRequest.vehicleLocs.size();
    size_t waitingBase = onboardBase + modRequest.onboardDemands.size();
    size_t newBase = waitingBase + 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        sources[sourceCount++] = i + onboardBase;
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        sources[sourceCount++] = 2 * i + waitingBase; // start_loc
        sources[sourceCount++] = 2 * i + 1 + waitingBase; // destination_loc
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        sources[sourceCount++] = 2 * i + newBase; // start_loc
        sources[sourceCount++] = 2 * i + 1 + newBase; // destination_loc
    }

    size_t destCount = 0;
    size_t onboardSizeInChange = modRequest.onboardDemands.size();
    size_t waitingSizeInChange = onboardSizeInChange + modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < changed.size(); i++) {
        auto v = changed[i];
        if (v < onboardSizeInChange) {
            destinations[destCount++] = v + onboardBase;
        } else if (v < waitingSizeInChange) {
            destinations[destCount++] = 2 * (v - onboardSizeInChange) + waitingBase;
            destinations[destCount++] = 2 * (v - onboardSizeInChange) + waitingBase + 1;
        } else {
            destinations[destCount++] = 2 * (v - waitingSizeInChange) + newBase;
            destinations[destCount++] = 2 * (v - waitingSizeInChange) + newBase + 1;
        }
    }
    makeTaskValhallaIndex(locs, sources, sourceCount, destinations, destCount, reqDateTime, tasks);
}

void functionValhallaCostFromNewChanged(
    const ModRequest& modRequest,
    const std::vector<Location> locs,
    size_t nodeCount,
    const std::vector<int>& changed,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
{
    if (changed.empty()) {
        return;
    }

    // changed가 아닌 not changed 목록을 찾기 위한 작업
    // changed인덱스는 onboard + waiting 내에서의 index 이것을 query cost 할 때의 index로 변환하는 것이 중요
    std::vector<int> notChanged(modRequest.onboardDemands.size()+ modRequest.onboardWaitingDemands.size() + modRequest.newDemands.size(), 0);
    for (auto c : changed) {
        notChanged[c] = -1;
    }
    size_t notChangedCount = 0;
    for (size_t i = 0; i < notChanged.size(); i++) {
        if (notChanged[i] == 0) {
            notChanged[notChangedCount++] = i;
        }
    }
    if (notChangedCount == 0) {
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

    size_t sourceCount = 0;
    for (size_t i = 0; i < changed.size(); i++) {
        auto v = changed[i];
        if (v < onboardSizeInChange) {
            sources[sourceCount++] = v + onboardBase;
        } else if (v < waitingSizeInChange) {
            sources[sourceCount++] = 2 * (v - onboardSizeInChange) + waitingBase;
            sources[sourceCount++] = 2 * (v - onboardSizeInChange) + waitingBase + 1;
        } else {
            sources[sourceCount++] = 2 * (v - waitingSizeInChange) + newBase;
            sources[sourceCount++] = 2 * (v - waitingSizeInChange) + newBase + 1;
        }
    }

    size_t destCount = 0;
    for (size_t i = 0; i < notChangedCount; i++) {
        auto v = notChanged[i];
        if (v < onboardSizeInChange) {
            destinations[destCount++] = v + onboardBase;
        } else if (v < waitingSizeInChange) {
            destinations[destCount++] = 2 * (v - onboardSizeInChange) + waitingBase;
            destinations[destCount++] = 2 * (v - onboardSizeInChange) + waitingBase + 1;
        } else {
            destinations[destCount++] = 2 * (v - waitingSizeInChange) + newBase;
            destinations[destCount++] = 2 * (v - waitingSizeInChange) + newBase + 1;
        }
    }
    makeTaskValhallaIndex(locs, sources, sourceCount, destinations, destCount, reqDateTime, tasks);
}

int queryCostValhallaNotInCache(
    const ModRequest& modRequest,
    const std::string& routePath,
    const size_t routeTasks,
    size_t nodeCount,
    const std::vector<int>& changed,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    StationToIdxMap stationToIdx;
    std::vector<Location> locs(nodeCount);
    size_t baseIdx = 0;
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicleLocs = modRequest.vehicleLocs[i];
        locs[i] = vehicleLocs.location;
    }
    baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        locs[baseIdx + i] = onboardDemand.destinationLoc;
        pushStationToIdx(stationToIdx, onboardDemand.destinationLoc.station_id, onboardDemand.destinationLoc.direction, baseIdx + i);
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        locs[baseIdx + i * 2] = waitingDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = waitingDemand.destinationLoc;
        pushStationToIdx(stationToIdx, waitingDemand.startLoc.station_id,waitingDemand.startLoc.direction, baseIdx + i * 2);
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto& newDemand = modRequest.newDemands[i];
        locs[baseIdx + i * 2] = newDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = newDemand.destinationLoc;
        pushStationToIdx(stationToIdx, newDemand.startLoc.station_id, newDemand.startLoc.direction, baseIdx + i * 2);
    }

    size_t baseVehicle = 1; // 0 = ghost depot 
    std::string reqDateTime = getReqDateTime(modRequest.dateTime);

    std::deque<std::shared_ptr<CTaskValhalla>> tasks;
    functionValhallaCostFromVehicle(modRequest, locs, nodeCount, stationToIdx, reqDateTime, tasks);
    functionValhallaCostToNewChanged(modRequest, locs, nodeCount, changed, reqDateTime, tasks);
    functionValhallaCostFromNewChanged(modRequest, locs, nodeCount, changed, reqDateTime, tasks);

    queryCostValhallaTask(routePath, routeTasks, baseVehicle, nodeCount, tasks, distMatrix, timeMatrix, showLog);

    // 여기에서 stationToIdx를 이용하는 이유는 위에서 FromVehicle 은 cost cache를 사용하지 못하기 때문에
    // vehicle에서 모든 request 에 대해 cost를 직접 조회해야 되는데
    // 이때 최대한 정류장 중복을 막기 위해 사용하는 것이다
    // 조회한 이후에 정류장 중복 처리를 아래와 같이 해야 함

    baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& destinationLoc = modRequest.onboardDemands[i].destinationLoc;
        auto cacheIdx = stationToIdx[std::make_pair(destinationLoc.station_id, destinationLoc.direction)];
        if (cacheIdx != (baseIdx + i)) {
            // cacheIdx 의 내용을 baseIdx + i 로 복사
            std::cout << "copy " << parseModIdx(cacheIdx, modRequest) << " -> onboard " << i << std::endl; 
            for (int j = 0; j < modRequest.vehicleLocs.size(); j++) {
                int src_idx = (j + baseVehicle) * (nodeCount + 1) + (cacheIdx + baseVehicle);
                int dest_idx = (j + baseVehicle) * (nodeCount + 1) + (baseIdx + i + baseVehicle);
                distMatrix[dest_idx] = distMatrix[src_idx];
                timeMatrix[dest_idx] = timeMatrix[src_idx];
            }
        }
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& startLoc = modRequest.onboardWaitingDemands[i].startLoc;
        auto cacheIdx = stationToIdx[std::make_pair(startLoc.station_id, startLoc.direction)];
        if (cacheIdx != (baseIdx + i * 2)) {
            // cacheIdx의 내용을 baseIdx + i * 2 로 복사
            std::cout << "copy " << parseModIdx(cacheIdx, modRequest) << " -> waiting " << i << std::endl;
            for (int j = 0; j < modRequest.vehicleLocs.size(); j++) {
                int src_idx = (j + baseVehicle) * (nodeCount + 1) + (cacheIdx + baseVehicle);
                int dest_idx = (j + baseVehicle) * (nodeCount + 1) + (baseIdx + i * 2 + baseVehicle);
                distMatrix[dest_idx] = distMatrix[src_idx];
                timeMatrix[dest_idx] = timeMatrix[src_idx];
            }
        }
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto& startLoc = modRequest.newDemands[i].startLoc;
        auto cacheIdx = stationToIdx[std::make_pair(startLoc.station_id, startLoc.direction)];
        if (cacheIdx != (baseIdx + i * 2)) {
            // cacheIdx의 내용을 baseIdx + i * 2 로 복사
            std::cout << "copy " << parseModIdx(cacheIdx, modRequest) << " -> new " << i << std::endl;
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

int queryCostValhallaNotInCache_cmp(
    const ModRequest& modRequest,
    const std::string& routePath,
    const size_t routeTasks,
    size_t nodeCount,
    const std::vector<int>& changed,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    StationToIdxMap stationToIdx;
    std::vector<Location> locs(nodeCount);
    size_t baseIdx = 0;
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicleLocs = modRequest.vehicleLocs[i];
        locs[i] = vehicleLocs.location;
    }
    baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        locs[baseIdx + i] = onboardDemand.destinationLoc;
        pushStationToIdx(stationToIdx, onboardDemand.destinationLoc.station_id, onboardDemand.destinationLoc.direction, baseIdx + i);
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        locs[baseIdx + i * 2] = waitingDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = waitingDemand.destinationLoc;
        pushStationToIdx(stationToIdx, waitingDemand.startLoc.station_id, waitingDemand.startLoc.direction, baseIdx + i * 2);
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto& newDemand = modRequest.newDemands[i];
        locs[baseIdx + i * 2] = newDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = newDemand.destinationLoc;
        pushStationToIdx(stationToIdx, newDemand.startLoc.station_id, newDemand.startLoc.direction, baseIdx + i * 2);
    }

    size_t baseVehicle = 1; // 0 = ghost depot 
    std::string reqDateTime = getReqDateTime(modRequest.dateTime);

    std::deque<std::shared_ptr<CTaskValhalla>> tasks;
    functionValhallaCostFromVehicle_cmp(modRequest, locs, nodeCount, stationToIdx, reqDateTime, tasks);
    functionValhallaCostToNewChanged(modRequest, locs, nodeCount, changed, reqDateTime, tasks);
    functionValhallaCostFromNewChanged(modRequest, locs, nodeCount, changed, reqDateTime, tasks);

    queryCostValhallaTask(routePath, routeTasks, baseVehicle, nodeCount, tasks, distMatrix, timeMatrix, showLog);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <json_request>" << std::endl;
        return 1;
    }

    try {
        std::string content = readFile(argv[1]);
        ModRequest modRequest = parseRequest(content.c_str());

        std::string routePath = "http://localhost:8002";
        size_t nRouteTasks = argc > 2 ? std::stoi(argv[2]) : 4;

        size_t nodeCount = modRequest.vehicleLocs.size() + modRequest.onboardDemands.size()
            + 2 * (modRequest.onboardWaitingDemands.size() + modRequest.newDemands.size());

        std::cout << "test nodeCount=" << nodeCount << "  vehicleCount=" << modRequest.vehicleLocs.size() << std::endl;
        std::cout << "     onboard=" << modRequest.onboardDemands.size() << "  waiting=" << modRequest.onboardWaitingDemands.size() << "  new=" << modRequest.newDemands.size() << std::endl;

        std::vector<int64_t> distMatrix((nodeCount + 1) * (nodeCount + 1));
        std::vector<int64_t> timeMatrix((nodeCount + 1) * (nodeCount + 1));

        std::vector<int> changed;
        std::cout << "queryCostValhallaNotInCache" << std::endl;
        queryCostValhallaNotInCache(modRequest, routePath, nRouteTasks, nodeCount, changed, distMatrix, timeMatrix, true);

        std::cout << std::endl << "queryCostValhallaNotInCache_cmp" << std::endl;
        std::vector<int64_t> distMatrix_cmp((nodeCount + 1) * (nodeCount + 1));
        std::vector<int64_t> timeMatrix_cmp((nodeCount + 1) * (nodeCount + 1));
        queryCostValhallaNotInCache_cmp(modRequest, routePath, nRouteTasks, nodeCount, changed, distMatrix_cmp, timeMatrix_cmp, true);

        bool diffFound = false;

        for (int i = 0; i < distMatrix.size(); i++) {
            if (distMatrix[i] != distMatrix_cmp[i]) {
                std::cout << "distMatrix[" << parseCacheIdx(i, nodeCount, modRequest) << "] " << distMatrix[i] << " != " << distMatrix_cmp[i] << std::endl;
                diffFound = true;
            }
            if (timeMatrix[i] != timeMatrix_cmp[i]) {
                std::cout << "timeMatrix[" << parseCacheIdx(i, nodeCount, modRequest) << "] " << timeMatrix[i] << " != " << timeMatrix_cmp[i] << std::endl;
                diffFound = true;
            }
        }
        return diffFound ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
