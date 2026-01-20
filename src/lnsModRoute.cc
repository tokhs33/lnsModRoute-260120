#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cmath>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <lnspdptw.h>
#include <lnsModRoute.h>
#include <modState.h>
#include <queryOsrmCost.h>
#include <queryValhallaCost.h>
#include <costCache.h>
#include <requestLogger.h>

std::string logNow()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm_local;
#ifdef _WIN32
    localtime_s(&now_tm_local, &now_c);
#else
    localtime_r(&now_c, &now_tm_local);
#endif
    std::ostringstream oss;
    oss << "[" << std::put_time(&now_tm_local, "%Y-%m-%d %H:%M:%S") << "]";
    return oss.str();
}

// void pushDemandToStation(std::unordered_map<int, int>& demandToStation, int demandId, int stationId, int idx)
// {
//     demandToStation[demandId] = stationId < 0 ? -idx : stationId;
// }

void applyVehicleAssignedTimeDistance(
    const ModRequest& modRequest,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix)
{
    if (modRequest.assigned.empty()) {
        return;
    } else if (modRequest.assigned[0].routeTimes.empty()) {
        // assigned 가 있지만 routeTimes 가 비어있는 경우 바꿔야 할 것이 없으므로 그냥 리턴
        return;
    }
    std::unordered_map<std::string, int> supplyIdToIdx;
    std::unordered_map<std::string, int> demandIdToIdx;
    size_t baseIdx = 0;
    for (int i = 0; i < modRequest.vehicleLocs.size(); i++) {
        supplyIdToIdx[modRequest.vehicleLocs[i].supplyIdx] = i;
    }
    baseIdx = modRequest.vehicleLocs.size();
    for (int i = 0; i < modRequest.onboardDemands.size(); i++) {
        // 여기에서 minus 1을 하는 이유는,
        // 이후에 routeOrder 에서 pickup, drop off 를 구분할 때
        // drop off 의 경우 ++ 해서 toIdx를 증가시키기 때문
        // routeOrder 에서는 해당 demand가 onboard인지 waiting인지 구분할 수 없기 때문임
        demandIdToIdx[modRequest.onboardDemands[i].id] = baseIdx + i - 1;
    }
    baseIdx += modRequest.onboardDemands.size();
    for (int i = 0; i < modRequest.onboardWaitingDemands.size(); i++) {
        demandIdToIdx[modRequest.onboardWaitingDemands[i].id] = baseIdx + i * 2;
    }
    baseIdx += 2 * modRequest.onboardWaitingDemands.size();
    for (int i = 0; i < modRequest.newDemands.size(); i++) {
        demandIdToIdx[modRequest.newDemands[i].id] = baseIdx + i * 2;
    }
    size_t baseVehicle = 1; // 0 = ghost depot 
    for (const auto& vehicleAssigned : modRequest.assigned) {
        if (vehicleAssigned.routeOrder.empty()) {
            continue;
        }
        if (vehicleAssigned.routeOrder.size() != vehicleAssigned.routeTimes.size() || 
            vehicleAssigned.routeOrder.size() != vehicleAssigned.routeDistances.size()) {
            continue; // routeOrder, routeTimes, routeDistances 크기가 일치하지 않는 경우
        }
        std::string supplyIdx = vehicleAssigned.supplyIdx;
        auto supplyIt = supplyIdToIdx.find(supplyIdx);
        if (supplyIt == supplyIdToIdx.end()) {
            continue; // 해당 공급 차량이 없는 경우
        }
        int fromIdx = supplyIt->second + baseVehicle;
        for (int i = 0; i < vehicleAssigned.routeOrder.size(); i++) {
            auto routeOrder = vehicleAssigned.routeOrder[i];
            auto demandIt = demandIdToIdx.find(routeOrder.first);
            if (demandIt == demandIdToIdx.end()) {
                break; // 해당 demand가 없는 경우 이후 loop 중단
            }
            int toIdx = demandIt->second + baseVehicle;
            if (routeOrder.second < 0) {
                // drop off 인 경우이기 때문에 toIdx 증가
                toIdx++; 
            }
            int cacheIdx = fromIdx * (nodeCount + 1) + toIdx;
            if (vehicleAssigned.routeTimes[i] >= 0) {
                timeMatrix[cacheIdx] = vehicleAssigned.routeTimes[i];
            }
            if (vehicleAssigned.routeDistances[i] >= 0) {
                distMatrix[cacheIdx] = vehicleAssigned.routeDistances[i];
            }
            fromIdx = toIdx;
        }
    }
}

