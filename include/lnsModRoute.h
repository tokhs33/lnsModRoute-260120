#ifndef _INC_LNSMODROUTE_HDR
#define _INC_LNSMODROUTE_HDR

#include <vector>
#include <utility>
#include <string>
#include <optional>
#include <unordered_map>
#include <lnspdptw.h>
#include <mod_parameters.h>

struct VehicleAssigned {
    std::string supplyIdx;
    std::vector<std::pair<std::string, int> > routeOrder;
    std::vector<int64_t> routeTimes;
    std::vector<int64_t> routeDistances;

    VehicleAssigned() = default;
    VehicleAssigned(std::string supply_idx) : supplyIdx(supply_idx) {}
};

struct ModRequest {
    std::vector<VehicleLocation> vehicleLocs;
    std::vector<OnboardDemand> onboardDemands;
    std::vector<OnboardWaitingDemand> onboardWaitingDemands;
    std::vector<NewDemand> newDemands;
    std::vector<VehicleAssigned> assigned;
    OptimizeType optimizeType = OptimizeType::Time;
    int maxSolutions = 0;
    std::string locHash;
    std::optional<std::string> dateTime;
    int maxDuration = 0;

    ModRequest() = default;
};

struct ModRoute {
    std::string id;
    int demand;
    Location location;
};

struct ModCacheRequest {
    std::string cacheKey;
};

std::unordered_map<int, ModRoute> makeNodeToModRoute(const ModRequest& modRequest, const size_t vehicleCount);

std::vector<Solution *> runOptimize(
    ModRequest& modRequest,
    std::string& sRoutePath,
    RouteType eRouteType,
    int nRouteTasks,
    std::unordered_map<int, ModRoute> mapNodeToModRoute,
    const struct AlgorithmParameters* ap,
    const struct ModRouteConfiguration& conf,
    bool showLog);

#endif // _INC_LNSMODROUTE_HDR
