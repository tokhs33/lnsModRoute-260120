#include <string>
#include <iomanip>
#include <fstream>
#include <mutex>
#include <cpp-httplib/httplib.h>
#include <gason/gason.h>
#include <lnsModRoute.h>
#include <queryOsrmCost.h>
#include <queryValhallaCost.h>
#include <costCache.h>
#include <requestLogger.h>
#include <main_utility.h>
#include <eurekaClient.h>

extern CCostCache g_costCache;

ModCacheRequest parseCacheRequest(const char *body)
{
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse((char*) body, &endptr, &value, allocator);
    if (status != JSON_OK) {
        throw std::runtime_error("Fail to parse JSON request");
    }
    if (value.getTag() != JSON_OBJECT || !value.toNode()) {
        throw std::runtime_error("Invalid JSON request format");
    }

    ModCacheRequest request;

    for (auto item : value) {
        if (strcmp(item->key, "key") == 0) {
            request.cacheKey = item->value.toString();
        }
    }
    return request;
}

// JSON 이스케이핑 함수
std::string escapeJson(const std::string& input) {
    std::string output;
    output.reserve(input.size() * 1.1);  // 약간의 오버헤드 방지
    for (char c : input) {
        switch (c) {
            case '"':  output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 32) {
                    // 제어 문자 이스케이핑 (선택적, JSON 표준에서는 필수 아님)
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                    output += buf;
                } else {
                    output += c;
                }
                break;
        }
    }
    return output;
}

std::string makeResponse(
    const ModRequest& modRequest,
    std::unordered_map<int, ModRoute> mapNodeToModRoute,
    std::vector<Solution *>& solutions)
{
    size_t vehicleCount = modRequest.vehicleLocs.size();
    std::ostringstream s_response;
    s_response << "{\"status\":0,\"results\":[";
    for (int i = 0; i < solutions.size(); i++) {
        auto solution = solutions[i];
        if (i > 0) {
            s_response << ",";
        }
        s_response << makeDispatchResponse(modRequest, vehicleCount, mapNodeToModRoute, solution);
        delete_solution(solution);
    }
    s_response << "]}";
    solutions.clear();
    return s_response.str();
}

std::mutex g_httpLogMutex;

void logHttpRequest(const httplib::Request &req, const httplib::Response &res) {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm now_tm_local;
#ifdef _WIN32
    localtime_s(&now_tm_local, &now_c);
#else
    localtime_r(&now_c, &now_tm_local);
#endif
    
    std::ostringstream filename;
    filename << "./logs/" << std::put_time(&now_tm_local, "%y%m%d") << "_access.log";
    
    std::lock_guard<std::mutex> lock(g_httpLogMutex);
    std::ofstream logFile(filename.str(), std::ios::app);
    if (logFile.is_open()) {
        logFile << std::put_time(&now_tm_local, "%H:%M:%S") 
                << "." << std::setw(3) << std::setfill('0') << now_ms.count()
                << " [" << req.remote_addr << "] "
                << "[" << req.method << "] " << req.path
                << " -> " << res.status
                << " (req: " << req.body.size() << " bytes, "
                << "res: " << res.body.size() << " bytes)\n";
        
        // 상세 로그가 필요한 경우 아래 주석 해제
        if (!req.body.empty()) {
            logFile << "Request Body: " << req.body << "\n";
        }
        if (!res.body.empty()) {
            logFile << "Response Body: " << res.body << "\n";
        }
        logFile << "---\n";
    }
}


