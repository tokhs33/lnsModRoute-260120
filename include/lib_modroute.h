#ifndef _INC_LIB_MODROUTE_HDR
#define _INC_LIB_MODROUTE_HDR

#include <vector>
#include <string>
#include <cstdint>
#include "mod_parameters.h"
#include "lnsModRoute.h"

struct ModVehicleRoute {
    std::string supply_idx;
    std::vector<struct ModRoute> routes;
};

struct ModDispatchSolution {
    std::vector<ModVehicleRoute> vehicle_routes;
    std::vector<ModRoute> missing;
    std::vector<ModRoute> unacceptables;
    int64_t total_distance;
    int64_t total_time;
};

std::vector<ModDispatchSolution> run_optimize(
    ModRequest& mod_request,
    std::string& route_path,
    RouteType route_type,
    int route_tasks,
    std::string& cache_path,
    struct AlgorithmParameters* ap,
    struct ModRouteConfiguration& conf);

void clear_cache();

#endif // _INC_LIB_MODROUTE_HDR