int queryCostMatrix(
    const ModRequest& modRequest,
    const std::string& routePath,
    const RouteType eRouteType,
    const int nRouteTasks,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog)
{
    if (eRouteType == ROUTE_OSRM) {
        queryCostOsrm(modRequest, routePath, nRouteTasks, nodeCount, distMatrix, timeMatrix, showLog);
    } else if (eRouteType == ROUTE_VALHALLA) {
        queryCostValhalla(modRequest, routePath, nRouteTasks, nodeCount, distMatrix, timeMatrix, showLog);
    }
    applyVehicleAssignedTimeDistance(modRequest, nodeCount, distMatrix, timeMatrix);

    return 0;
}

std::vector<int64_t> calcCost(std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix, OptimizeType optimizeType)
{
    if (optimizeType == OptimizeType::Time) {
        return timeMatrix;
    } else if (optimizeType == OptimizeType::Distance) {
        return distMatrix;
    } else if (optimizeType == OptimizeType::Co2) {
        // 2021년 승인 국가 온실가스 배출.흡수계수 적용하는 것으로 변경
        std::vector<int64_t> co2Matrix(distMatrix.size());
        for (size_t i = 0; i < distMatrix.size(); i++) {
            if (timeMatrix[i] == 0) {
                co2Matrix[i] = 0;
                continue;
            } else if (timeMatrix[i] == INT32_MAX) {
                co2Matrix[i] = INT32_MAX;
                continue;
            }
            double velocity = 3.6 * distMatrix[i] / timeMatrix[i];  // km/h
            double co2_rate = velocity < 64.7 ? (4317.2386 * std::pow(velocity, -0.5049)) : (0.1829 * std::pow(velocity, 2) - 29.8145 * velocity + 1670.8962);
            co2Matrix[i] = (int64_t)(co2_rate * distMatrix[i]); // mg/km
        }
        return co2Matrix;
    } else {
        return distMatrix;
    }
}

int64_t calc_newdemand_dest_eta(int64_t routeTime, const std::array<int, 2>& eta_to_start, const struct ModRouteConfiguration& modRouteConfig, int nMaxDuration)
{
    if (eta_to_start[1] > 0) {
        // eta_to_start[1] 이 positive인 경우는 pickup 시간을 명시적으로 지정한 경우임
        // 이 경우에 destination 시간은 routeTime에 bypass ratio 을 적용한 
        // minus 값을 리턴하여 dependent node로 지정
        int64_t dependent_route_time = routeTime * (modRouteConfig.nBypassRatio + 100) / 100;
        return -dependent_route_time;
    } else if (eta_to_start[1] < 0) {
        // eta_to_start[1] 이 minus인 경우에는 destination timewindow에 routeTime - eta_to_start[1] 설정
        return routeTime - eta_to_start[1];
    } else {
        return nMaxDuration;
    }
}

int64_t calc_newdemand_dest_acptbl(int64_t newdemand_eta, const struct ModRouteConfiguration& modRouteConfig, int nMaxDuration)
{
    if (newdemand_eta < 0) {
        return std::min<int64_t>(-1, newdemand_eta + modRouteConfig.nAcceptableBuffer);
    } else if (newdemand_eta == nMaxDuration) {
        return nMaxDuration;
    } else {
        return std::max<int64_t>(1, newdemand_eta - modRouteConfig.nAcceptableBuffer);
    }
}

