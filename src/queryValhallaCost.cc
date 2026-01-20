#include <sstream>
#include <vector>
#include <list>
#include <deque>
#include <set>
#include <utility>
#include <future>
#include <memory>
#include <chrono>
#include <cassert>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <cpp-httplib/httplib.h>
#include <gason/gason.h>
#include <lnsModRoute.h>
#include <costCache.h>

#define VALHALLA_MAX_LOCATIONS 50

// #define CHECK_VALHALLA_COST_CACHE
// #define LOG_COST_CACHE_TIMING

extern CCostCache g_costCache;

extern std::string logNow();

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

    for (auto item : value) {
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

#ifdef LOG_COST_CACHE_TIMING
    std::cout << logNow() << " makeTaskValhallaIndex sourceCount: " << sourceCount << ", destinationCount: " << destinationCount << std::endl;
#endif

    for (size_t s = 0; s < sourceCount; s += VALHALLA_MAX_LOCATIONS) {
        std::vector<int> sub_sources(sources.begin() + s, sources.begin() + std::min(s + VALHALLA_MAX_LOCATIONS, sourceCount));
        std::string q_source = makeValhallaIndexLocs("sources", locs, sub_sources);
        for (size_t d = 0; d < destinationCount; d += VALHALLA_MAX_LOCATIONS) {
            std::vector<int> sub_destinations(destinations.begin() + d, destinations.begin() + std::min(d + VALHALLA_MAX_LOCATIONS, destinationCount));
            std::string q_dest = makeValhallaIndexLocs("targets", locs, sub_destinations);
            std::string q_body = "{" + q_source + "," + q_dest + ",\"costing\":\"auto\",\"date_time\":{\"type\":1,\"value\":\"" + reqDateTime + "\"}}";
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
    std::tm now_tm_local;
#ifdef _WIN32
    localtime_s(&now_tm_local, &now_c);
#else
    localtime_r(&now_c, &now_tm_local);
#endif
    std::ostringstream oss;
    oss << std::put_time(&now_tm_local, "%Y-%m-%dT%H:%M");
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
    const std::vector<Location>& locs,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::unordered_map<std::string, int>& demandIdToIdx,
    const std::unordered_map<std::string, int>& supplyIdToIdx,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
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
            auto it = stationToIdx.find(makeStationKey(newDemandStartLoc.station_id, newDemandStartLoc.direction));
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

        makeTaskValhallaIndex(locs, sources, sourceCount, destinations, destCount, reqDateTime, tasks);
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
                    auto it = stationToIdx.find(makeStationKey(loc.station_id, loc.direction));
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
            makeTaskValhallaIndex(locs, sources, 1, destinations, destCount, reqDateTime, tasks);
        }
    }
#else
    // TODO: vehicle 에서 모든 request에 대해서 직접 조회할 필요는 없음
    // assigned 되어 있지 않은 request에 대해서는 조회하고 assigned 되어 있는 것은 해당 vehicle에서만 조회하면 됨
    // 그런데 지금 cost 조회할 때 assigned 되어 있는 request만 조회하는 방법이 마땅치 않아서 다 조회하고 있음

    // vehicle -> start_loc 조회
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        sources[i] = i;
    }
    size_t destCount = 0;
    for (auto it = stationToIdx.begin(); it != stationToIdx.end(); it++) {
        destinations[destCount++] = it->second;
    }

    makeTaskValhallaIndex(locs, sources, modRequest.vehicleLocs.size(), destinations, destCount, reqDateTime, tasks);
#endif
}

