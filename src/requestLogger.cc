#include <sstream>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <requestLogger.h>

std::string g_logPath;
std::string g_logDate;
std::mutex g_logPathMutex;

void checkLogPath(std::tm* now_tm)
{
    std::lock_guard<std::mutex> lock(g_logPathMutex);
    std::ostringstream oss;
    oss << std::put_time(now_tm, "%y%m%d");
    std::string currentDate = oss.str();
    
    if (g_logDate != currentDate) {
        g_logDate = currentDate;
        g_logPath = "./logs/" + g_logDate;
        std::filesystem::create_directories(g_logPath);
    }
}

void prepareLogPath()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm_local;
#ifdef _WIN32
    localtime_s(&now_tm_local, &now_c);
#else
    localtime_r(&now_c, &now_tm_local);
#endif
    checkLogPath(&now_tm_local);
}

void logRequest(
    const size_t nodeCount,
    const size_t vehicleCount,
    const std::vector<int64_t>& costMatrix,
    const std::vector<int64_t>& distMatrix,
    const std::vector<int64_t>& timeMatrix,
    const CModState& modState)
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm now_tm_local;
#ifdef _WIN32
    localtime_s(&now_tm_local, &now_c);
#else
    localtime_r(&now_c, &now_tm_local);
#endif
    checkLogPath(&now_tm_local);
    std::ostringstream oss;
    oss << g_logPath << "/" << std::put_time(&now_tm_local, "%H%M%S") << "_" << std::setw(3) << std::setfill('0') << now_ms.count() << "_req.txt";
    std::ofstream wsFile(oss.str());
    if (wsFile.is_open()) {
        wsFile << nodeCount + 1 << std::endl;
        for (auto c : costMatrix) {
            wsFile << " " << c;
        }
        wsFile << std::endl;
        for (auto d : distMatrix) {
            wsFile << " " << d;
        }
        wsFile << std::endl;
        for (auto t : timeMatrix) {
            wsFile << " " << t;
        }
        wsFile << std::endl;
        for (auto d : modState.demands) {
            wsFile << " " << d;
        }
        wsFile << std::endl;
        for (auto s : modState.serviceTimes) {
            wsFile << " " << s;
        }
        wsFile << std::endl;
        for (auto e : modState.earliestArrival) {
            wsFile << " " << e;
        }
        wsFile << std::endl;
        for (auto l : modState.latestArrival) {
            wsFile << " " << l;
        }
        wsFile << std::endl;
        for (auto a : modState.acceptableArrival) {
            wsFile << " " << a;
        }
        wsFile << std::endl;
        for (auto p : modState.pickupSibling) {
            wsFile << " " << p;
        }
        wsFile << std::endl;
        for (auto d : modState.deliverySibling) {
            wsFile << " " << d;
        }
        wsFile << std::endl;
        for (auto f : modState.fixedAssignment) {
            wsFile << " " << f;
        }
        wsFile << std::endl;
        wsFile << " " << vehicleCount << std::endl;
        for (auto c : modState.vehicleCapacities) {
            wsFile << " " << c;
        }
        wsFile << std::endl;
        for (auto s : modState.startDepots) {
            wsFile << " " << s;
        }
        wsFile << std::endl;
        for (auto e : modState.endDepots) {
            wsFile << " " << e;
        }
        wsFile << std::endl;
        wsFile << modState.initialSolution.size() << std::endl;
        for (auto i : modState.initialSolution) {
            wsFile << " " << i;
        }
        wsFile << std::endl;
    }
}

void logResponse(
    const ModRequest& modRequest,
    const CModState& modState,
    const size_t nodeCount,
    const size_t vehicleCount,
    std::unordered_map<int, ModRoute>& mapModRoute,
    const std::vector<int64_t>& distMatrix,
    const std::vector<int64_t>& timeMatrix,
    const Solution* solution)
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm now_tm_local;
#ifdef _WIN32
    localtime_s(&now_tm_local, &now_c);
#else
    localtime_r(&now_c, &now_tm_local);
#endif
    checkLogPath(&now_tm_local);
    std::ostringstream oss;
    oss << g_logPath <<  "/" << std::put_time(&now_tm_local, "%H%M%S") << "_" << std::setw(3) << std::setfill('0') << now_ms.count() << "_res.txt";
    std::ofstream wsFile(oss.str());
    if (wsFile.is_open()) {
        for (int i = 0; i < solution->n_routes; i++) {
            auto& route = solution->routes[i];
            auto& vehicle = modRequest.vehicleLocs[solution->vehicles[i]];
            int64_t s_dist = 0;
            int64_t s_time = 0;
            wsFile << "supply_idx: " << vehicle.supplyIdx << std::endl;
            for (int j = 0; j < route.length; j++) {
                auto& node = route.path[j];
                if (node < 0) {
                    continue;
                }
                if (node == 0) {
                    continue;
                }
                if (node <= vehicleCount) {
                    if (solution->vehicles[i] + 1 != node) {
                        throw std::runtime_error("Invalid vehicle assignment");
                    }
                } else {
                    auto& prevNode = route.path[j - 1];
                    wsFile << "  [" << mapModRoute[node].id << "," << mapModRoute[node].demand << "] = ";
                    s_dist += distMatrix[prevNode * (nodeCount + 1) + node];
                    s_time += timeMatrix[prevNode * (nodeCount + 1) + node] + modState.serviceTimes[prevNode];
                    wsFile << s_dist << ", " << s_time << std::endl;
                }
            }
        }
        if (solution->n_missing > 0) {
            for (int i = 0; i < solution->n_missing; i++) {
                auto& node = solution->missing[i];
                wsFile << "missing: " << mapModRoute[node].id << " demand: " << mapModRoute[node].demand << std::endl;
            }
        }
        wsFile << "total_distance: " << static_cast<int64_t>(solution->distance) << std::endl;
        wsFile << "total_time: " << static_cast<int64_t>(solution->time) << std::endl;
    }
}
