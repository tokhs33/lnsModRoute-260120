#include <cassert>
#include <costCache.h>

class CCostCacheTest {
protected:
    CCostCache cache;
    ModRequest modRequest;

public:
    void SetUp() {
        // Initialize modRequest with test data
        modRequest.vehicleLocs = { {1, 0} };
        modRequest.onboardDemands = { {3, 0, 0}, {4, 0, 0}, {7, 0, 0} };
        modRequest.onboardWaitingDemands = { {5, 0, 0}, {6, 0, 0}, {8, 0, 0} };
        modRequest.newDemands = { {9, 0} };

        // populate cache with test data
        cache.m_mapId = { {2, 0}, {3, 2}, {4, 4}, {5, 6}, {6, 8} };
        cache.m_distCache = {
            {200, 201, 202, 203, 204, 205, 206, 207, 208, 209}, // 2_s
            {210, 211, 212, 213, 214, 215, 216, 217, 218, 219}, // 2_d
            {300, 301, 302, 303, 304, 305, 306, 307, 308, 309}, // 3_s
            {310, 311, 312, 313, 314, 315, 316, 317, 318, 319}, // 3_d
            {400, 401, 402, 403, 404, 405, 406, 407, 408, 409}, // 4_s
            {410, 411, 412, 413, 414, 415, 416, 417, 418, 419}, // 4_d
            {500, 501, 502, 503, 504, 505, 506, 507, 508, 509}, // 5_s
            {510, 511, 512, 513, 514, 515, 516, 517, 518, 519}, // 5_d
            {600, 601, 602, 603, 604, 605, 606, 607, 608, 609}, // 6_s
            {610, 611, 612, 613, 614, 615, 616, 617, 618, 619}, // 6_d
        };
        cache.m_timeCache = {
            {200, 201, 202, 203, 204, 205, 206, 207, 208, 209},
            {210, 211, 212, 213, 214, 215, 216, 217, 218, 219},
            {300, 301, 302, 303, 304, 305, 306, 307, 308, 309},
            {310, 311, 312, 313, 314, 315, 316, 317, 318, 319},
            {400, 401, 402, 403, 404, 405, 406, 407, 408, 409},
            {410, 411, 412, 413, 414, 415, 416, 417, 418, 419},
            {500, 501, 502, 503, 504, 505, 506, 507, 508, 509},
            {510, 511, 512, 513, 514, 515, 516, 517, 518, 519},
            {600, 601, 602, 603, 604, 605, 606, 607, 608, 609},
            {610, 611, 612, 613, 614, 615, 616, 617, 618, 619},
        };

        // set expiration times
        auto now = std::chrono::steady_clock::now();
        cache.m_expirationTimes = {
            now - std::chrono::seconds(3600),   // 2_s
            now - std::chrono::seconds(3600),   // 2_d
            now - std::chrono::seconds(3600),   // 3_s
            now - std::chrono::seconds(3600),   // 3_d
            now + std::chrono::seconds(3600),   // 4_s
            now + std::chrono::seconds(3600),   // 4_d
            now - std::chrono::seconds(3600),   // 5_s
            now - std::chrono::seconds(3600),   // 5_d
            now + std::chrono::seconds(3600),   // 6_s
            now + std::chrono::seconds(3600),   // 6_d
        };
    }

public:
    void test() {
        std::vector<int> changed;
        cache.checkChangedItem(modRequest, changed);

        assert(changed.size() == 5);
        std::vector<int> expected = { 0, 2, 3, 5, 6 };
        assert(changed == expected);

        size_t nodeCount = 12;  // node count에서는 ghost depot 갯수는 포함하지 않음
        std::vector<int64_t> costMatrix = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,  // from 0
            100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,    // from 1
            200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212,    // from 3
            300, 301, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312,    // from 4
            400, 401, 402, 403, 404, 405, 406, 407, 408, 409, 410, 411, 412,    // from 7
            500, 501, 502, 503, 504, 505, 506, 507, 508, 509, 510, 511, 512,    // from 5_s
            600, 601, 602, 603, 604, 605, 606, 607, 608, 609, 610, 611, 612,    // from 5_d
            700, 701, 702, 703, 704, 705, 706, 707, 708, 709, 710, 711, 712,    // from 6_s
            800, 801, 802, 803, 804, 805, 806, 807, 808, 809, 810, 811, 812,    // from 6_d
            900, 901, 902, 903, 904, 905, 906, 907, 908, 909, 910, 911, 912,    // from 8_s
            1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011, 1012,   // from 8_d
            1100, 1101, 1102, 1103, 1104, 1105, 1106, 1107, 1108, 1109, 1110, 1111, 1112,   // from 9_s
            1200, 1201, 1202, 1203, 1204, 1205, 1206, 1207, 1208, 1209, 1210, 1211, 1212,   // from 9_d
        };
        // cacheIdx 순서
        // [3_d(x), 4_d, 7_d(n), 5_s(x), 5_d(x), 6_s, 6_d, 8_s(n), 8_d(n)]
        cache.updateCacheAndCost(modRequest, nodeCount, changed, costMatrix, costMatrix);
        // check if costMatrix is updated
        // from 4, 6 is updated with cache value
        size_t node4_d = 3, node6_s = 7, node6_d = 8;
        size_t node3_d = 2, node7_d = 4, node5_s = 5, node5_d = 6, node8_s = 9, node8_d = 10;
        assert(costMatrix[node4_d * (nodeCount + 1) + node4_d] == 415);
        assert(costMatrix[node4_d * (nodeCount + 1) + node6_s] == 418);
        assert(costMatrix[node4_d * (nodeCount + 1) + node6_d] == 419);
        assert(costMatrix[node6_s * (nodeCount + 1) + node4_d] == 605);
        assert(costMatrix[node6_s * (nodeCount + 1) + node6_s] == 608);
        assert(costMatrix[node6_s * (nodeCount + 1) + node6_d] == 609);
        assert(costMatrix[node6_d * (nodeCount + 1) + node4_d] == 615);
        assert(costMatrix[node6_d * (nodeCount + 1) + node6_s] == 618);
        assert(costMatrix[node6_d * (nodeCount + 1) + node6_d] == 619);

