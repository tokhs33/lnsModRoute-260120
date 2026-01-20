#ifndef _INC_MODPARAMETERS_HDR
#define _INC_MODPARAMETERS_HDR

#ifdef lib_EXPORTS
#if defined(_MSC_VER)
#define LIBRARY_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define LIBRARY_API __attribute__((visibility("default")))
#else
#define LIBRARY_API
#endif
#else
#if defined(_MSC_VER)
#define LIBRARY_API __declspec(dllimport)
#else
#define LIBRARY_API
#endif
#endif

#include <array>
#include <string>

enum OptimizeType {
    Time = 0,
    Distance,
    Co2,
};

struct Location {
    double lng = 0.0;
    double lat = 0.0;
    int waypoint_id = -1;
    std::string station_id;
    int direction = -1;

    Location() = default;
    Location(double lng, double lat, int direction) : lng(lng), lat(lat), direction(direction) {}
    Location(double lng, double lat, int direction, std::string station_id) : lng(lng), lat(lat), direction(direction), station_id(station_id) {}
};

struct VehicleLocation {
    std::string supplyIdx;
    int capacity{0};
    Location location;
    std::array<int, 2> operationTime{{0, 0}};

    VehicleLocation() = default;
    VehicleLocation(std::string supply_idx, int capacity) : supplyIdx(supply_idx), capacity(capacity) {}
};

struct OnboardDemand {
    std::string id;
    std::string supplyIdx;
    int demand{0};
    Location destinationLoc;
    std::array<int, 2> etaToDestination{{0, 0}};

    OnboardDemand() = default;
    OnboardDemand(std::string id, std::string supply_idx, int demand) : id(id), supplyIdx(supply_idx), demand(demand) {}
};

struct OnboardWaitingDemand {
    std::string id;
    std::string supplyIdx;
    int demand{0};
    Location startLoc;
    Location destinationLoc;
    std::array<int, 2> etaToStart{{0, 0}};
    std::array<int, 2> etaToDestination{{0, 0}};

    OnboardWaitingDemand() = default;
    OnboardWaitingDemand(std::string id, std::string supply_idx, int demand) : id(id), supplyIdx(supply_idx), demand(demand) {}
};

struct NewDemand {
    std::string id;
    int demand{0};
    Location startLoc;
    Location destinationLoc;
    std::array<int, 2> etaToStart{{0, 0}};

    NewDemand() = default;
    NewDemand(std::string id, int demand) : id(id), demand(demand) {}
};

struct ModRouteConfiguration {
    int nMaxDuration;
    int nBypassRatio;
    int nServiceTime;
    int nAcceptableBuffer;   // latest arrival 시간 전에 delaytime penalty 없이 도착할 수 있는 시간
    bool bLogRequest;
    int nCacheExpirationTime;
    int nSolutionLimit;
};

enum RouteType {
    ROUTE_OSRM = 0,
    ROUTE_VALHALLA = 1,
};

#ifdef __cplusplus
extern "C"
#endif
LIBRARY_API struct ModRouteConfiguration default_mod_configuraiton();

#endif // _INC_MODPARAMETERS_HDR