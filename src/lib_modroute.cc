#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include "lib_modroute.h"
#include "costCache.h"
#include "requestLogger.h"


extern CCostCache g_costCache;

std::vector<ModDispatchSolution> run_optimize(
    ModRequest& mod_request,
    std::string& route_path,
    RouteType route_type,
    int route_tasks,
    std::string& cache_path,
    struct AlgorithmParameters* ap,
    struct ModRouteConfiguration& conf)
{
    if (!cache_path.empty()) {
        g_costCache.loadStationCache(cache_path);
    }
    if (conf.bLogRequest) {
        prepareLogPath();
    }
    size_t vehicleCount = mod_request.vehicleLocs.size();
    std::unordered_map<int, ModRoute> mapNodeToModRoute = makeNodeToModRoute(mod_request, vehicleCount);
    std::vector<Solution *> solutions = runOptimize(mod_request, route_path, route_type, route_tasks,  mapNodeToModRoute, ap, conf, false);
    std::vector<ModDispatchSolution> dispatch_solutions;
    for (int i = 0; i < solutions.size(); i++) {
        auto solution = solutions[i];
        ModDispatchSolution dispatch_solution;
        for (int j = 0; j < solution->n_routes; j++) {
            ModVehicleRoute vehicle_route;
            auto& route = solution->routes[j];
            auto& vehicle = mod_request.vehicleLocs[solution->vehicles[j]];
            vehicle_route.supply_idx = vehicle.supplyIdx;
            for (int k = 0; k < route.length; k++) {
                auto& node = route.path[k];
                if (node <= 0) {
                    continue;
                } else if (node <= vehicleCount) {
                    if (solution->vehicles[j] + 1 != node) {
                        throw std::runtime_error("Invalid vehicle assignment");
                    }
                } else {
                    ModRoute route = mapNodeToModRoute[solution->routes[j].path[k]];
                    vehicle_route.routes.push_back(route);
                }
            }
            dispatch_solution.vehicle_routes.push_back(vehicle_route);
        }
        for (int j = 0; j < solution->n_missing; j++) {
            ModRoute route = mapNodeToModRoute[solution->missing[j]];
            dispatch_solution.missing.push_back(route);
        }
        for (int j = 0; j < solution->n_unacceptables; j++) {
            ModRoute route = mapNodeToModRoute[solution->unacceptables[j]];
            dispatch_solution.unacceptables.push_back(route);
        }
        dispatch_solution.total_distance = solution->distance;
        dispatch_solution.total_time = solution->time;
        dispatch_solutions.push_back(dispatch_solution);
        delete_solution(solution);
    }
    return dispatch_solutions;  
}

void clear_cache() {
    g_costCache.clear();
    g_costCache.clearStationCache();
}
