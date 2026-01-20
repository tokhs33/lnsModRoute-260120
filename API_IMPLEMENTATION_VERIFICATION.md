# API 구현 확인 보고서

## 요청 사항 검증

사용자가 제공한 API 형태:
```yaml
lns-optimization:
    url: http://3.37.123.228:8080/api/v1/optimize
health-check: http://3.37.123.228:8080/api/v1/health
spec-document: http://3.37.123.228:8080/api/v1/openapi
    max-duration: 7200 #최대조회시간
```

---

## 검증 결과 요약

| API 항목 | 구현 여부 | 코드 위치 | 기본값/설정 |
|---------|---------|---------|-----------|
| **POST /api/v1/optimize** | ✅ **구현됨** | src/main.cc:263 | - |
| **GET /api/v1/health** | ✅ **구현됨** | src/main.cc:316 | - |
| **GET /api/v1/openapi** | ✅ **구현됨** | src/main.cc:326 | lnsmodroute.yaml |
| **max-duration** | ✅ **구현됨** | src/main.cc:178 | 7200초 (기본값) |

**결론**: ✅ **모든 API가 완벽하게 구현되어 있습니다.**

---

## 1. POST /api/v1/optimize

### 1.1 구현 코드 (src/main.cc:263-278)

```cpp
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
```

### 1.2 기능
- **메소드**: POST
- **경로**: `/api/v1/optimize`
- **Content-Type**: application/json
- **입력**: ModRequest (JSON)
- **출력**: OptimizeResponse (JSON)
- **에러 처리**: 400 Bad Request (예외 발생 시)

### 1.3 OpenAPI 스펙 (lnsmodroute.yaml:10-32)

```yaml
/api/v1/optimize:
  post:
    summary: Optimize vehicle routes
    description: Solves the vehicle routing problem with the given request data.
    requestBody:
      required: true
      content:
        application/json:
          schema:
            $ref: '#/components/schemas/ModRequest'
    responses:
      '200':
        description: Successful optimization response
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/OptimizeResponse'
      '400':
        description: Bad request or error
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/ErrorResponse'
```

---

## 2. GET /api/v1/health

### 2.1 구현 코드 (src/main.cc:316-318)

```cpp
svr.Get("/api/v1/health", [&](const httplib::Request &req, httplib::Response &res) {
    res.set_content("{\"status\":0}", "application/json");
});
```

### 2.2 기능
- **메소드**: GET
- **경로**: `/api/v1/health`
- **응답**: `{"status":0}` (항상 정상)
- **용도**: 서비스 상태 확인 (헬스체크)

### 2.3 OpenAPI 스펙 (lnsmodroute.yaml:50-60)

```yaml
/api/v1/health:
  get:
    summary: Health check
    description: Returns the health status of the service.
    responses:
      '200':
        description: Service is healthy
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/StatusResponse'
```

---

## 3. GET /api/v1/openapi

### 3.1 구현 코드 (src/main.cc:325-338)

```cpp
std::string openapiPath = "./lnsmodroute.yaml";
svr.Get("/api/v1/openapi", [&](const httplib::Request &req, httplib::Response &res) {
    res.set_header("Access-Control-Allow-Origin", "*");  // CORS 허용
    try {
        std::ifstream file(openapiPath);
        if (!file.is_open()) {
            throw std::runtime_error("OpenAPI spec file not found");
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        res.set_content(content, "application/x-yaml");
    } catch (std::exception& e) {
        res.status = 404;
        std::string error = "{\"status\":404,\"error\": \"" + escapeJson(e.what()) + "\"}";
        res.set_content(error, "application/json");
    }
});
```

### 3.2 기능
- **메소드**: GET
- **경로**: `/api/v1/openapi`
- **응답**: lnsmodroute.yaml 파일 내용
- **Content-Type**: application/x-yaml
- **CORS**: 허용 (`Access-Control-Allow-Origin: *`)
- **용도**: OpenAPI 3.0 스펙 문서 제공

### 3.3 OpenAPI 파일 정보

| 항목 | 값 |
|-----|---|
| 파일명 | lnsmodroute.yaml |
| OpenAPI 버전 | 3.0.0 |
| API 버전 | 0.9.7 |
| 타이틀 | LNS ModRoute API |