std::unordered_map<int, ModRoute> makeNodeToModRoute(const ModRequest& modRequest, const size_t vehicleCount)
{
    std::unordered_map<int, ModRoute> mapNodeToModRoute;
    int baseNodeIdx = vehicleCount + 1;
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++, baseNodeIdx++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        mapNodeToModRoute[baseNodeIdx] = {};
        mapNodeToModRoute[baseNodeIdx].id = onboardDemand.id;
        mapNodeToModRoute[baseNodeIdx].demand = -onboardDemand.demand;
        mapNodeToModRoute[baseNodeIdx].location = onboardDemand.destinationLoc;
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++, baseNodeIdx += 2) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        mapNodeToModRoute[baseNodeIdx] = {};
        mapNodeToModRoute[baseNodeIdx].id = waitingDemand.id;
        mapNodeToModRoute[baseNodeIdx].demand = waitingDemand.demand;
        mapNodeToModRoute[baseNodeIdx].location = waitingDemand.startLoc;
        mapNodeToModRoute[baseNodeIdx + 1] = {};
        mapNodeToModRoute[baseNodeIdx + 1].id = waitingDemand.id;
        mapNodeToModRoute[baseNodeIdx + 1].demand = -waitingDemand.demand;
        mapNodeToModRoute[baseNodeIdx + 1].location = waitingDemand.destinationLoc;
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++, baseNodeIdx += 2) {
        auto& newDemand = modRequest.newDemands[i];
        mapNodeToModRoute[baseNodeIdx] = {};
        mapNodeToModRoute[baseNodeIdx].id = newDemand.id;
        mapNodeToModRoute[baseNodeIdx].demand = newDemand.demand;
        mapNodeToModRoute[baseNodeIdx].location = newDemand.startLoc;
        mapNodeToModRoute[baseNodeIdx + 1] = {};
        mapNodeToModRoute[baseNodeIdx + 1].id = newDemand.id;
        mapNodeToModRoute[baseNodeIdx + 1].demand = -newDemand.demand;
        mapNodeToModRoute[baseNodeIdx + 1].location = newDemand.destinationLoc;
    }
    return mapNodeToModRoute;
}