void functionValhallaCostToNewChanged(
    const ModRequest& modRequest,
    const std::vector<Location>& locs,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::vector<int>& changed,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
{
    if (changed.empty()) {
        return;
    }

    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);

    auto insertLocationIdx = [&](const Location& loc, std::set<int>& set, size_t idx) {
        if (!loc.station_id.empty()) {
            auto it = stationToIdx.find(makeStationKey(loc.station_id, loc.direction));
            if (it != stationToIdx.end()) {
                set.insert(it->second);
            } else {
                set.insert(idx);
            }
        } else {
            set.insert(idx);
        }
    };

    // (onboard + waiting + new) * changed (= change + new)
    std::set<int> sourceSet;
    size_t onboardBase = modRequest.vehicleLocs.size();
    size_t waitingBase = onboardBase + modRequest.onboardDemands.size();
    size_t newBase = waitingBase + 2 * modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& loc = locs[i + onboardBase];
        assert(modRequest.onboardDemands[i].destinationLoc.station_id == loc.station_id);
        insertLocationIdx(loc, sourceSet, i + onboardBase);
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& startLoc = locs[2 * i + waitingBase];
        assert(modRequest.onboardWaitingDemands[i].startLoc.station_id == startLoc.station_id);
        insertLocationIdx(startLoc, sourceSet, 2 * i + waitingBase);

        auto& destLoc = locs[2 * i + 1 + waitingBase];
        assert(modRequest.onboardWaitingDemands[i].destinationLoc.station_id == destLoc.station_id);
        insertLocationIdx(destLoc, sourceSet, 2 * i + 1 + waitingBase);
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++) {
        auto& startLoc = locs[2 * i + newBase];
        assert(modRequest.newDemands[i].startLoc.station_id == startLoc.station_id);
        insertLocationIdx(startLoc, sourceSet, 2 * i + newBase);

        auto& destLoc = locs[2 * i + 1 + newBase];
        assert(modRequest.newDemands[i].destinationLoc.station_id == destLoc.station_id);
        insertLocationIdx(destLoc, sourceSet, 2 * i + 1 + newBase);
    }

    std::set<int> destSet;
    size_t onboardSizeInChange = modRequest.onboardDemands.size();
    size_t waitingSizeInChange = onboardSizeInChange + modRequest.onboardWaitingDemands.size();
    for (size_t i = 0; i < changed.size(); i++) {
        auto v = changed[i];
        if (v < onboardSizeInChange) {
            auto &loc = locs[v + onboardBase];
            assert(modRequest.onboardDemands[v].destinationLoc.station_id == loc.station_id);
            insertLocationIdx(loc, destSet, v + onboardBase);
        } else if (v < waitingSizeInChange) {
            auto &startLoc = locs[2 * (v - onboardSizeInChange) + waitingBase];
            assert(modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc.station_id == startLoc.station_id);
            insertLocationIdx(startLoc, destSet, 2 * (v - onboardSizeInChange) + waitingBase);

            auto &destLoc = locs[2 * (v - onboardSizeInChange) + 1 + waitingBase];
            assert(modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc.station_id == destLoc.station_id);
            insertLocationIdx(destLoc, destSet, 2 * (v - onboardSizeInChange) + 1 + waitingBase);
        } else {
            auto &startLoc = locs[2 * (v - waitingSizeInChange) + newBase];
            assert(modRequest.newDemands[v - waitingSizeInChange].startLoc.station_id == startLoc.station_id);
            insertLocationIdx(startLoc, destSet, 2 * (v - waitingSizeInChange) + newBase);

            auto &destLoc = locs[2 * (v - waitingSizeInChange) + 1 + newBase];
            assert(modRequest.newDemands[v - waitingSizeInChange].destinationLoc.station_id == destLoc.station_id);
            insertLocationIdx(destLoc, destSet, 2 * (v - waitingSizeInChange) + 1 + newBase);
        }
    }
    std::copy(sourceSet.begin(), sourceSet.end(), sources.begin());
    std::copy(destSet.begin(), destSet.end(), destinations.begin());
    makeTaskValhallaIndex(locs, sources, sourceSet.size(), destinations, destSet.size(), reqDateTime, tasks);
}