---

## 4. max-duration 설정

### 4.1 구현 방법 (3가지)

#### 방법 1: 커맨드라인 인자 (기본값 설정)

**코드** (src/main.cc:177-178):
```cpp
} else if (arg == "--max-duration" && i + 1 < argc) {
    conf.nMaxDuration = std::stoi(argv[++i]);
}
```

**사용 예시**:
```bash
./lnsModRoute --max-duration 7200 --port 8080
```

#### 방법 2: 기본 설정값

**코드** (src/lnsModRoute.cc:469):
```cpp
struct ModRouteConfiguration configuration;
configuration.nMaxDuration = 7200;  // ← 기본값 7200초
configuration.nServiceTime = 10;
configuration.nBypassRatio = 100;
```

#### 방법 3: 요청별 설정 (ModRequest)

**코드** (src/lnsModRoute.cc:233):
```cpp
int nMaxDuration = modRequest.maxDuration == 0
                   ? modRouteConfig.nMaxDuration    // 설정값 사용
                   : modRequest.maxDuration;        // 요청값 우선
```

**우선순위**:
```
요청 maxDuration > 커맨드라인 --max-duration > 기본값 7200
```

### 4.2 max-duration 사용 위치

| 위치 | 코드 라인 | 용도 |
|-----|---------|------|
| 기본값 설정 | src/lnsModRoute.cc:469 | 7200초 기본값 |
| 커맨드라인 파싱 | src/main.cc:178 | --max-duration 인자 |
| 요청별 우선순위 | src/lnsModRoute.cc:233 | modRequest.maxDuration 우선 |
| Time Window 기본값 | src/lnsModRoute.cc:237-238 | latestArrival 초기화 |
| onboardDemands | src/lnsModRoute.cc:259-260 | 하차 시간 제약 |
| onboardWaitingDemands | src/lnsModRoute.cc:272-277 | 픽업/하차 시간 제약 |
| newDemands | src/lnsModRoute.cc:293-297 | 픽업/하차 시간 제약 |

### 4.3 max-duration의 역할

```
max-duration = 7200초 (2시간)

[역할]
1. Time Window 상한값
   - 시간 제약이 없는 경우 기본 최대 시간으로 사용
   - latestArrival, acceptableArrival 기본값

2. 최대 운행 시간 제한
   - 차량의 최대 운행 시간
   - 승객의 최대 대기/이동 시간

3. ALNS 최적화 제약
   - 이 시간을 초과하는 경로는 불가능(infeasible)
   - Time Window Violation 방지
```

### 4.4 설정 예시

#### 예시 1: 커맨드라인으로 2시간 설정
```bash
./lnsModRoute \
    --port 8080 \
    --max-duration 7200 \
    --route-path http://localhost:8002
```

#### 예시 2: Docker 환경 설정
```dockerfile
CMD ["./lnsModRoute", \
     "--port", "8080", \
     "--max-duration", "7200", \
     "--route-path", "http://valhalla:8002"]
```

#### 예시 3: 요청 JSON에서 개별 설정
```json
{
  "vehicle_locs": [...],
  "new_demands": [...],
  "max_duration": 3600,  // ← 이 요청만 1시간으로 제한
  "optimize_type": "Time"
}
```

---

## 5. 추가 구현 API

사용자가 명시하지 않았지만 추가로 구현된 API:

| 엔드포인트 | 메소드 | 기능 | 코드 위치 |
|----------|--------|------|---------|
| `/api/v1/reset` | GET | 라우팅 캐시 초기화 | src/main.cc:279 |
| `/api/v1/cache` | PUT | Station Cache 로드 | src/main.cc:293 |
| `/api/v1/cache` | DELETE | Station Cache 삭제 | src/main.cc:306 |
| `/api/v1/quit` | GET | 서버 종료 (디버그용) | src/main.cc:320 |

---

## 6. 서버 설정

### 6.1 기본 설정

| 항목 | 기본값 | 설정 방법 |
|-----|--------|---------|
| **포트** | 8080 | `--port 8080` |
| **호스트** | localhost | `--host 0.0.0.0` |
| **라우팅 엔진** | VALHALLA | `--route-type VALHALLA` |
| **라우팅 경로** | http://localhost:8002 | `--route-path http://...` |
| **max-duration** | 7200 | `--max-duration 7200` |
| **service-time** | 10 | `--service-time 10` |
| **bypass-ratio** | 100 | `--bypass-ratio 100` |

