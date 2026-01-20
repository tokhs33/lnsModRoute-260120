#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <utility>
#include <chrono>
#include <future>
#include <ctime>
#include <cassert>
#include <gason/gason.h>
#include <cpp-httplib/httplib.h>
#include <lnsModRoute.h>
#include <queryValhallaCost.h>
#include <costCache.h>
#include "test_utility.h"

#define VALHALLA_MAX_LOCATIONS 50

extern CCostCache g_costCache;

std::string makeValhallaRequestBody(std::vector<Location>& locs, size_t source, size_t target)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8) << "{\"sources\":[";
    size_t sourceCount = std::min(locs.size(), source + VALHALLA_MAX_LOCATIONS);
    for (size_t i = source; i < sourceCount; i++) {
        oss << (i == source ? "" : ",") << "{\"lat\":" << locs[i].lat << ",\"lon\":" << locs[i].lng;
        if (locs[i].direction != -1) oss << ",\"heading\":" << locs[i].direction;
        oss << "}";
    }
    oss << "],\"targets\":[";
    size_t targetCount = std::min(locs.size(), target + VALHALLA_MAX_LOCATIONS);
    for (int i = target; i < targetCount; i++) {
        oss << (i == target ? "" : ",") << "{\"lat\":" << locs[i].lat << ",\"lon\":" << locs[i].lng;
        if (locs[i].direction != -1) oss << ",\"heading\":" << locs[i].direction;
        oss << "}";
    }
    oss << "],\"costing\":\"auto\"}";
    return oss.str();
}

std::string makeValhallaIndexLocs(const char *name, const std::vector<Location>& locs, const std::vector<int>& selected, int from, int to)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8) << "\"" << name << "\":[";
    if (selected.size() != 0 && from < to) {
        oss << "{\"lat\":" << locs[selected[from]].lat << ",\"lon\":" << locs[selected[from]].lng;
        if (locs[selected[from]].direction != -1) oss << ",\"heading\":" << locs[selected[from]].direction;
        oss << "}";
        for (int i = from + 1; i < to; i++) {
            oss << ",{\"lat\":" << locs[selected[i]].lat << ",\"lon\":" << locs[selected[i]].lng;
            if (locs[selected[i]].direction != -1) oss << ",\"heading\":" << locs[selected[i]].direction;
            oss << "}";
        }
    }
    oss << "]";
    return oss.str();
}

int queryCostValhallaAll_1(
    const ModRequest& modRequest,
    const std::string& routePath,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    auto start = std::chrono::high_resolution_clock::now();

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

    std::string url = "/sources_to_targets";

    httplib::Client cli(routePath.c_str());
    for (size_t s = 0; s < nodeCount; s += VALHALLA_MAX_LOCATIONS) {
        for (size_t d = 0; d < nodeCount; d += VALHALLA_MAX_LOCATIONS) {
            auto res = cli.Post(url.c_str(), makeValhallaRequestBody(locs, s, d).c_str(), "application/json");
            if (!res) {
                auto err = res.error();
                throw std::runtime_error("Fail to connect to Valhalla");
            } else if(res->status != 200) {
                throw std::runtime_error("Fail to get cost from Valhalla");
            }
            char *endptr;
            JsonValue value;
            JsonAllocator allocator;
            int status = jsonParse((char*) res->body.c_str(), &endptr, &value, allocator);
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
                                            distance_value = static_cast<int64_t>(std::ceil(cost->value.toNumber() * 1000.0));
                                        } else if (strcmp(cost->key, "time") == 0) {
                                            time_value = static_cast<int64_t>(std::ceil(cost->value.toNumber()));
                                        } else if (strcmp(cost->key, "to_index") == 0) {
                                            to_index = (int) cost->value.toNumber();
                                        } else if (strcmp(cost->key, "from_index") == 0) {
                                            from_index = (int) cost->value.toNumber();
                                        }
                                    }
                                    if (to_index != -1 && from_index != -1) {
                                        int idx = (s + 1 + from_index) * (nodeCount + 1) + (d + 1 + to_index);
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
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << logNow() << " costValhallaAll : " << duration << " ms" << std::endl;

    return 0;
}

int queryCostValhallaIndex(
    const ModRequest& modRequest,
    httplib::Client& httpClient,
    size_t nodeCount,
    const std::vector<Location> locs,
    const std::vector<int>& sources,
    const size_t sourceCount,
    const std::vector<int>& destinations,
    const size_t destinationCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    if (sourceCount == 0 || destinationCount == 0) {
        return 0;
    }

    size_t baseVehicle = 1;
    std::string url = "/sources_to_targets";

    for (size_t s = 0; s < sourceCount; s += 50) {
        std::string q_source = makeValhallaIndexLocs("sources", locs, sources, s, std::min(s + 50, sourceCount));
        for (size_t d = 0; d < destinationCount; d += 50) {
            std::string q_dest = makeValhallaIndexLocs("targets", locs, destinations, d, std::min(d + 50, destinationCount));
            std::string q_body = "{" + q_source + "," + q_dest + ",\"costing\":\"auto\"}";
            auto res = httpClient.Post(url.c_str(), q_body.c_str(), "application/json");
            if (!res) {
                auto err = res.error();
                throw std::runtime_error("Fail to connect to Valhalla");
            } else if(res->status != 200) {
                throw std::runtime_error("Fail to get cost from Valhalla");
            }
            char *endptr;
            JsonValue value;
            JsonAllocator allocator;
            int status = jsonParse((char*) res->body.c_str(), &endptr, &value, allocator);
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
                                            distance_value = static_cast<int64_t>(std::ceil(cost->value.toNumber() * 1000.0));
                                        } else if (strcmp(cost->key, "time") == 0) {
                                            time_value = static_cast<int64_t>(std::ceil(cost->value.toNumber()));
                                        } else if (strcmp(cost->key, "to_index") == 0) {
                                            to_index = (int) cost->value.toNumber();
                                        } else if (strcmp(cost->key, "from_index") == 0) {
                                            from_index = (int) cost->value.toNumber();
                                        }
                                    }
                                    if (to_index != -1 && from_index != -1) {
                                        int idx = (sources[s + from_index] + baseVehicle) * (nodeCount + 1) + (destinations[d + to_index] + baseVehicle);
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
    }

    return 0;
}

void functionValhallaCostFromVehicle(
    const ModRequest& modRequest,
    const std::string& routePath,
    const std::vector<Location> locs,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);
    httplib::Client cli(routePath.c_str());

    // vehicle -> start_loc 조회
    for (size_t i = 0; i < modRequest.vehicleLocs.size(); i++) {
        sources[i] = i;
    }
    size_t destCount = 0;
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
    queryCostValhallaIndex(modRequest, cli, nodeCount, locs, sources, modRequest.vehicleLocs.size(), destinations, destCount, distMatrix, timeMatrix);

    if (showLog) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << logNow() << " costFromVehicle : " << duration << " ms" << std::endl;
    }
}