int main(int argc, char **argv)
{
    int nSvrPort = 8080;
    std::string sSvrHost = "localhost";
    std::string sRoutePath = "http://localhost:8002";
    int nRouteTasks = 4;
    std::string sCacheDir = "";
    std::string sInitCacheKey = "";
    std::string sEurekaUrl = "";
    std::string sEurekaHost = "localhost";
    RouteType eRouteType = ROUTE_VALHALLA;
    auto conf = default_mod_configuraiton();
    auto parameter = default_algorithm_parameters();
    parameter.time_limit = 1;
    parameter.waittime_penalty = 0.0;
    parameter.delaytime_penalty = 10.0;
    parameter.enable_missing_solution = true;
#ifdef NDEBUG
    parameter.nb_iterations = 5000; // 더 빨리 끝내기 위해 반복 횟수를 줄임
#endif
    std::string appName = "LNS-DISPATCH-SERVICE";
    bool bLogHttp = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            nSvrPort = std::stoi(argv[++i]);
        } else if (arg == "--host" && i + 1 < argc) {
            sSvrHost = argv[++i];
        } else if (arg == "--route-path" && i + 1 < argc) {
            sRoutePath = argv[++i];
        } else if (arg == "--route-type" && i + 1 < argc) {
            std::string type = argv[++i];
            if (type == "OSRM") {
                eRouteType = ROUTE_OSRM;
            } else if (type == "VALHALLA") {
                eRouteType = ROUTE_VALHALLA;
            } else {
                std::cerr << "Invalid route type: " << type << std::endl;
                return 1;
            }
        } else if (arg == "--route-tasks" && i + 1 < argc) {
            nRouteTasks = std::stoi(argv[++i]);
            if (nRouteTasks < 1) {
                std::cerr << "Invalid route tasks: " << nRouteTasks << std::endl;
                return 1;
            }
        } else if (arg == "--max-duration" && i + 1 < argc) {
            conf.nMaxDuration = std::stoi(argv[++i]);
        } else if (arg == "--service-time" && i + 1 < argc) {
            conf.nServiceTime = std::stoi(argv[++i]);
        } else if (arg == "--bypass-ratio" && i + 1 < argc) {
            conf.nBypassRatio = std::stoi(argv[++i]);
        } else if (arg == "--acceptable-buffer" && i + 1 < argc) {
            conf.nAcceptableBuffer = std::stoi(argv[++i]);
        } else if (arg == "--cache-expiration-time" && i + 1 < argc) {
            conf.nCacheExpirationTime = std::stoi(argv[++i]);
        } else if (arg == "--delaytime-penalty" && i + 1 < argc) {
            parameter.delaytime_penalty = std::stod(argv[++i]);
        } else if (arg == "--waittime-penalty" && i + 1 < argc) {
            parameter.waittime_penalty = std::stod(argv[++i]);
        } else if (arg == "--log-request") {
            conf.bLogRequest = true;
            prepareLogPath();
        } else if (arg == "--log-http") {
            bLogHttp = true;
            prepareLogPath();
        } else if (arg == "--cache-directory" && i + 1 < argc) {
            sCacheDir = argv[++i];
        } else if (arg == "--init-cache-key" && i + 1 < argc) {
            sInitCacheKey = argv[++i];
        } else if (arg == "--max-solution-limit" && i + 1 < argc) {
            conf.nSolutionLimit = std::stoi(argv[++i]);
        } else if (arg == "--eureka-app" && i + 1 < argc) {
            appName = argv[++i];
        } else if (arg == "--eureka-url" && i + 1 < argc) {
            sEurekaUrl = argv[++i];
        } else if (arg == "--eureka-host" && i + 1 < argc) {
            sEurekaHost = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --port <port> : Server port (default: 8080)" << std::endl;
            std::cout << "  --host <host> : Server host (default: localhost)" << std::endl;
            std::cout << "  --route-path <url> : Route service path (default: http://localhost:8002)" << std::endl;
            std::cout << "  --route-type <type> : Route service type [OSRM|VALHALLA] (default: VALHALLA)" << std::endl;
            std::cout << "  --route-tasks <count> : Number of route tasks (default: 4)" << std::endl;
            std::cout << "  --max-duration <seconds> : Maximum duration for route (default: 7200)" << std::endl;
            std::cout << "  --service-time <seconds> : Service time for each node (default: 10)" << std::endl;
            std::cout << "  --bypass-ratio <ratio> : Bypass ratio percent for each node (default: 100)" << std::endl;
            std::cout << "  --acceptable-buffer <seconds> : Acceptable buffer time for each node (default: 600)" << std::endl;
            std::cout << "  --cache-expiration-time <seconds> : Cache expiration time (default: 3600)" << std::endl;
            std::cout << "  --delaytime-penalty <value> : Delay Time penalty (default: 10.0)" << std::endl;
            std::cout << "  --waittime-penalty <value> : Wait Time penalty (default: 0.0)" << std::endl;
            std::cout << "  --log-request : Log request and response" << std::endl;
            std::cout << "  --log-http : Log HTTP access" << std::endl;
            std::cout << "  --cache-directory <path> : Cache directory path" << std::endl;
            std::cout << "  --init-cache-key <key> : Initialize cache with key" << std::endl;
            std::cout << "  --max-solution-limit <count> : Maximum solution limit (default: 3)" << std::endl;
            std::cout << "  --eureka-app <name> : Eureka application name (default: LNS-DISPATCH-SERVICE)" << std::endl;
            std::cout << "  --eureka-url <url> : Eureka server URL (e.g., http://localhost:8761)" << std::endl;
            std::cout << "  --eureka-host <host> : Eureka instance host (default: localhost)" << std::endl;
            return 0;
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << std::endl;
            return 1;
        }
    }

    std::string instanceId = sEurekaHost + ":" + appName + ":"  + std::to_string(nSvrPort);
    EurekaClient eurekaClient(sEurekaUrl, appName, instanceId, sEurekaHost, nSvrPort);

    httplib::Server svr;
