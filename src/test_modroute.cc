#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <costCache.h>
#include <queryOsrmCost.h>
#include <queryValhallaCost.h>
#include <main_utility.h>
#include <requestLogger.h>
#include "lib_modroute.h"

extern CCostCache g_costCache;

std::string readFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Fail to open file");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string makeDispatchSolutionResponse(
    const ModRequest& modRequest,
    const ModDispatchSolution& solution)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(8) << "{\"vehicle_routes\": [";
    for (int i = 0; i < solution.vehicle_routes.size(); i++) {
        auto& vehicle_route = solution.vehicle_routes[i];
        oss << (i == 0 ? "" : ",") << "{\"supply_idx\":\"" << vehicle_route.supply_idx << "\",\"routes\":[";
        for (int j = 0; j < vehicle_route.routes.size(); j++) {
            auto& route = vehicle_route.routes[j];
            oss << (j > 0 ? "," : "") << "{\"id\":\"" << route.id << "\",\"demand\":" << route.demand;
            oss << ",\"loc\":{\"lat\":" << route.location.lat << ",\"lng\":" << route.location.lng;
            if (route.location.waypoint_id >= 0) {
                oss << ",\"waypoint_id\":" << route.location.waypoint_id;
            }
            if (!route.location.station_id.empty()) {
                oss << ",\"station_id\":\"" << route.location.station_id << "\"";
            }
            oss << "}}";
        }
        oss << "]}";
    }
    oss << "],\"missing\":[";
    if (solution.missing.size() > 0) {
        for (int i = 0; i < solution.missing.size(); i++) {
            auto& route = solution.missing[i];
            oss << (i > 0 ? "," : "") << "{\"id\":\"" << route.id << "\",\"demand\":" << route.demand << "}";
        }
    }
    oss << "],\"unacceptables\":[";
    if (solution.unacceptables.size() > 0) {
        for (int i = 0; i < solution.unacceptables.size(); i++) {
            auto& route = solution.unacceptables[i];
            oss << (i > 0 ? "," : "") << "{\"id\":\"" << route.id << "\",\"demand\":" << route.demand << "}";
        }
    }
    oss << "],\"total_distance\":" << solution.total_distance << ",\"total_time\":" << solution.total_time << "}"; 
    return oss.str();
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mod_request_log> <cache_path>" << std::endl;
        return 1;
    }

    AlgorithmParameters ap = default_algorithm_parameters();
    ModRouteConfiguration conf = default_mod_configuraiton();
    conf.bLogRequest = true;
    conf.nServiceTime = 10;
    conf.nMaxDuration = 2400;
    ap.time_limit = 3;
    ap.thread_count = 1;
    ap.enable_missing_solution = true;
    ap.skip_remove_route = true;
    std::string route_path = "http://localhost:8002";
    RouteType route_type = ROUTE_VALHALLA;
    int route_tasks = 4;
    std::string cache_path = argc >= 3 ? argv[2] : "";

    std::string body = readFile(argv[1]);
    ModRequest mod_request = parseRequest(body.c_str());

    std::vector<ModDispatchSolution> dispatch_solutions = run_optimize(mod_request, route_path, route_type, route_tasks, cache_path, &ap, conf);
    for (int i = 0; i < dispatch_solutions.size(); i++) {
        auto dispatch_solution = dispatch_solutions[i];
        std::cout << makeDispatchSolutionResponse(mod_request, dispatch_solution) << std::endl;
    }

    return 0;
}