        // check if cache value is update
        // from 2 is deleted, other values is updated
        assert(cache.m_mapId.find(2) == cache.m_mapId.end());
        assert(cache.m_mapId[3] == 0);
        assert(cache.m_mapId[4] == 2);
        assert(cache.m_mapId[6] == 6);

        // check cache data is updated with cost
        size_t cache_idxs[] = {
            size_t(cache.m_mapId[3] + 1), size_t(cache.m_mapId[4] + 1), size_t(cache.m_mapId[7] + 1),
            size_t(cache.m_mapId[5]), size_t(cache.m_mapId[5] + 1), size_t(cache.m_mapId[6]), size_t(cache.m_mapId[6] + 1), size_t(cache.m_mapId[8]), size_t(cache.m_mapId[8] + 1)
        };
        size_t node_idxs[] = {
            node3_d, node4_d, node7_d,
            node5_s, node5_d, node6_s, node6_d, node8_s, node8_d
        };
        for (size_t i = 0; i < sizeof(cache_idxs) / sizeof(cache_idxs[0]); i++) {
            for (size_t j = 0; j < sizeof(cache_idxs) / sizeof(cache_idxs[0]); j++) {
                assert(cache.m_distCache[cache_idxs[i]][cache_idxs[j]] == costMatrix[node_idxs[i] * (nodeCount + 1) + node_idxs[j]]);
                assert(cache.m_timeCache[cache_idxs[i]][cache_idxs[j]] == costMatrix[node_idxs[i] * (nodeCount + 1) + node_idxs[j]]);
            }
        }
    }
};

int main(int argc, char **argv) {
    CCostCacheTest test;
    test.SetUp();
    test.test();
    return 0;
}