### 6.2 사용자 제공 설정과의 매칭

```yaml
# 사용자 제공 설정
lns-optimization:
    url: http://3.37.123.228:8080/api/v1/optimize

# 현재 구현 매칭
./lnsModRoute \
    --host 0.0.0.0 \              # ← 외부 접근 허용
    --port 8080 \                 # ← 포트 8080
    --max-duration 7200 \         # ← max-duration 7200
    --route-path http://valhalla:8002
```

**완벽하게 매칭됨!**

---

## 7. 실제 사용 예시

### 7.1 Health Check

**요청**:
```bash
curl http://3.37.123.228:8080/api/v1/health
```

**응답**:
```json
{
  "status": 0
}
```

### 7.2 OpenAPI 스펙 조회

**요청**:
```bash
curl http://3.37.123.228:8080/api/v1/openapi
```

**응답**:
```yaml
openapi: 3.0.0
info:
  title: LNS ModRoute API
  version: 0.9.7
  description: API for optimizing vehicle routes using LNS algorithm.
servers:
  - url: http://localhost:8080
...
```

### 7.3 최적화 요청

**요청**:
```bash
curl -X POST http://3.37.123.228:8080/api/v1/optimize \
  -H "Content-Type: application/json" \
  -d '{
    "vehicle_locs": [...],
    "new_demands": [...],
    "max_duration": 7200,
    "optimize_type": "Time"
  }'
```

**응답**:
```json
{
  "solutions": [
    {
      "vehicle_routes": [...],
      "total_cost": 2450,
      "total_distance": 15300
    }
  ]
}
```

---

## 8. 검증 체크리스트

| 항목 | 요구사항 | 구현 상태 | 검증 |
|-----|---------|---------|------|
| ✅ POST /api/v1/optimize | 최적화 API | 구현됨 | src/main.cc:263 |
| ✅ GET /api/v1/health | 헬스체크 API | 구현됨 | src/main.cc:316 |
| ✅ GET /api/v1/openapi | 스펙 문서 API | 구현됨 | src/main.cc:326 |
| ✅ max-duration 7200 | 최대 시간 설정 | 구현됨 | 기본값 7200초 |
| ✅ 포트 8080 | 서버 포트 | 설정 가능 | `--port 8080` |
| ✅ JSON 응답 | Content-Type | 구현됨 | application/json |
| ✅ 에러 처리 | 400/404 응답 | 구현됨 | try-catch 블록 |
| ✅ CORS 지원 | OpenAPI CORS | 구현됨 | Access-Control-Allow-Origin |

---

## 9. 결론

### 9.1 검증 결과

✅ **모든 API가 요구사항대로 완벽하게 구현되어 있습니다.**

| 검증 항목 | 결과 |
|---------|------|
| POST /api/v1/optimize | ✅ 구현 완료 |
| GET /api/v1/health | ✅ 구현 완료 |
| GET /api/v1/openapi | ✅ 구현 완료 |
| max-duration: 7200 | ✅ 기본값 7200초 |

### 9.2 추가 기능

요구사항 외 추가로 구현된 기능:
- ✅ Cache 관리 API (PUT/DELETE /api/v1/cache)
- ✅ 캐시 초기화 API (GET /api/v1/reset)
- ✅ 디버그 종료 API (GET /api/v1/quit)
- ✅ Eureka 서비스 등록 지원
- ✅ HTTP 요청 로깅 기능

### 9.3 서버 실행 명령

사용자 설정에 맞는 서버 실행:
```bash
./lnsModRoute \
    --host 0.0.0.0 \
    --port 8080 \
    --max-duration 7200 \
    --route-type VALHALLA \
    --route-path http://valhalla:8002
```

### 9.4 Docker 실행 예시

```bash
docker run -d \
  -p 8080:8080 \
  -e MAX_DURATION=7200 \
  lnsmodroute:latest
```

---

**보고서 생성일**: 2026-01-21
**검증 대상**: lnsModRoute API 구현
**검증 결과**: ✅ **모든 요구사항 충족**