#ifndef NDEBUG
    svr.set_read_timeout(3600, 0);
    svr.set_write_timeout(3600, 0);
#endif
    g_costCache.setMaxAge(std::chrono::seconds(conf.nCacheExpirationTime));
    if (!sInitCacheKey.empty()) {
        g_costCache.loadStationCache(sCacheDir, sInitCacheKey);
    }

    // HTTP 로깅 설정
    if (bLogHttp) {
        svr.set_logger(logHttpRequest);
    }

    // Eureka 등록 및 hearbeat 시작
    if (!sEurekaUrl.empty()) {
        eurekaClient.registerInstance();
        eurekaClient.startHeartbeat();
    }

    svr.Post("/api/v1/optimize", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            std::vector<char> body(req.body.begin(), req.body.end());
            body.push_back('\0');
            ModRequest request = parseRequest(body.data());

            std::unordered_map<int, ModRoute> mapNodeToModRoute = makeNodeToModRoute(request, request.vehicleLocs.size());
            auto solutions = runOptimize(request, sRoutePath, eRouteType, nRouteTasks, mapNodeToModRoute, &parameter, conf, true);
            auto response = makeResponse(request, mapNodeToModRoute, solutions);
            res.set_content(response.c_str(), "application/json");
        } catch (std::exception& e) {
            res.status = 400;
            std::string error = "{\"status\":400,\"error\": \"" + escapeJson(e.what()) + "\"}";
            res.set_content(error, "application/json");
        }
    });
    svr.Get("/api/v1/reset", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            if (eRouteType == ROUTE_OSRM) {
                queryCostOsrmReset();
            } else if (eRouteType == ROUTE_VALHALLA) {
                queryCostValhallaReset();
            }
            res.set_content("{\"status\":0}", "application/json");
        } catch (std::exception& e) {
            res.status = 400;
            std::string error = "{\"status\":400,\"error\": \"" + escapeJson(e.what()) + "\"}";
            res.set_content(error, "application/json");
        }
    });
    svr.Put("/api/v1/cache", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            std::vector<char> body(req.body.begin(), req.body.end());
            body.push_back('\0');
            ModCacheRequest request = parseCacheRequest(body.data());
            g_costCache.loadStationCache(sCacheDir, request.cacheKey);
            res.set_content("{\"status\":0}", "application/json");
        } catch (std::exception& e) {
            res.status = 400;
            std::string error = "{\"status\":400,\"error\": \"" + escapeJson(e.what()) + "\"}";
            res.set_content(error, "application/json");
        }
    });
    svr.Delete("/api/v1/cache", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            g_costCache.clearStationCache();
            res.set_content("{\"status\":0}", "application/json");
        } catch (std::exception& e) {
            res.status = 400;
            std::string error = "{\"status\":400,\"error\": \"" + escapeJson(e.what()) + "\"}";
            res.set_content(error, "application/json");
        }
    });
    svr.Get("/api/v1/health", [&](const httplib::Request &req, httplib::Response &res) {
        res.set_content("{\"status\":0}", "application/json");
    });
#ifndef NDEBUG
    svr.Get("/api/v1/quit", [&](const httplib::Request &req, httplib::Response &res) {
        res.set_content("{\"status\":0}", "application/json");
        svr.stop();
    });
#endif
    std::string openapiPath = "./lnsmodroute.yaml";
    svr.Get("/api/v1/openapi", [&](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");  // CORS 허용
        try {
            std::ifstream file(openapiPath);
            if (!file.is_open()) {
                throw std::runtime_error("OpenAPI spec file not found");
            }
            std::string spec((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            res.set_content(spec, "application/yaml");  // 또는 "application/json" if JSON
        } catch (std::exception& e) {
            res.status = 500;
            std::string error = "{\"status\":500,\"error\": \"" + escapeJson(e.what()) + "\"}";
            res.set_content(error, "application/json");
        }
    });
    svr.listen(sSvrHost, nSvrPort);

    // 서버 종료 시 Eureka 해제
    if (!sEurekaUrl.empty()) {
        eurekaClient.stopHeartbeat();
        eurekaClient.deregisterInstance();
    }

    return 0;
}
