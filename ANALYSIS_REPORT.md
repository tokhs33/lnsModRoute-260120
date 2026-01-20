# lnsModRoute 프로젝트 분석 보고서

**작성일**: 2026-01-20
**프로젝트 버전**: 0.9.7
**분석 대상**: lnsModRoute-260120

---

## 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [ALNS vs LNS 알고리즘 분석](#2-alns-vs-lns-알고리즘-분석)
3. [동적 라우팅 알고리즘 상세 분석](#3-동적-라우팅-알고리즘-상세-분석)
4. [API 구성 분석](#4-api-구성-분석)
5. [아키텍처 분석](#5-아키텍처-분석)
6. [결론 및 권장사항](#6-결론-및-권장사항)

---

## 1. 프로젝트 개요

### 1.1 프로젝트 정보
- **프로젝트명**: lnsModRoute
- **설명**: LNS 알고리즘을 사용한 차량 라우팅 최적화 서비스
- **주요 기술 스택**:
  - C++20
  - CMake 빌드 시스템
  - REST API (cpp-httplib)
  - Python/Java 바인딩 지원

### 1.2 핵심 의존성
- **alns-pdp** (외부 라이브러리)
  - Repository: `https://github.com/cielmobilityDev/alns-pdp.git`
  - 역할: PDPTW (Pickup and Delivery Problem with Time Windows) 솔버 제공
  - 함수: `solve_lns_pdptw`

### 1.3 지원 플랫폼
- Linux (shared library)
- Windows (static library)
- Docker 컨테이너 (Alpine Linux 기반)

---

## 2. ALNS vs LNS 알고리즘 분석

### 2.1 명명 규칙 분석

**결론**: **프로젝트는 LNS라는 이름을 사용하지만, 실제로는 ALNS (Adaptive Large Neighborhood Search) 알고리즘을 구현하고 있습니다.**

#### 증거

1. **외부 라이브러리 이름**
   - 의존 라이브러리: `alns-pdp` (ALNS를 명시)
   - 프로젝트 이름: `lnsModRoute` (LNS를 명시)

2. **호출 함수 분석** (src/lnsModRoute.cc:392, 432)
   ```cpp
   auto solution = solve_lns_pdptw(
       nodeCount + 1, costMatrix.data(), distMatrix.data(), timeMatrix.data(),
       modState.demands.data(), modState.serviceTimes.data(),
       modState.earliestArrival.data(), modState.latestArrival.data(),
       modState.acceptableArrival.data(),
       modState.pickupSibling.data(), modState.deliverySibling.data(),
       modState.fixedAssignment.data(),
       (int) vehicleCount, modState.vehicleCapacities.data(),
       modState.startDepots.data(), modState.endDepots.data(),
       (int) modState.initialSolution.size(), modState.initialSolution.data(),
       ap
   );
   ```

3. **AlgorithmParameters 구조 분석** (src/py_modroute.cc:297-320)

### 2.2 ALNS 컴포넌트 확인

#### Destruction (Removal) 휴리스틱

| 파라미터 | 설명 | 분류 |
|---------|------|-----|
| `shaw_phi_distance` | Shaw removal - 거리 가중치 | ALNS Removal |
| `shaw_chi_time` | Shaw removal - 시간 가중치 | ALNS Removal |
| `shaw_psi_capacity` | Shaw removal - 용량 가중치 | ALNS Removal |
| `shaw_removal_p` | Shaw removal - 파라미터 p | ALNS Removal |
| `worst_removal_p` | Worst removal - 파라미터 p | ALNS Removal |

**Shaw Removal**: 유사한 특성을 가진 고객들을 함께 제거하는 휴리스틱
**Worst Removal**: 가장 비용이 높은 고객들을 제거하는 휴리스틱

#### Metaheuristic (탐색 전략)

| 파라미터 | 설명 | 분류 |
|---------|------|-----|
| `simulated_annealing_start_temp_control_w` | SA 시작 온도 제어 | Metaheuristic |
| `simulated_annealing_cooling_rate_c` | SA 냉각 비율 | Metaheuristic |

**Simulated Annealing**: 지역 최적해를 탈출하기 위한 확률적 탐색 기법

#### Adaptive Mechanism (적응형 메커니즘)

| 파라미터 | 설명 | 분류 |
|---------|------|-----|
| `adaptive_weight_adj_d1` | 새로운 최적해 발견 시 가중치 조정 | Adaptive |
| `adaptive_weight_adj_d2` | 현재 솔루션보다 개선 시 가중치 조정 | Adaptive |
| `adaptive_weight_adj_d3` | 수용된 솔루션에 대한 가중치 조정 | Adaptive |
| `adaptive_weight_dacay_r` | 가중치 감쇠 비율 | Adaptive |

**Adaptive Weight**: 각 휴리스틱의 성능에 따라 동적으로 선택 확률을 조정

#### 기타 파라미터

| 파라미터 | 설명 | 분류 |
|---------|------|-----|
| `nb_iterations` | 반복 횟수 | 실행 제어 |
| `time_limit` | 시간 제한 (초) | 실행 제어 |
| `thread_count` | 스레드 수 | 병렬 처리 |
| `insertion_objective_noise_n` | 삽입 시 노이즈 추가 | Insertion |
| `removal_req_iteration_control_e` | 제거 요청 반복 제어 | Removal |
| `delaytime_penalty` | 지연 시간 페널티 | 목적함수 |
| `waittime_penalty` | 대기 시간 페널티 | 목적함수 |
| `enable_missing_solution` | 일부 배정 실패 허용 | 제약 완화 |
| `skip_remove_route` | 전체 경로 제거 스킵 | 휴리스틱 제어 |

### 2.3 구분 여부 결론

**ALNS와 LNS는 코드상에서 구분되지 않습니다.**

- 프로젝트 이름과 함수명은 "LNS"를 사용
- 실제 알고리즘 구현은 "ALNS" (적응형 메커니즘 포함)
- **이유**: LNS는 ALNS의 기본 프레임워크이며, ALNS는 LNS에 adaptive weight mechanism을 추가한 확장 버전
- **실무적 관점**: 두 용어가 혼용되어 사용되며, ALNS가 더 포괄적인 개념

---

## 3. 동적 라우팅 알고리즘 상세 분석

### 3.1 문제 정의

**PDPTW (Pickup and Delivery Problem with Time Windows)**

- **Pickup**: 승객 탑승 지점
- **Delivery**: 승객 하차 지점
- **Time Windows**: 각 지점의 도착 가능 시간 범위
- **Capacity**: 차량 용량 제약

### 3.2 알고리즘 호출 흐름

```
[ModRequest 수신]
    ↓
[queryCostMatrix] - 거리/시간 매트릭스 계산
    ├─ queryOsrmCost (OSRM 라우팅 엔진)
    └─ queryValhallaCost (Valhalla 라우팅 엔진)
    ↓
[calcCost] - 최적화 타입에 따른 비용 계산
    ├─ Time (시간 최적화)
    ├─ Distance (거리 최적화)
    └─ Co2 (CO2 배출량 최적화)
    ↓
[loadToModState] - 문제 상태 초기화
    ├─ 차량 상태
    ├─ 수요 정보
    ├─ 시간 제약
    └─ 고정 배정
    ↓
[solve_lns_pdptw] ← **핵심 알고리즘 호출**
    ├─ Removal 휴리스틱 (Shaw, Worst)
    ├─ Insertion 휴리스틱
    ├─ Simulated Annealing
    └─ Adaptive Weight Adjustment
    ↓
[Solution] - 최적화 결과 반환
```

**소스 위치**: src/lnsModRoute.cc:355-464

### 3.3 호출되는 알고리즘 컴포넌트

#### 1. Removal (Destroy) 휴리스틱

**Shaw Removal** (관련성 기반)
- 거리, 시간, 용량 관점에서 유사한 요청들을 함께 제거
- 파라미터: `shaw_phi_distance`, `shaw_chi_time`, `shaw_psi_capacity`, `shaw_removal_p`
- 목적: 유사한 특성을 가진 요청들을 재배치하여 더 나은 그룹핑 발견

**Worst Removal** (비용 기반)
- 현재 솔루션에서 가장 비용이 높은 요청들을 제거
- 파라미터: `worst_removal_p`
- 목적: 비효율적인 배정을 제거하여 개선 기회 창출

#### 2. Insertion (Repair) 휴리스틱

- 제거된 요청들을 다시 경로에 삽입
- `insertion_objective_noise_n`: 탐색 다양성을 위한 노이즈 추가
- 제약 조건 고려:
  - Time Windows (earliestArrival, latestArrival, acceptableArrival)
  - Capacity (vehicleCapacities)
  - Pickup-Delivery 순서 (pickupSibling, deliverySibling)
  - Fixed Assignment (fixedAssignment)

#### 3. Acceptance Criterion

**Simulated Annealing**
- 초기 온도: `simulated_annealing_start_temp_control_w`
- 냉각 비율: `simulated_annealing_cooling_rate_c`
- 더 나쁜 솔루션도 확률적으로 수용하여 지역 최적해 탈출

#### 4. Adaptive Weight Mechanism (ALNS의 핵심)

```
가중치 조정 전략:
- d1: 새로운 전역 최적해 발견 시
- d2: 현재 솔루션보다 개선 시
- d3: 수용된 솔루션에 대해
- r: 시간에 따른 가중치 감쇠
```

각 휴리스틱의 성능에 따라 선택 확률을 동적으로 조정하여 효과적인 휴리스틱을 더 자주 사용

### 3.4 최적화 타입 (src/lnsModRoute.cc:133-158)

```cpp
std::vector<int64_t> calcCost(..., OptimizeType optimizeType)
{
    if (optimizeType == OptimizeType::Time) {
        return timeMatrix;  // 시간 최소화
    } else if (optimizeType == OptimizeType::Distance) {
        return distMatrix;  // 거리 최소화
    } else if (optimizeType == OptimizeType::Co2) {
        // CO2 배출량 계산 (속도 기반)
        // velocity = 3.6 * distance / time (km/h)
        // co2_rate = f(velocity) using emission factors
        return co2Matrix;  // CO2 배출량 최소화
    }
}
```

**CO2 배출량 계산식**:
- 속도 < 64.7 km/h: `co2_rate = 4317.2386 * velocity^(-0.5049)`
- 속도 >= 64.7 km/h: `co2_rate = 0.1829 * velocity^2 - 29.8145 * velocity + 1670.8962`
- 출처: 2021년 승인 국가 온실가스 배출·흡수계수

### 3.5 동적 요소 지원

1. **Onboard Demands**: 이미 차량에 탑승한 승객
2. **Onboard Waiting Demands**: 배정되었으나 아직 픽업 전인 승객
3. **New Demands**: 새로운 요청
4. **Assigned Routes**: 기존 배정 경로 (초기 솔루션으로 사용)

### 3.6 다중 솔루션 생성 (src/lnsModRoute.cc:423-461)

```cpp
if (modRequest.maxSolutions > 1) {
    // 새로운 수요에 대해 다른 차량 배정 금지
    reflectAssignedForNewDemandRoute(...)
    // 두 번째, 세 번째 솔루션 생성
    solve_lns_pdptw(...)
}
```

목적: 동일한 입력에 대해 여러 대안 솔루션 제공 (최대 `nSolutionLimit`개)

---

## 4. API 구성 분석

### 4.1 API 서버 구성

**웹 프레임워크**: cpp-httplib (헤더 온리 라이브러리)
**포트**: 8080 (기본값, `--port` 옵션으로 변경 가능)
**호스트**: localhost (기본값, `--host` 옵션으로 변경 가능)

### 4.2 REST API 엔드포인트

#### 4.2.1 POST /api/v1/optimize

**기능**: 차량 라우팅 최적화
**Request Body**: `ModRequest` (JSON)
**Response**: `OptimizeResponse` (JSON)

**Request 구조**:
```json
{
  "vehicle_locs": [...],           // 차량 위치 정보
  "onboard_demands": [...],        // 탑승 중인 승객
  "onboard_waiting_demands": [...],// 배정되었으나 픽업 전
  "new_demands": [...],            // 새로운 요청
  "assigned": [...],               // 기존 배정 경로
  "optimize_type": "Time|Distance|Co2",
  "max_solution_number": 0,        // 솔루션 개수 (0=1개)
  "loc_hash": "...",               // 캐시 키
  "date_time": "2026-01-20T10:30", // Valhalla 트래픽 기반 라우팅
  "max_duration": 7200             // 최대 시간 (초)
}
```

**Response 구조**:
```json
{
  "status": 0,
  "results": [
    {
      "vehicle_routes": [...],   // 차량별 경로
      "missing": [...],          // 배정 실패한 요청
      "unacceptables": [...],    // 시간 제약 위반 요청
      "total_distance": 9695.0,  // 총 거리 (m)
      "total_time": 1388.0       // 총 시간 (s)
    }
  ]
}
```

**소스 위치**: src/main.cc:263-278

#### 4.2.2 GET /api/v1/reset

**기능**: 라우팅 엔진 캐시 리셋
**Response**: `{"status": 0}`

**동작**:
- OSRM: `queryCostOsrmReset()`
- Valhalla: `queryCostValhallaReset()`

**소스 위치**: src/main.cc:279-292

#### 4.2.3 PUT /api/v1/cache

**기능**: Station 간 거리/시간 캐시 파일 로딩
**Request Body**: `{"key": "cache_2.csv"}`
**Response**: `{"status": 0}`

**캐시 파일 형식**:
```
FromStationID ToStationID Distance(m) Time(s)
2800302 2800302 0 0
2800302 2800321 6812 521
2800302 2800322 6791 481
```

**사용 시나리오**:
- 정거장 기반 MOD 서비스
- 반복적인 라우팅 계산 회피
- station_id 필드가 있는 요청에만 적용

**소스 위치**: src/main.cc:293-305

#### 4.2.4 DELETE /api/v1/cache

**기능**: 로딩된 Station 캐시 삭제
**Response**: `{"status": 0}`

**소스 위치**: src/main.cc:306-315

#### 4.2.5 GET /api/v1/health

**기능**: 헬스 체크
**Response**: `{"status": 0}`

**용도**:
- 쿠버네티스 liveness/readiness probe
- 로드 밸런서 헬스 체크

**소스 위치**: src/main.cc:316-318

#### 4.2.6 GET /api/v1/openapi

**기능**: OpenAPI 3.0 스펙 반환
**Response**: YAML 형식의 API 스펙

**파일**: lnsmodroute.yaml
**소스 위치**: src/main.cc:325-340

#### 4.2.7 GET /api/v1/quit (디버그 빌드만)

**기능**: 서버 종료
**조건**: `#ifndef NDEBUG`

**소스 위치**: src/main.cc:319-323

### 4.3 OpenAPI 스펙 정보

**파일**: lnsmodroute.yaml
**버전**: OpenAPI 3.0.0
**프로젝트 버전**: 0.9.7

**주요 스키마**:
- ModRequest
- VehicleLocation
- OnboardDemand
- OnboardWaitingDemand
- NewDemand
- VehicleAssigned
- Location
- OptimizeResponse
- SolutionResponse
- VehicleRoute
- RouteItem

### 4.4 다국어 바인딩

#### Python 바인딩

**파일**: src/py_modroute.cc
**라이브러리**: pybind11 (v2.13.6)
**모듈명**: mod_route

**제공 함수**:
```python
from mod_route import (
    AlgorithmParameters,
    ModRouteConfiguration,
    default_algorithm_parameters,
    default_mod_configuraiton,
    run_optimize,
    clear_cache
)
```

**빌드 방법**:
```bash
cd python
python3 -m build -w -v
```

#### Java 바인딩

**파일**: src/jni_modroute.cc
**기술**: JNI (Java Native Interface)
**패키지**: com.ciel.microservices.dispatch_engine_service.mod_route

**클래스**:
- ModRouteEngine
- ModRequest
- AlgorithmParameters
- ModRouteConfiguration
- ModDispatchSolution

**빌드 방법**:
```bash
cd java
mvn clean install
```

### 4.5 서비스 등록 (Eureka)

**지원 여부**: ✅ 지원
**파라미터**:
- `--eureka-url`: Eureka 서버 URL
- `--eureka-app`: 애플리케이션 이름 (기본값: LNS-DISPATCH-SERVICE)
- `--eureka-host`: 인스턴스 호스트

**동작**:
1. 서버 시작 시 Eureka에 등록
2. 주기적으로 heartbeat 전송
3. 서버 종료 시 등록 해제

**소스 위치**: src/main.cc:240, 258-261, 343-347

### 4.6 실행 옵션

| 옵션 | 설명 | 기본값 |
|-----|-----|-------|
| `--port` | 서버 포트 | 8080 |
| `--host` | 서버 호스트 | localhost |
| `--route-path` | 라우팅 엔진 URL | http://localhost:8002 |
| `--route-type` | 라우팅 엔진 타입 (OSRM/VALHALLA) | VALHALLA |
| `--route-tasks` | 라우팅 작업 스레드 수 | 4 |
| `--max-duration` | 최대 시간 (초) | 7200 |
| `--service-time` | 서비스 시간 (초) | 10 |
| `--bypass-ratio` | 우회 비율 (%) | 100 |
| `--acceptable-buffer` | 허용 버퍼 (초) | 600 |
| `--cache-expiration-time` | 캐시 만료 시간 (초) | 3600 |
| `--delaytime-penalty` | 지연 페널티 | 10.0 |
| `--waittime-penalty` | 대기 페널티 | 0.0 |
| `--log-request` | 요청/응답 로깅 | false |
| `--log-http` | HTTP 액세스 로깅 | false |
| `--cache-directory` | 캐시 디렉토리 | "" |
| `--init-cache-key` | 초기 캐시 키 | "" |
| `--max-solution-limit` | 최대 솔루션 수 | 3 |
| `--eureka-app` | Eureka 앱 이름 | LNS-DISPATCH-SERVICE |
| `--eureka-url` | Eureka URL | "" |
| `--eureka-host` | Eureka 호스트 | localhost |

---

## 5. 아키텍처 분석

### 5.1 전체 구조

```
┌─────────────────────────────────────────────────────┐
│                  Client Layer                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐          │
│  │   REST   │  │  Python  │  │   Java   │          │
│  │   API    │  │  Binding │  │  Binding │          │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘          │
└────────┼─────────────┼─────────────┼───────────────┘
         │             │             │
┌────────▼─────────────▼─────────────▼───────────────┐
│              API Gateway Layer                      │
│  ┌─────────────────────────────────────────────┐   │
│  │         cpp-httplib (HTTP Server)           │   │
│  │  /api/v1/optimize | /cache | /health        │   │
│  └───────────────────┬─────────────────────────┘   │
└──────────────────────┼─────────────────────────────┘
                       │
┌──────────────────────▼─────────────────────────────┐
│            Business Logic Layer                     │
│  ┌─────────────────────────────────────────────┐   │
│  │         lnsModRoute (Orchestrator)          │   │
│  │  - Request parsing                          │   │
│  │  - Cost matrix query                        │   │
│  │  - State initialization                     │   │
│  │  - Solution formatting                      │   │
│  └───────┬──────────────────────┬──────────────┘   │
└──────────┼──────────────────────┼──────────────────┘
           │                      │
    ┌──────▼──────┐        ┌─────▼──────────────┐
    │   Caching   │        │  Routing Engines   │
    │  ┌────────┐ │        │  ┌──────────────┐  │
    │  │Cost    │ │        │  │   Valhalla   │  │
    │  │Cache   │ │        │  │   (default)  │  │
    │  └────────┘ │        │  └──────────────┘  │
    │  ┌────────┐ │        │  ┌──────────────┐  │
    │  │Station │ │        │  │     OSRM     │  │
    │  │Cache   │ │        │  └──────────────┘  │
    │  └────────┘ │        └────────────────────┘
    └─────────────┘
           │
┌──────────▼─────────────────────────────────────────┐
│          Algorithm Core Layer                       │
│  ┌─────────────────────────────────────────────┐   │
│  │        alns-pdp Library (External)          │   │
│  │  ┌─────────────────────────────────────┐   │   │
│  │  │      solve_lns_pdptw Function       │   │   │
│  │  ├─────────────────────────────────────┤   │   │
│  │  │  ┌──────────────────────────────┐   │   │   │
│  │  │  │  Removal Heuristics          │   │   │   │
│  │  │  │  - Shaw Removal              │   │   │   │
│  │  │  │  - Worst Removal             │   │   │   │
│  │  │  └──────────────────────────────┘   │   │   │
│  │  │  ┌──────────────────────────────┐   │   │   │
│  │  │  │  Insertion Heuristics        │   │   │   │
│  │  │  └──────────────────────────────┘   │   │   │
│  │  │  ┌──────────────────────────────┐   │   │   │
│  │  │  │  Simulated Annealing         │   │   │   │
│  │  │  └──────────────────────────────┘   │   │   │
│  │  │  ┌──────────────────────────────┐   │   │   │
│  │  │  │  Adaptive Weight Mechanism   │   │   │   │
│  │  │  └──────────────────────────────┘   │   │   │
│  │  └─────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────┘
```

### 5.2 주요 컴포넌트

#### 5.2.1 lnsModRoute (src/lnsModRoute.cc)

**역할**: 최적화 프로세스 오케스트레이터

**주요 함수**:
1. `queryCostMatrix()`: 라우팅 엔진에서 거리/시간 매트릭스 획득
2. `calcCost()`: 최적화 타입에 따른 비용 계산
3. `loadToModState()`: PDPTW 문제 상태 초기화
4. `runOptimize()`: 전체 최적화 프로세스 실행
5. `makeNodeToModRoute()`: 노드 인덱스와 실제 경로 매핑

#### 5.2.2 Cost Cache (src/costCache.cc)

**역할**: 라우팅 비용 캐싱

**캐싱 전략**:
- **In-Memory Cache**: LRU 기반, 만료 시간 설정 가능
- **Station Cache**: CSV 파일 기반 정거장 간 거리/시간 사전 계산

**캐시 키**: `{from_lat}_{from_lng}_{to_lat}_{to_lng}_{direction}`

#### 5.2.3 Query Engines

**OSRM** (src/queryOsrmCost.cc)
- Open Source Routing Machine
- HTTP API 호출: `/table/v1/driving/{coordinates}`

**Valhalla** (src/queryValhallaCost.cc)
- Mapbox 라우팅 엔진
- HTTP API 호출: `/sources_to_targets`
- 트래픽 기반 라우팅 지원 (date_time 파라미터)

**Thread Pool**: 병렬 라우팅 쿼리 처리 (src/threadPool.cc)

#### 5.2.4 Request Logger (src/requestLogger.cc)

**로깅 대상**:
- 요청 파라미터 (노드, 차량, 비용 매트릭스)
- 응답 결과 (경로, 도착 시간)

**파일 형식**: `./logs/YYMMDD_HHMMSS_{request|response}.log`

### 5.3 데이터 흐름

```
ModRequest (JSON)
    ↓
parseRequest() [main_utility.cc]
    ↓
ModRequest (C++ struct)
    ↓
queryCostMatrix() [lnsModRoute.cc]
    ├─ Cache Hit? → Use cached cost
    └─ Cache Miss → Query routing engine
         ├─ OSRM
         └─ Valhalla
    ↓
Cost Matrix (distance, time)
    ↓
calcCost() → Cost Matrix (optimized for type)
    ↓
loadToModState() → CModState
    ├─ demands[]
    ├─ serviceTimes[]
    ├─ earliestArrival[]
    ├─ latestArrival[]
    ├─ acceptableArrival[]
    ├─ pickupSibling[]
    ├─ deliverySibling[]
    ├─ fixedAssignment[]
    ├─ vehicleCapacities[]
    ├─ startDepots[]
    ├─ endDepots[]
    └─ initialSolution[]
    ↓
solve_lns_pdptw() [alns-pdp library]
    ↓
Solution*
    ├─ routes[]
    ├─ vehicles[]
    ├─ missing[]
    ├─ unacceptables[]
    ├─ distance
    └─ time
    ↓
makeDispatchResponse() [main_utility.cc]
    ↓
JSON Response
```

### 5.4 빌드 시스템

**CMake 구조**:

```cmake
lnsmodroute (executable)
  ├─ src/main.cc
  ├─ src/lnsModRoute.cc
  ├─ src/costCache.cc
  ├─ src/queryOsrmCost.cc
  ├─ src/queryValhallaCost.cc
  └─ liblnspdptw_static.a

mod_route (Python module)
  ├─ src/py_modroute.cc
  ├─ src/lib_modroute.cc
  ├─ pybind11
  └─ liblnspdptw_static.a

modroute_jni (Java JNI library)
  ├─ src/jni_modroute.cc
  ├─ src/lib_modroute.cc
  ├─ JNI headers
  └─ liblnspdptw_static.a
```

**외부 의존성**:
- lnspdptw (ALNS PDPTW 솔버)
- googletest (테스트 프레임워크)
- pybind11 (Python 바인딩)
- cpp-httplib (HTTP 서버, 헤더 온리)
- gason (JSON 파싱, 헤더 온리)

### 5.5 Docker 배포

**Dockerfile** (docker/Dockerfile):

```dockerfile
# Stage 1: Builder
FROM alpine:latest AS builder
RUN apk add git cmake build-base openssh

# Clone and build alns-pdp
RUN git clone git@github.com:cielmobilityDev/alns-pdp.git
RUN cmake -S . -B build && cmake --build build && cmake --install build

# Clone and build lnsModRoute
RUN git clone git@github.com:cielmobilityDev/lnsModRoute.git
RUN cmake -S . -B build && cmake --build build && cmake --install build

# Stage 2: Runtime
FROM alpine:latest AS runstage
RUN apk add libstdc++ libgcc
COPY --from=builder /usr/local /usr/local
```

**이미지 빌드**:
```bash
cd docker
DOCKER_BUILDKIT=1 docker build --ssh default -t ciel/lnsmodroute:0.0.1-SNAPSHOT .
```

**컨테이너 실행**:
```bash
docker run -d --name lnsmodroute_container \
  --network my_network \
  -v "${PWD}/test:/data" \
  -p 8080:8080 \
  ciel/lnsmodroute:0.0.1-SNAPSHOT \
  lnsmodroute --host 0.0.0.0 \
              --route-type VALHALLA \
              --route-path http://valhalla:8002 \
              --bypass-time 600 \
              --service-time 5 \
              --max-duration 2400 \
              --cache-directory /data
```

---

## 6. 결론 및 권장사항

### 6.1 주요 발견 사항 요약

#### 1. ALNS vs LNS 구분

**발견**:
- 프로젝트명과 함수명은 "LNS"를 사용
- 실제 알고리즘은 "ALNS" (Adaptive LNS) 구현
- 구분되지 않고 혼용됨

**권장사항**:
- ✅ 현재 상태 유지 (실무에서 두 용어가 혼용되므로 문제 없음)
- 📝 문서에 ALNS 알고리즘임을 명시
- 🔧 함수명을 `solve_alns_pdptw`로 변경 고려 (선택사항)

#### 2. 동적 라우팅 알고리즘

**호출 알고리즘**:
- `solve_lns_pdptw` (alns-pdp 라이브러리)
- PDPTW (Pickup and Delivery Problem with Time Windows)

**알고리즘 컴포넌트**:
1. **Destruction**: Shaw Removal, Worst Removal
2. **Repair**: Greedy Insertion with noise
3. **Acceptance**: Simulated Annealing
4. **Adaptive**: Weight adjustment based on performance

**장점**:
- ✅ 동적 시나리오 지원 (onboard, waiting, new demands)
- ✅ 다양한 제약 조건 처리 (time windows, capacity, fixed assignment)
- ✅ 다중 목적함수 지원 (Time, Distance, CO2)
- ✅ 다중 솔루션 생성 가능

**개선 권장사항**:
- 📊 알고리즘 파라미터 튜닝 가이드 문서화
- 🧪 벤치마크 데이터셋 기반 성능 평가
- 📈 실시간 모니터링 메트릭 추가 (반복 횟수, 개선 비율 등)

#### 3. API 구성

**API 완성도**: ✅ **매우 우수**

**강점**:
- ✅ RESTful 설계 원칙 준수
- ✅ OpenAPI 3.0 스펙 제공
- ✅ Python/Java 바인딩 제공
- ✅ Eureka 서비스 등록 지원
- ✅ 캐싱 메커니즘 (In-Memory + Station Cache)
- ✅ 다양한 실행 옵션

**개선 권장사항**:
- 🔐 인증/인가 메커니즘 추가 (현재 없음)
- 📝 Rate Limiting 고려
- 📊 메트릭 엔드포인트 추가 (Prometheus 형식)
- 🔍 로그 집계 (ELK Stack 연동)
- 🧪 Health Check 상세화 (Valhalla/OSRM 연결 상태 포함)

### 6.2 아키텍처 평가

**점수**: ⭐⭐⭐⭐☆ (4.5/5)

**강점**:
- ✅ 명확한 레이어 분리 (API, Business Logic, Algorithm Core)
- ✅ 외부 라이브러리 의존성 관리 (alns-pdp)
- ✅ 멀티플랫폼 지원 (Linux, Windows, Docker)
- ✅ 병렬 처리 (Thread Pool for routing queries)
- ✅ 효율적인 캐싱 전략

**개선 권장사항**:
- 🔧 설정 파일 지원 (YAML/JSON)
- 🧩 플러그인 아키텍처 (새로운 라우팅 엔진 추가 용이)
- 🔄 비동기 API 엔드포인트 (긴 최적화 작업용)
- 📦 Kubernetes Helm Chart 제공

### 6.3 운영 환경 권장사항

#### 프로덕션 배포

**필수 사항**:
```bash
lnsmodroute \
  --host 0.0.0.0 \
  --port 8080 \
  --route-type VALHALLA \
  --route-path http://valhalla:8002 \
  --route-tasks 8 \                     # CPU 코어 수에 맞춤
  --max-duration 3600 \                 # 1시간
  --delaytime-penalty 10.0 \
  --waittime-penalty 0.0 \
  --cache-directory /data/cache \
  --cache-expiration-time 3600 \
  --log-http \                          # HTTP 로깅 활성화
  --eureka-url http://eureka:8761 \
  --eureka-app LNS-DISPATCH-SERVICE \
  --eureka-host $(hostname)
```

**모니터링**:
- Health check: `GET /api/v1/health` (15초 간격)
- 응답 시간 모니터링 (목표: p95 < 5초)
- 캐시 히트율 모니터링

**스케일링**:
- Horizontal: Eureka + Load Balancer
- Vertical: `--route-tasks` 증가 (라우팅 쿼리 병렬화)

#### 성능 튜닝

**알고리즘 파라미터** (main.cc:142-149):
```cpp
parameter.nb_iterations = 5000;       // 반복 횟수 (↑ = 품질↑, 시간↑)
parameter.time_limit = 1;             // 시간 제한 (초)
parameter.delaytime_penalty = 10.0;   // 지연 페널티 (↑ = 정시성↑)
parameter.waittime_penalty = 0.0;     // 대기 페널티
```

**권장 설정**:
- 소규모 (차량 < 10, 수요 < 50): `nb_iterations=3000`, `time_limit=1`
- 중규모 (차량 10-30, 수요 50-200): `nb_iterations=5000`, `time_limit=2`
- 대규모 (차량 > 30, 수요 > 200): `nb_iterations=10000`, `time_limit=5`

### 6.4 보안 고려사항

**현재 상태**: ⚠️ **인증 없음**

**권장사항**:
1. **API Key 인증** 추가
   ```cpp
   svr.Post("/api/v1/optimize", [&](const Request &req, Response &res) {
       std::string api_key = req.get_header_value("X-API-Key");
       if (!validateApiKey(api_key)) {
           res.status = 401;
           return;
       }
       // ... 기존 코드
   });
   ```

2. **TLS/HTTPS** 지원
   ```cpp
   #define CPPHTTPLIB_OPENSSL_SUPPORT
   httplib::SSLServer svr(cert_path, key_path);
   ```

3. **Rate Limiting**
   - IP 기반 요청 제한
   - API Key별 쿼터

4. **입력 검증 강화**
   - JSON 스키마 검증
   - 좌표 범위 확인
   - 배열 크기 제한

### 6.5 테스트 권장사항

**단위 테스트** (tests/ 디렉토리):
- ✅ 일부 테스트 존재 (test_query.cc, test_costCache.cc)
- 📝 커버리지 확대 필요

**통합 테스트**:
- API 엔드포인트 테스트
- 라우팅 엔진 연동 테스트
- 캐싱 동작 테스트

**부하 테스트**:
- Apache JMeter / Locust 사용
- 목표: 100 req/s, p95 < 5s

### 6.6 문서화 권장사항

**추가 필요 문서**:
1. **알고리즘 파라미터 가이드**
   - 각 파라미터의 의미와 영향
   - 시나리오별 권장 설정

2. **운영 가이드**
   - 배포 절차
   - 모니터링 메트릭
   - 트러블슈팅

3. **API 사용 예제**
   - cURL 예제
   - Python 클라이언트 예제
   - Java 클라이언트 예제

4. **성능 튜닝 가이드**
   - 벤치마크 결과
   - 병목 지점 분석
   - 최적화 팁

---

## 부록

### A. 파일 구조

```
lnsModRoute-260120/
├── CMakeLists.txt                     # CMake 빌드 설정
├── README.md                          # 프로젝트 README
├── lnsmodroute.yaml                   # OpenAPI 3.0 스펙
├── include/                           # 헤더 파일
│   ├── lnsModRoute.h                  # 메인 헤더
│   ├── lib_modroute.h                 # 라이브러리 인터페이스
│   ├── mod_parameters.h               # 파라미터 정의
│   ├── costCache.h                    # 캐시 헤더
│   ├── queryOsrmCost.h                # OSRM 쿼리
│   ├── queryValhallaCost.h            # Valhalla 쿼리
│   ├── requestLogger.h                # 로거 헤더
│   ├── threadPool.h                   # 스레드 풀
│   ├── main_utility.h                 # 유틸리티
│   ├── modState.h                     # 상태 관리
│   ├── jni_modroute.h                 # JNI 헤더
│   ├── eurekaClient.h                 # Eureka 클라이언트
│   ├── cpp-httplib/httplib.h          # HTTP 서버
│   └── gason/gason.h                  # JSON 파싱
├── src/                               # 소스 파일
│   ├── main.cc                        # 메인 엔트리포인트
│   ├── lnsModRoute.cc                 # 최적화 로직
│   ├── lib_modroute.cc                # 라이브러리 구현
│   ├── main_utility.cc                # 유틸리티 구현
│   ├── costCache.cc                   # 캐시 구현
│   ├── queryOsrmCost.cc               # OSRM 구현
│   ├── queryValhallaCost.cc           # Valhalla 구현
│   ├── threadPool.cc                  # 스레드 풀 구현
│   ├── requestLogger.cc               # 로거 구현
│   ├── py_modroute.cc                 # Python 바인딩
│   ├── jni_modroute.cc                # JNI 바인딩
│   └── test_modroute.cc               # 테스트
├── tests/                             # 테스트 파일
│   ├── test_query.cc
│   ├── test_costCache.cc
│   └── test_utility.cc
├── python/                            # Python 패키지
│   ├── setup.py
│   ├── mod_route/__init__.py
│   └── README.md
├── java/                              # Java 패키지
│   ├── pom.xml
│   ├── src/main/java/.../mod_route/
│   └── README.md
└── docker/                            # Docker 설정
    ├── Dockerfile
    └── ssh_config
```

### B. 알고리즘 파라미터 상세 설명

| 파라미터 | 타입 | 기본값 | 설명 |
|---------|------|-------|------|
| nb_iterations | int | 5000 | ALNS 반복 횟수 (↑ 품질, ↑ 시간) |
| time_limit | int | 1 | 최대 실행 시간 (초) |
| thread_count | int | 1 | 병렬 스레드 수 |
| shaw_phi_distance | double | 9 | Shaw removal 거리 가중치 |
| shaw_chi_time | double | 3 | Shaw removal 시간 가중치 |
| shaw_psi_capacity | double | 2 | Shaw removal 용량 가중치 |
| shaw_removal_p | int | 6 | Shaw removal 파라미터 |
| worst_removal_p | int | 3 | Worst removal 파라미터 |
| simulated_annealing_start_temp_control_w | double | 0.05 | SA 초기 온도 |
| simulated_annealing_cooling_rate_c | double | 0.99975 | SA 냉각 비율 |
| adaptive_weight_adj_d1 | double | 33 | 새 최적해 가중치 |
| adaptive_weight_adj_d2 | double | 9 | 개선 가중치 |
| adaptive_weight_adj_d3 | double | 13 | 수용 가중치 |
| adaptive_weight_dacay_r | double | 0.1 | 가중치 감쇠 |
| insertion_objective_noise_n | double | 0.025 | 삽입 노이즈 |
| removal_req_iteration_control_e | double | 0.4 | 제거 제어 |
| delaytime_penalty | double | 10.0 | 지연 페널티 |
| waittime_penalty | double | 0.0 | 대기 페널티 |
| seed | int | 1234 | 난수 시드 |
| enable_missing_solution | bool | true | 일부 배정 실패 허용 |
| skip_remove_route | bool | false | 경로 제거 스킵 |
| unfeasible_delaytime | int | 0 | 허용 불가 지연 시간 |

### C. API 요청 예제

#### cURL

```bash
# 최적화 요청
curl -X POST http://localhost:8080/api/v1/optimize \
  -H 'Content-Type: application/json' \
  -d @request.json

# 캐시 로딩
curl -X PUT http://localhost:8080/api/v1/cache \
  -H 'Content-Type: application/json' \
  -d '{"key":"cache_2.csv"}'

# 헬스 체크
curl http://localhost:8080/api/v1/health
```

#### Python

```python
import requests

# 최적화 요청
response = requests.post('http://localhost:8080/api/v1/optimize',
    json={
        'vehicle_locs': [...],
        'new_demands': [...],
        'optimize_type': 'Time'
    }
)
result = response.json()
```

#### Java

```java
import com.ciel.microservices.dispatch_engine_service.mod_route.*;

ModRouteEngine engine = new ModRouteEngine();
AlgorithmParameters ap = engine.default_algorithm_parameters();
ModRouteConfiguration conf = engine.default_mod_configuraiton();

List<ModDispatchSolution> solutions = engine.run_optimize(
    modRequest, routePath, RouteType.VALHALLA,
    4, cachePath, ap, conf
);
```

### D. 성능 벤치마크 (예시)

| 시나리오 | 차량 수 | 수요 수 | 반복 횟수 | 실행 시간 | 총 거리 | 배정률 |
|---------|--------|--------|----------|----------|---------|-------|
| 소규모 | 5 | 20 | 3000 | 0.8초 | 45.2km | 100% |
| 중규모 | 15 | 80 | 5000 | 2.3초 | 128.7km | 98% |
| 대규모 | 30 | 150 | 5000 | 4.9초 | 287.3km | 95% |

---

**보고서 작성**: Claude Code
**기반 코드 분석**: lnsModRoute v0.9.7
**분석 일자**: 2026-01-20
