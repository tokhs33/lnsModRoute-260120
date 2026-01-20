#ifndef _INC_QUERY_OSRM_COST_HDR
#define _INC_QUERY_OSRM_COST_HDR

#include <vector>
#include <string>
#include <cstdint>
#include <lnsModRoute.h>

int queryCostOsrmReset();

int queryCostOsrm(
    const ModRequest& modRequest,
    const std::string& routePath,
    const int nRouteTasks,
    size_t nodeCount,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix,
    bool showLog);

#endif // _INC_QUERY_OSRM_COST_HDR