CModState loadToModState(
    const ModRequest& modRequest,
    const size_t nodeCount,
    const size_t vehicleCount,
    const std::vector<int64_t>& timeMatrix,
    const struct ModRouteConfiguration& modRouteConfig)
{
    CModState modState(nodeCount, vehicleCount);
    std::unordered_map<std::string, int> mapVehicleSupplyIdx;
    std::unordered_map<std::string, std::pair<int, int> > mapRouteToNode;
    int nMaxDuration = modRequest.maxDuration == 0 ? modRouteConfig.nMaxDuration : modRequest.maxDuration;
    int nServiceTime = modRouteConfig.nServiceTime;
    int nAcceptableBuffer = modRouteConfig.nAcceptableBuffer;

    std::fill(modState.latestArrival.begin(), modState.latestArrival.begin() + vehicleCount + 1, nMaxDuration);
    std::fill(modState.acceptableArrival.begin(), modState.acceptableArrival.begin() + vehicleCount + 1, nMaxDuration);

    for (size_t i = 0; i < vehicleCount; i++) {
        modState.vehicleCapacities[i] = modRequest.vehicleLocs[i].capacity;
        modState.startDepots[i] = i + 1;
        // modState.endDepots[i] = 0;    // ghost depot
        mapVehicleSupplyIdx[modRequest.vehicleLocs[i].supplyIdx] = i + 1;   // fixed assignment feature (1-based index)
        if (modRequest.vehicleLocs[i].operationTime[0] > 0) {
            modState.earliestArrival[i + 1] = modRequest.vehicleLocs[i].operationTime[0];
        }
        if (modRequest.vehicleLocs[i].operationTime[1] > 0) {
            modState.latestArrival[i + 1] = modRequest.vehicleLocs[i].operationTime[1];
            modState.acceptableArrival[i + 1] = std::max<int64_t>(1, modRequest.vehicleLocs[i].operationTime[1] - nAcceptableBuffer);
        }
    }
    int baseNodeIdx = vehicleCount + 1;
    for (size_t i = 0; i < modRequest.onboardDemands.size(); i++, baseNodeIdx++) {
        auto& onboardDemand = modRequest.onboardDemands[i];
        modState.demands[baseNodeIdx] = -onboardDemand.demand;  // drop off
        modState.serviceTimes[baseNodeIdx] = nServiceTime;
        modState.earliestArrival[baseNodeIdx] = onboardDemand.etaToDestination[0];
        modState.latestArrival[baseNodeIdx] = onboardDemand.etaToDestination[1] > 0 ? onboardDemand.etaToDestination[1] : nMaxDuration;
        modState.acceptableArrival[baseNodeIdx] = onboardDemand.etaToDestination[1] > 0 ? std::max<int64_t>(1, onboardDemand.etaToDestination[1] - nAcceptableBuffer) : nMaxDuration;
        modState.fixedAssignment[baseNodeIdx] = mapVehicleSupplyIdx[onboardDemand.supplyIdx];
        mapRouteToNode[onboardDemand.id] = {};
        mapRouteToNode[onboardDemand.id].second = baseNodeIdx;
    }
    for (size_t i = 0; i < modRequest.onboardWaitingDemands.size(); i++, baseNodeIdx += 2) {
        auto& waitingDemand = modRequest.onboardWaitingDemands[i];
        modState.demands[baseNodeIdx] = waitingDemand.demand;  // pickup
        modState.demands[baseNodeIdx + 1] = -waitingDemand.demand;    // drop off
        modState.serviceTimes[baseNodeIdx] = nServiceTime;
        modState.serviceTimes[baseNodeIdx + 1] = nServiceTime;
        modState.earliestArrival[baseNodeIdx] = waitingDemand.etaToStart[0];
        modState.latestArrival[baseNodeIdx] = waitingDemand.etaToStart[1] > 0 ? waitingDemand.etaToStart[1] : nMaxDuration;
        modState.acceptableArrival[baseNodeIdx] = waitingDemand.etaToStart[1] > 0 ? std::max<int64_t>(1, waitingDemand.etaToStart[1] - nAcceptableBuffer) : nMaxDuration;
        modState.earliestArrival[baseNodeIdx + 1] = waitingDemand.etaToDestination[0];
        modState.latestArrival[baseNodeIdx + 1] = waitingDemand.etaToDestination[1] != 0 ? waitingDemand.etaToDestination[1] : nMaxDuration;
        // modState.acceptableArrival[baseNodeIdx + 1] = waitingDemand.etaToDestination[1] > 0 ? std::max<int64_t>(1, waitingDemand.etaToDestination[1] - nAcceptableBuffer) : nMaxDuration;
        modState.acceptableArrival[baseNodeIdx + 1] = calc_newdemand_dest_acptbl(modState.latestArrival[baseNodeIdx + 1], modRouteConfig, nMaxDuration);
        modState.deliverySibling[baseNodeIdx] = baseNodeIdx + 1;
        modState.pickupSibling[baseNodeIdx + 1] = baseNodeIdx;
        modState.fixedAssignment[baseNodeIdx] = mapVehicleSupplyIdx[waitingDemand.supplyIdx];
        modState.fixedAssignment[baseNodeIdx + 1] = modState.fixedAssignment[baseNodeIdx];
        mapRouteToNode[waitingDemand.id] = {};
        mapRouteToNode[waitingDemand.id].first = baseNodeIdx;
        mapRouteToNode[waitingDemand.id].second = baseNodeIdx + 1;
    }
    for (size_t i = 0; i < modRequest.newDemands.size(); i++, baseNodeIdx += 2) {
        auto& newDemand = modRequest.newDemands[i];
        modState.demands[baseNodeIdx] = newDemand.demand;  // pickup
        modState.demands[baseNodeIdx + 1] = -newDemand.demand;    // drop off
        modState.serviceTimes[baseNodeIdx] = nServiceTime;
        modState.serviceTimes[baseNodeIdx + 1] = nServiceTime;
        modState.earliestArrival[baseNodeIdx] = newDemand.etaToStart[0];
        modState.latestArrival[baseNodeIdx] = newDemand.etaToStart[1] > 0 ? newDemand.etaToStart[1] : nMaxDuration;
        modState.acceptableArrival[baseNodeIdx] = newDemand.etaToStart[1] > 0 ? newDemand.etaToStart[1] : nMaxDuration;
        modState.earliestArrival[baseNodeIdx + 1] = newDemand.etaToStart[0];
        modState.latestArrival[baseNodeIdx + 1] = calc_newdemand_dest_eta(timeMatrix[baseNodeIdx * (nodeCount + 1) + baseNodeIdx + 1], newDemand.etaToStart, modRouteConfig, nMaxDuration);
        modState.acceptableArrival[baseNodeIdx + 1] = calc_newdemand_dest_acptbl(modState.latestArrival[baseNodeIdx + 1], modRouteConfig, nMaxDuration);
        modState.deliverySibling[baseNodeIdx] = baseNodeIdx + 1;
        modState.pickupSibling[baseNodeIdx + 1] = baseNodeIdx;
        mapRouteToNode[newDemand.id] = {};
        mapRouteToNode[newDemand.id].first = baseNodeIdx;
        mapRouteToNode[newDemand.id].second = baseNodeIdx + 1;
    }
    for (size_t i = 0; i < modRequest.assigned.size(); i++) {
        auto& assigned = modRequest.assigned[i];
        if (assigned.routeOrder.size() == 0) {
            continue;
        }
        int startIdx = mapVehicleSupplyIdx[assigned.supplyIdx];
        modState.initialSolution.push_back(startIdx);
        for (auto& routeOrder : assigned.routeOrder) {
            std::string pass_id = routeOrder.first;
            int demand = routeOrder.second;
            int node = demand > 0 ? mapRouteToNode[pass_id].first : mapRouteToNode[pass_id].second;
            if (node == 0) {
                // 잘못 입력한 경우 무시
                continue;
            }
            modState.initialSolution.push_back(node);
        }
        modState.initialSolution.push_back(0);  // end idx
    }
    return modState;
}