void functionValhallaCostToNewChanged(
    const ModRequest& modRequest,
    const std::string& routePath,
    const std::vector<Location> locs,
    size_t nodeCount,
    const std::vector<int>& changed,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    if (changed.empty()) {
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<int> sources(nodeCount);
    std::vector<int> destinations(nodeCount);
    httplib::Client cli(routePath.c_str());

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
    queryCostValhallaIndex(modRequest, cli, nodeCount, locs, sources, sourceCount, destinations, destCount, distMatrix, timeMatrix);

    if (showLog) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << logNow() << " costToNewChange : " << duration << " ms" << std::endl;
    }
}

void functionValhallaCostFromNewChanged(
    const ModRequest& modRequest,
    const std::string& routePath,
    const std::vector<Location> locs,
    size_t nodeCount,
    const std::vector<int>& changed,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    if (changed.empty()) {
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

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
    httplib::Client cli(routePath.c_str());

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
    queryCostValhallaIndex(modRequest, cli, nodeCount, locs, sources, sourceCount, destinations, destCount, distMatrix, timeMatrix);

    if (showLog) {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << logNow() << " costFromNewChng : " << duration << " ms" << std::endl;
    }
}

int queryCostValhallaNotInCache(
    const ModRequest& modRequest,
    const std::string& routePath,
    size_t nodeCount,
    const std::vector<int>& changed,
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

    std::future<void> future1 = std::async(std::launch::async,
        functionValhallaCostFromVehicle,
        std::cref(modRequest), std::cref(routePath), std::cref(locs), nodeCount, std::ref(distMatrix), std::ref(timeMatrix), showLog);
    std::future<void> future2 = std::async(std::launch::async,
        functionValhallaCostToNewChanged,
        std::cref(modRequest), std::cref(routePath), std::cref(locs), nodeCount, std::cref(changed), std::ref(distMatrix), std::ref(timeMatrix), showLog);
    std::future<void> future3 = std::async(std::launch::async,
        functionValhallaCostFromNewChanged,
        std::cref(modRequest), std::cref(routePath), std::cref(locs), nodeCount, std::cref(changed), std::ref(distMatrix), std::ref(timeMatrix), showLog);

    future1.get();
    future2.get();
    future3.get();

    return 0;
}

int queryCostValhalla_original(
    const ModRequest& modRequest,
    const std::string& routePath,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    // 어떤 경우이든 caching 기능을 사용하도록 수정
    // if (nodeCount <= 50) {
    //     return queryCostValhallaAll(modRequest, routePath, nodeCount, distMatrix, timeMatrix);
    // }

    std::vector<int> changed;
    if (g_costCache.checkChangedItem(modRequest, changed)) {
        queryCostValhallaNotInCache(modRequest, routePath, nodeCount, changed, distMatrix, timeMatrix, showLog);
    }

#ifdef CHECK_VALHALLA_COST_CACHE
    testCostValhallaCache(modRequest, routePath, nodeCount, distMatrix, timeMatrix);
#endif

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

        size_t nodeCount = modRequest.vehicleLocs.size() + modRequest.onboardDemands.size()
            + 2 * (modRequest.onboardWaitingDemands.size() + modRequest.newDemands.size());

        std::cout << "test nodeCount : " << nodeCount << std::endl;

        std::vector<int64_t> distMatrix1((nodeCount + 1) * (nodeCount + 1));
        std::vector<int64_t> timeMatrix1((nodeCount + 1) * (nodeCount + 1));

        std::vector<int64_t> distMatrix2((nodeCount + 1) * (nodeCount + 1));
        std::vector<int64_t> timeMatrix2((nodeCount + 1) * (nodeCount + 1));

        queryCostValhalla_original(modRequest, routePath, nodeCount, distMatrix1, timeMatrix1, true);
        queryCostValhalla(modRequest, routePath, 4, nodeCount, distMatrix2, timeMatrix2, true);

        assert(distMatrix1 == distMatrix2);
        assert(timeMatrix1 == timeMatrix2);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}