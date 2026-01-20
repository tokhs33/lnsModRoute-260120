#ifndef _INC_REQUESTLOGGER_HDR
#define _INC_REQUESTLOGGER_HDR

#include <vector>
#include <unordered_map>
#include <lnspdptw.h>
#include <lnsModRoute.h>
#include <modState.h>

void prepareLogPath();
void logRequest(
    const size_t nodeCount,
    const size_t vehicleCount,
    const std::vector<int64_t>& costMatrix,
    const std::vector<int64_t>& distMatrix,
    const std::vector<int64_t>& timeMatrix,
    const CModState& modState);
void logResponse(
    const ModRequest& modRequest,
    const CModState& modState,
    const size_t nodeCount,
    const size_t vehicleCount,
    std::unordered_map<int, ModRoute>& mapModRoute,
    const std::vector<int64_t>& distMatrix,
    const std::vector<int64_t>& timeMatrix,
    const Solution* solution);

#endif // _INC_REQUESTLOGGER_HDR