#ifndef _INC_TEST_UTILITY_HDR
#define _INC_TEST_UTILITY_HDR

#include <string>
#include <lnsModRoute.h>
#include <unordered_map>
#include <cassert>

inline void hash_combine(std::size_t& seed, const std::size_t& value) {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct pair_hash {
    std::size_t operator()(const std::pair<std::string, int>& p) const {
        std::size_t h1 = std::hash<std::string>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        std::size_t seed = 0;
        hash_combine(seed, h1);
        hash_combine(seed, h2);
        return seed;
    };
};

using StationToIdxMap = std::unordered_map<std::pair<std::string, int>, int, pair_hash>;

ModRequest parseRequest(const char *body);
std::string readFile(const std::string& filePath);
std::string logNow();
void pushStationToIdx(StationToIdxMap& stationToIdx, const std::string& stationId, int direction, int idx);
std::string parseModIdx(int idx, const ModRequest& request);
std::string parseCacheIdx(int idx, size_t nodeCount, const ModRequest& request);

#endif // _INC_TEST_UTILITY_HDR
