#ifndef _INC_MAIN_UTILITY_HDR
#define _INC_MAIN_UTILITY_HDR

#include <string>
#include <unordered_map>
#include <lnsModRoute.h>

ModRequest parseRequest(const char *body);
std::string makeDispatchResponse(
    const ModRequest& modRequest,
    size_t vehicleCount,
    std::unordered_map<int, ModRoute>& mapModRoute,
    const Solution* solution);

#endif // _INC_MAIN_UTILITY_HDR