void functionValhallaCostFromNewChanged(
    const ModRequest& modRequest,
    const std::vector<Location>& locs,
    size_t nodeCount,
    const StationToIdxMap& stationToIdx,
    const std::vector<int>& changed,
    const std::vector<int>& notChanged,
    const std::string& reqDateTime,
    std::deque<std::shared_ptr<CTaskValhalla>>& tasks)
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
            auto it = stationToIdx.find(makeStationKey(loc.station_id, loc.direction));
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
            auto &loc = locs[v + onboardBase];
            assert(modRequest.onboardDemands[v].destinationLoc.station_id == loc.station_id);
            insertLocationIdx(loc, sourceSet, v + onboardBase);
        } else if (v < waitingSizeInChange) {
            auto &startLoc = locs[2 * (v - onboardSizeInChange) + waitingBase];
            assert(modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc.station_id == startLoc.station_id);
            insertLocationIdx(startLoc, sourceSet, 2 * (v - onboardSizeInChange) + waitingBase);

            auto &destLoc = locs[2 * (v - onboardSizeInChange) + 1 + waitingBase];
            assert(modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc.station_id == destLoc.station_id);
            insertLocationIdx(destLoc, sourceSet, 2 * (v - onboardSizeInChange) + 1 + waitingBase);
        } else {
            auto &startLoc = locs[2 * (v - waitingSizeInChange) + newBase];
            assert(modRequest.newDemands[v - waitingSizeInChange].startLoc.station_id == startLoc.station_id);
            insertLocationIdx(startLoc, sourceSet, 2 * (v - waitingSizeInChange) + newBase);

            auto &destLoc = locs[2 * (v - waitingSizeInChange) + 1 + newBase];
            assert(modRequest.newDemands[v - waitingSizeInChange].destinationLoc.station_id == destLoc.station_id);
            insertLocationIdx(destLoc, sourceSet, 2 * (v - waitingSizeInChange) + 1 + newBase);
        }
    }

    std::set<int> destSet;
    for (size_t i = 0; i < notChanged.size(); i++) {
        auto v = notChanged[i];
        if (v < onboardSizeInChange) {
            auto &loc = locs[v + onboardBase];
            assert(modRequest.onboardDemands[v].destinationLoc.station_id == loc.station_id);
            insertLocationIdx(loc, destSet, v + onboardBase);
        } else if (v < waitingSizeInChange) {
            auto &startLoc = locs[2 * (v - onboardSizeInChange) + waitingBase];
            assert(modRequest.onboardWaitingDemands[v - onboardSizeInChange].startLoc.station_id == startLoc.station_id);
            insertLocationIdx(startLoc, destSet, 2 * (v - onboardSizeInChange) + waitingBase);

            auto &destLoc = locs[2 * (v - onboardSizeInChange) + 1 + waitingBase];
            assert(modRequest.onboardWaitingDemands[v - onboardSizeInChange].destinationLoc.station_id == destLoc.station_id);
            insertLocationIdx(destLoc, destSet, 2 * (v - onboardSizeInChange) + 1 + waitingBase);
        } else {
            auto &startLoc = locs[2 * (v - waitingSizeInChange) + newBase];
            assert(modRequest.newDemands[v - waitingSizeInChange].startLoc.station_id == startLoc.station_id);
            insertLocationIdx(startLoc, destSet, 2 * (v - waitingSizeInChange) + newBase);

            auto &destLoc = locs[2 * (v - waitingSizeInChange) + 1 + newBase];
            assert(modRequest.newDemands[v - waitingSizeInChange].destinationLoc.station_id == destLoc.station_id);
            insertLocationIdx(destLoc, destSet, 2 * (v - waitingSizeInChange) + 1 + newBase);
        }
    }

    std::copy(sourceSet.begin(), sourceSet.end(), sources.begin());
    std::copy(destSet.begin(), destSet.end(), destinations.begin());
    makeTaskValhallaIndex(locs, sources, sourceSet.size(), destinations, destSet.size(), reqDateTime, tasks);
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
    std::unordered_map<std::string, int> demandIdToIdx;
    std::unordered_map<std::string, int> supplyIdToIdx;
    std::vector<Location> locs(nodeCount);
    size_t baseIdx = 0;
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        auto& vehicleLocs = modRequest.vehicleLocs[i];
        locs[i] = vehicleLocs.location;
        supplyIdToIdx[vehicleLocs.supplyIdx] = i;
    }
    baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        locs[baseIdx + i] = onboardDemand.destinationLoc;
        pushStationToIdx(stationToIdx, onboardDemand.destinationLoc.station_id, onboardDemand.destinationLoc.direction, baseIdx + i);
        demandIdToIdx[onboardDemand.id] = baseIdx + i;
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        locs[baseIdx + i * 2] = waitingDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = waitingDemand.destinationLoc;
        pushStationToIdx(stationToIdx, waitingDemand.startLoc.station_id, waitingDemand.startLoc.direction, baseIdx + i * 2);
        pushStationToIdx(stationToIdx, waitingDemand.destinationLoc.station_id, waitingDemand.destinationLoc.direction, baseIdx + i * 2 + 1);
        demandIdToIdx[waitingDemand.id] = baseIdx + i * 2;
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        auto& newDemand = modRequest.newDemands[i];
        locs[baseIdx + i * 2] = newDemand.startLoc;
        locs[baseIdx + i * 2 + 1] = newDemand.destinationLoc;
        pushStationToIdx(stationToIdx, newDemand.startLoc.station_id, newDemand.startLoc.direction, baseIdx + i * 2);
        pushStationToIdx(stationToIdx, newDemand.destinationLoc.station_id, newDemand.destinationLoc.direction, baseIdx + i * 2 + 1);
        demandIdToIdx[newDemand.id] = baseIdx + i * 2;
    }

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
    std::string reqDateTime = getReqDateTime(modRequest.dateTime);

    std::deque<std::shared_ptr<CTaskValhalla>> tasks;
    functionValhallaCostFromVehicle(modRequest, locs, nodeCount, stationToIdx, demandIdToIdx, supplyIdToIdx, reqDateTime, tasks);
    functionValhallaCostToNewChanged(modRequest, locs, nodeCount, stationToIdx, changed, reqDateTime, tasks);
    functionValhallaCostFromNewChanged(modRequest, locs, nodeCount, stationToIdx, changed, notChanged, reqDateTime, tasks);

    queryCostValhallaTask(routePath, routeTasks, baseVehicle, nodeCount, tasks, distMatrix, timeMatrix, showLog);

    updateFromVehicleCostMatrixWithStationCache(modRequest, nodeCount, stationToIdx, distMatrix, timeMatrix);
    updateChangedCostMatrixWithStationCache(modRequest, nodeCount, changed, notChanged, stationToIdx, distMatrix, timeMatrix);

    return 0;
}

