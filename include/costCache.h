#ifndef _INC_COSTCACHE_HDR
#define _INC_COSTCACHE_HDR

#include <vector>
#include <map>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <mutex>
#include <cstdint>
#include <filesystem>
#include <lnsModRoute.h>

inline void hash_combine(std::size_t& seed, const std::size_t& value) {
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct pair_hash {
    std::size_t operator()(const std::pair<std::string, std::string>& p) const {
        std::size_t h1 = std::hash<std::string>{}(p.first);
        std::size_t h2 = std::hash<std::string>{}(p.second);
        std::size_t seed = 0;
        hash_combine(seed, h1);
        hash_combine(seed, h2);
        return seed;
    };
};

class CCostCache {
public:
    CCostCache(std::chrono::seconds maxAge = std::chrono::seconds(3600));
    virtual ~CCostCache();

    void setMaxAge(std::chrono::seconds maxAge);
    void clear();
    bool checkChangedItem(const ModRequest &modRequest, std::vector<int>& changed);
    void updateCacheAndCost(const ModRequest &modRequest, size_t nodeCount, const std::vector<int>& changed, std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix);

    friend class CCostCacheTest;

    void addEdge(std::string fromNode, std::string toNode, int64_t dist, int64_t time);
    bool getEdge(std::string fromNode, std::string toNode, int64_t& dist, int64_t& time);
    bool isEdgeCached(std::string fromNode);
    void loadStationCache(const std::string& cacheDir, const std::string& cacheKey);
    void loadStationCache(const std::string& cachePath);
    void clearStationCache();

    void exportStationCache(const std::string& path);

private:
    std::chrono::seconds m_maxAge;
    std::map<std::string, int> m_mapId;
    std::vector<std::chrono::time_point<std::chrono::steady_clock>> m_expirationTimes;
    std::vector<std::vector<int64_t>> m_distCache;
    std::vector<std::vector<int64_t>> m_timeCache;

    std::string m_lastLoadedCachePath;
    std::filesystem::file_time_type m_lastLoadedCacheTime;

    std::string m_lastLocHash;
    std::vector<int64_t> m_lastDistMatrix;
    std::vector<int64_t> m_lastTimeMatrix;

    std::mutex m_mutex;

    std::unordered_map<std::pair<std::string, std::string>, std::pair<int64_t, int64_t>, pair_hash> m_stationCache;

    bool checkForStationCache(const ModRequest &modRequest, std::vector<int>& changed);
    void updateForStationCache(const ModRequest &modRequest, size_t nodeCount, const std::vector<int>& changed, std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix);

    bool checkForLocalCache(const ModRequest &modRequest, std::vector<int>& changed);
    void updateForLocalCache(const ModRequest &modRequest, size_t nodeCount, const std::vector<int>& changed, std::vector<int64_t>& distMatrix, std::vector<int64_t>& timeMatrix);
};

struct order_hash {
    std::size_t operator()(const std::pair<std::string, int>& p) const {
        std::size_t h1 = std::hash<std::string>{}(p.first);
        std::size_t h2 = std::hash<int>{}(p.second);
        std::size_t seed = 0;
        hash_combine(seed, h1);
        hash_combine(seed, h2);
        return seed;
    };
};

using StationToIdxMap = std::unordered_map<std::pair<std::string, int>, int, order_hash>;

inline std::pair<std::string, int> makeStationKey(const std::string& stationId, int direction)
{
    return std::make_pair(stationId, direction > 0 ? (direction / 30) * 30 : direction);
}

void pushStationToIdx(StationToIdxMap& stationToIdx, const std::string& stationId, int direction, int idx);

#define CHECK_COST_VEHICLE_ONLY_ASSIGNED

int updateFromVehicleCostMatrixWithStationCache(
    const ModRequest& modRequest,
    const size_t nodeCount,
    StationToIdxMap stationToIdx,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix);

int updateChangedCostMatrixWithStationCache(
    const ModRequest& modRequest,
    const size_t nodeCount,
    const std::vector<int>& changed,
    const std::vector<int>& notChanged,
    StationToIdxMap& stationToIdx,
    std::vector<int64_t>& distMatrix,
    std::vector<int64_t>& timeMatrix);


#endif // _INC_COSTCACHE_HDR
