#ifndef _INC_EUREKACLIENT_HDR
#define _INC_EUREKACLIENT_HDR

#include <string>
#include <cpp-httplib/httplib.h>
#include <chrono>
#include <ctime>
#include <iomanip>

// Eureka 클라이언트 클래스
class EurekaClient {
public:
    EurekaClient(const std::string& eurekaUrl, const std::string& appName, const std::string& instanceId, const std::string& host, int port)
        : eurekaUrl(eurekaUrl), appName(appName), instanceId(instanceId), host(host), port(port), running(false), lastHeartbeatSuccess(false), retryCount(0) {}

    bool registerInstance() {
        if (eurekaUrl.empty()) return false;

        httplib::Client client(eurekaUrl.c_str());
        std::string path = "/eureka/apps/" + appName;

        // Eureka 등록 JSON 생성
        std::string jsonBody = R"(
        {
            "instance": {
                "instanceId": ")" + instanceId + R"(",
                "hostName": ")" + host + R"(",
                "app": ")" + appName + R"(",
                "ipAddr": ")" + host + R"(",
                "vipAddress": ")" + appName + R"(",
                "port": {
                    "$": )" + std::to_string(port) + R"(,
                    "@enabled": "true"
                },
                "status": "UP",
                "dataCenterInfo": {
                    "@class": "com.netflix.appinfo.InstanceInfo$DefaultDataCenterInfo",
                    "name": "MyOwn"
                }
            }
        })";

        auto res = client.Post(path.c_str(), jsonBody, "application/json");
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);        
        if (res && res->status == 204) {
            std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Eureka registration successful for " << instanceId << std::endl;
            retryCount = 0;
            return true;
        } else {
            std::cerr << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Eureka registration failed: " << (res ? res->status : 0) << std::endl;
            return false;
        }
    }

    void sendHeartbeat() {
        if (eurekaUrl.empty()) return;

        httplib::Client client(eurekaUrl.c_str());
        std::string path = "/eureka/apps/" + appName + "/" + instanceId;

        auto res = client.Put(path.c_str(), "", "application/json");
        bool currentSuccess = (res && res->status == 200);

        // 이전 상태와 비교하여 변경되었을 때만 로그 출력
        if (currentSuccess != lastHeartbeatSuccess) {
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);            
            if (currentSuccess) {
                std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Eureka heartbeat successful for " << instanceId << std::endl;
            } else {
                std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Eureka heartbeat failed: " << (res ? res->status : 0) << std::endl;
            }
            lastHeartbeatSuccess = currentSuccess;
        }
        if (!currentSuccess) {
            if (retryCount == 0) {
                auto now = std::chrono::system_clock::now();
                std::time_t t = std::chrono::system_clock::to_time_t(now);
                std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Attempting re-registration for " << instanceId << std::endl;
            }
            retryCount++;
            if (registerInstance()) {
                lastHeartbeatSuccess = true;    // 성공 시 상태 업데이트
            }
        } else {
            retryCount = 0; // 성공 시 재시도 카운트 리셋
        }
    }

    void deregisterInstance() {
        if (eurekaUrl.empty()) return;

        httplib::Client client(eurekaUrl.c_str());
        std::string path = "/eureka/apps/" + appName + "/" + instanceId;

        auto res = client.Delete(path.c_str());
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        if (res && res->status == 200) {
            std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Eureka deregistration successful for " << instanceId << std::endl;
        } else {
            std::cout << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " Eureka deregistration failed: " << (res ? res->status : 0) << std::endl;
        }
    }

    void startHeartbeat() {
        if (eurekaUrl.empty()) return;

        running = true;
        heartbeatThread = std::thread([this]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                sendHeartbeat();
            }
        });
    }

    void stopHeartbeat() {
        if (eurekaUrl.empty()) return;

        running = false;
        if (heartbeatThread.joinable()) {
            heartbeatThread.join();
        }
    }

private:
    std::string eurekaUrl;
    std::string appName;
    std::string instanceId;
    std::string host;
    int port;
    std::atomic<bool> running;
    std::thread heartbeatThread;
    bool lastHeartbeatSuccess;  // 이전 heartbeat 성공 상태 추적
    int retryCount; // 재등록 재시도 횟수
};

#endif // _INC_EUREKACLIENT_HDR