int queryCostValhallaReset()
{
    g_costCache.clear();
    return 0;
}

#ifdef CHECK_VALHALLA_COST_CACHE
void testCostValhallaCache(
    const ModRequest& modRequest,
    const std::string& routePath,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    std::vector<int64_t> checkDistMatrix(distMatrix.size());
    std::vector<int64_t> checkTimeMatrix(timeMatrix.size());
    queryCostValhallaAll(modRequest, routePath, 4, nodeCount, checkDistMatrix, checkTimeMatrix, true);
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

int queryCostValhalla(
    const ModRequest& modRequest,
    const std::string& routePath,
    const int nRouteTasks,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    // 어떤 경우이든 caching 기능을 사용하도록 수정
    // if (nodeCount <= 50) {
    //     return queryCostValhallaAll(modRequest, routePath, nodeCount, distMatrix, timeMatrix, showLog);
    // }

    std::vector<int> changed;
    if (g_costCache.checkChangedItem(modRequest, changed)) {
        queryCostValhallaNotInCache(modRequest, routePath, nRouteTasks, nodeCount, changed, distMatrix, timeMatrix, showLog);
    }
    g_costCache.updateCacheAndCost(modRequest, nodeCount, changed, distMatrix, timeMatrix);

#ifdef CHECK_VALHALLA_COST_CACHE
    testCostValhallaCache(modRequest, routePath, nodeCount, distMatrix, timeMatrix);
#endif

    return 0;
}
