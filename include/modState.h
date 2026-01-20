#ifndef _INC_MODSTATE_HDR
#define _INC_MODSTATE_HDR

#include <vector>
#include <cstdint>

class CModState {
public:
    CModState(size_t nodeCount, size_t vehicleCount) : 
        demands(nodeCount + 1),
        serviceTimes(nodeCount + 1),
        earliestArrival(nodeCount + 1),
        latestArrival(nodeCount + 1),
        acceptableArrival(nodeCount + 1),
        pickupSibling(nodeCount + 1),
        deliverySibling(nodeCount + 1),
        fixedAssignment(nodeCount + 1),
        vehicleCapacities(vehicleCount),
        startDepots(vehicleCount),
        endDepots(vehicleCount)
    {};

    std::vector<int64_t> demands;
    std::vector<int64_t> serviceTimes;
    std::vector<int64_t> earliestArrival;
    std::vector<int64_t> latestArrival;
    std::vector<int64_t> acceptableArrival;
    std::vector<int> pickupSibling;
    std::vector<int> deliverySibling;
    std::vector<int> fixedAssignment;
    std::vector<int64_t> vehicleCapacities;
    std::vector<int> startDepots;
    std::vector<int> endDepots;
    std::vector<int> initialSolution;
};

#endif // _INC_MODSTATE_HDR