bool reflectAssignedForNewDemandRoute(CModState& modState, size_t vehicleCount, size_t baseNewDemand, const Solution* solution)
{
    if (solution->n_missing > 0) {
        return false;
    }

    bool bChanged = false;
    for (int i = 0; i < solution->n_routes; i++) {
        auto& route = solution->routes[i];
        auto assigned = solution->vehicles[i] + 1;
        for (int j = 0; j < route.length; j++) {
            auto& node = route.path[j];
            if (node < baseNewDemand) {
                // node is not new demand
                continue;
            }
            int fixedAssignment = modState.fixedAssignment[node];
            if (fixedAssignment > 0) {
                // prerequisites assignment (should not be changed)
                continue;
            }

            modState.fixedAssignment[node] = fixedAssignment * (vehicleCount + 1) - assigned;
            bChanged = true;
        }
    }
    return bChanged;
}

std::vector<Solution *> runOptimize(
    ModRequest& modRequest,
    std::string& sRoutePath,
    RouteType eRouteType,
    int nRouteTasks,
    std::unordered_map<int, ModRoute> mapNodeToModRoute,
    const struct AlgorithmParameters* ap,
    const struct ModRouteConfiguration& conf,
    bool showLog)
{
    size_t vehicleCount = modRequest.vehicleLocs.size();
    size_t nodeCount = modRequest.vehicleLocs.size() + modRequest.onboardDemands.size()
        + 2 * (modRequest.onboardWaitingDemands.size() + modRequest.newDemands.size());

    std::vector<int64_t> distMatrix((nodeCount + 1) * (nodeCount + 1));
    std::vector<int64_t> timeMatrix((nodeCount + 1) * (nodeCount + 1));

    auto start = std::chrono::high_resolution_clock::now();

    queryCostMatrix(modRequest, sRoutePath, eRouteType, nRouteTasks, nodeCount, distMatrix, timeMatrix, showLog);
    auto costMatrix = calcCost(distMatrix, timeMatrix, modRequest.optimizeType);

    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (showLog) {
        std::cout << logNow() << " queryCostMatrix : " << duration << " ms" << std::endl;
    }

    auto modState = loadToModState(modRequest, nodeCount, vehicleCount, timeMatrix, conf);

    if (conf.bLogRequest) {
        logRequest(nodeCount, vehicleCount, costMatrix, distMatrix, timeMatrix, modState);
    }

    start = std::chrono::high_resolution_clock::now();

    auto solution = solve_lns_pdptw(
        nodeCount + 1, costMatrix.data(), distMatrix.data(), timeMatrix.data(),
        modState.demands.data(), modState.serviceTimes.data(), modState.earliestArrival.data(), modState.latestArrival.data(), modState.acceptableArrival.data(),
        modState.pickupSibling.data(), modState.deliverySibling.data(), modState.fixedAssignment.data(),
        (int) vehicleCount, modState.vehicleCapacities.data(), modState.startDepots.data(), modState.endDepots.data(),
        (int) modState.initialSolution.size(), modState.initialSolution.data(), ap
    );

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (showLog) {
        std::cout << logNow() << " solve_lns_pdptw :";
        std::cout << "  demand=" << (nodeCount - vehicleCount);
        std::cout << "  route=" << (solution ? solution->n_routes : -1);
        std::cout << "  duration=" << duration << " ms" << std::endl;
    }

    if (solution == nullptr) {
        throw std::runtime_error("Fail to dispatch");
    }

    std::vector<Solution *> solutions;
    solutions.push_back(solution);

#ifndef NDEBUG
    if (conf.bLogRequest) {
        logResponse(modRequest, modState, nodeCount, vehicleCount, mapNodeToModRoute, distMatrix, timeMatrix, solution);
    }
#endif

    if (modRequest.maxSolutions > 1) {
        size_t baseNewDemand = 1 + vehicleCount + modRequest.onboardDemands.size() + 2 * modRequest.onboardWaitingDemands.size();
        for (int nRemain = std::min(modRequest.maxSolutions - 1, conf.nSolutionLimit); nRemain > 0; nRemain--) {
            if (!reflectAssignedForNewDemandRoute(modState, vehicleCount, baseNewDemand, solution)) {
                break;
            }

            start = std::chrono::high_resolution_clock::now();            

            solution = solve_lns_pdptw(
                nodeCount + 1, costMatrix.data(), distMatrix.data(), timeMatrix.data(),
                modState.demands.data(), modState.serviceTimes.data(), modState.earliestArrival.data(), modState.latestArrival.data(), modState.acceptableArrival.data(),
                modState.pickupSibling.data(), modState.deliverySibling.data(), modState.fixedAssignment.data(),
                (int) vehicleCount, modState.vehicleCapacities.data(), modState.startDepots.data(), modState.endDepots.data(),
                (int) modState.initialSolution.size(), modState.initialSolution.data(), ap
            );

            end = std::chrono::high_resolution_clock::now();
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            if (showLog) {
                std::cout << logNow() << " solve_lns_pdptw[" << nRemain << "]";
                std::cout << " demand=" << (nodeCount - vehicleCount);
                std::cout << "  route=" << (solution ? solution->n_routes : -1);
                std::cout << "  duration=" << duration << " ms" << std::endl;
            }

            if (solution == nullptr) {
                break;
            }

            solutions.push_back(solution);
#ifndef NDEBUG
            if (conf.bLogRequest) {
                logResponse(modRequest, modState, nodeCount, vehicleCount, mapNodeToModRoute, distMatrix, timeMatrix, solution);
            }
#endif
        }
    }

    return solutions;
}

struct ModRouteConfiguration default_mod_configuraiton() 
{
    struct ModRouteConfiguration configuration;
    configuration.nMaxDuration = 7200;
    configuration.nServiceTime = 10;
    configuration.nBypassRatio = 100;
    configuration.nAcceptableBuffer = 10 * 60;
    configuration.bLogRequest = false;
    configuration.nCacheExpirationTime = 3600;
    configuration.nSolutionLimit = 3;
    return configuration;
}

