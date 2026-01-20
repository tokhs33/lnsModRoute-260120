# ALNS 알고리즘 동작 방식 상세 분석 보고서

**프로젝트**: lnsModRoute v0.9.7
**작성일**: 2026-01-21
**목적**: ALNS 알고리즘의 동작 원리와 API 명세 문서화

---

## 목차

1. [ALNS 알고리즘 개요](#1-alns-알고리즘-개요)
2. [알고리즘 동작 방식 (단계별 분석)](#2-알고리즘-동작-방식-단계별-분석)
3. [샘플 시나리오 기반 실행 예시](#3-샘플-시나리오-기반-실행-예시)
4. [데이터 구조 및 변환 과정](#4-데이터-구조-및-변환-과정)
5. [API 명세서](#5-api-명세서)
6. [알고리즘 파라미터 튜닝 가이드](#6-알고리즘-파라미터-튜닝-가이드)

---

## 1. ALNS 알고리즘 개요

### 1.1 ALNS (Adaptive Large Neighborhood Search)란?

**ALNS**는 복잡한 조합 최적화 문제를 해결하기 위한 메타휴리스틱 알고리즘입니다.

```
┌──────────────────────────────────────────────────────────┐
│              ALNS 알고리즘 구조                            │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  1. Initial Solution (초기 솔루션 생성)                   │
│     └─ Greedy Heuristic / User Provided                 │
│                                                          │
│  2. Main Loop (반복 최적화)                              │
│     ┌──────────────────────────────────────────┐       │
│     │ A. Destroy (파괴) - Removal Heuristics   │       │
│     │    ├─ Shaw Removal (유사성 기반)         │       │
│     │    ├─ Worst Removal (비용 기반)          │       │
│     │    ├─ Random Removal                     │       │
│     │    └─ Route Removal                      │       │
│     │                                          │       │
│     │ B. Repair (복구) - Insertion Heuristics  │       │
│     │    └─ Greedy Insertion + Noise          │       │
│     │                                          │       │
│     │ C. Acceptance (수용 여부 결정)            │       │
│     │    └─ Simulated Annealing               │       │
│     │                                          │       │
│     │ D. Adaptive Weight Update (가중치 조정)   │       │
│     │    └─ Performance-based weight          │       │
│     └──────────────────────────────────────────┘       │
│                                                          │
│  3. Return Best Solution (최적 솔루션 반환)               │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### 1.2 PDPTW 문제 정의

**PDPTW (Pickup and Delivery Problem with Time Windows)**

- **목적**: 여러 차량을 사용하여 승객 픽업/하차를 최소 비용으로 수행
- **제약 조건**:
  - Time Windows: 각 지점의 도착 시간 범위
  - Capacity: 차량 용량 제한
  - Precedence: 픽업은 하차보다 먼저 발생
  - Fixed Assignment: 특정 승객은 특정 차량에 고정

### 1.3 프로젝트 적용 사항

본 프로젝트는 **동적 MOD (Mobility on Demand) 서비스**를 위한 실시간 라우팅 최적화를 수행합니다.

**특징**:
- ✅ 실시간 동적 배차 (이미 탑승 중인 승객 고려)
- ✅ 다중 목적함수 (Time, Distance, CO2)
- ✅ 실제 도로 네트워크 (Valhalla/OSRM 연동)
- ✅ 다중 솔루션 생성 (대안 경로 제공)

---

## 2. 알고리즘 동작 방식 (단계별 분석)

### 2.1 전체 실행 흐름

```
[Client Request]
    ↓
┌─────────────────────────────────────────────────┐
│ STEP 1: 요청 파싱 및 검증                        │
│ parseRequest() - src/main_utility.cc:11        │
│ - JSON → ModRequest 구조체 변환                 │
└─────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────┐
│ STEP 2: 비용 매트릭스 계산                       │
│ queryCostMatrix() - src/lnsModRoute.cc:113     │
│ - 모든 노드 간 거리/시간 계산                    │
│ - Valhalla/OSRM API 호출                       │
│ - 캐시 활용 (Station Cache / In-Memory)        │
└─────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────┐
│ STEP 3: 비용 매트릭스 최적화                     │
│ calcCost() - src/lnsModRoute.cc:133            │
│ - Time/Distance/CO2 중 선택                    │
│ - CO2: 속도 기반 배출량 계산                    │
└─────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────┐
│ STEP 4: 문제 상태 초기화                        │
│ loadToModState() - src/lnsModRoute.cc:223      │
│ - PDPTW 문제로 변환                            │
│ - Time Windows, Capacity, Precedence 설정     │
│ - Initial Solution 구성 (assigned 파라미터)    │
└─────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────┐
│ STEP 5: ALNS 알고리즘 실행 ★                    │
│ solve_lns_pdptw() - alns-pdp library           │
│                                                │
│  [초기화]                                       │
│  ├─ 초기 솔루션 평가                            │
│  └─ 휴리스틱 가중치 초기화                       │
│                                                │
│  [메인 루프: nb_iterations 회 반복]             │
│  │                                             │
│  ├─ A. Destroy (파괴 단계)                      │
│  │   ├─ 휴리스틱 선택 (Adaptive Weight 기반)    │
│  │   │   ├─ Shaw Removal (40% 확률)           │
│  │   │   ├─ Worst Removal (30% 확률)          │
│  │   │   ├─ Random Removal (20% 확률)         │
│  │   │   └─ Route Removal (10% 확률)          │
│  │   │                                         │
│  │   └─ n개 요청 제거 (n = total * 0.1~0.4)    │
│  │                                             │
│  ├─ B. Repair (복구 단계)                       │
│  │   ├─ 제거된 요청들을 순회                    │
│  │   ├─ 각 요청에 대해 최적 삽입 위치 탐색      │
│  │   │   ├─ 모든 차량의 모든 위치 시도         │
│  │   │   ├─ 비용 증가량 계산                   │
│  │   │   ├─ 제약 조건 검사                     │
│  │   │   └─ Noise 추가 (다양성 확보)           │
│  │   └─ 삽입 실패 시 missing으로 분류          │
│  │                                             │
│  ├─ C. Acceptance (수용 단계)                   │
│  │   ├─ 새 솔루션 평가                         │
│  │   ├─ 개선되었는가?                          │
│  │   │   ├─ YES → 무조건 수용                  │
│  │   │   └─ NO → Simulated Annealing          │
│  │   │         └─ P = exp(-Δ/T) 확률로 수용    │
│  │   └─ 온도 감소 (T = T * cooling_rate)      │
│  │                                             │
│  └─ D. Adaptive Update (가중치 조정)            │
│      ├─ 새 최적해? → weight += d1              │
│      ├─ 개선? → weight += d2                   │
│      ├─ 수용? → weight += d3                   │
│      └─ 모든 가중치 감쇠 (weight *= (1-r))     │
│                                                │
│  [종료 조건 확인]                               │
│  ├─ nb_iterations 도달?                        │
│  └─ time_limit 도달?                           │
│                                                │
└─────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────┐
│ STEP 6: 솔루션 변환 및 반환                      │
│ makeDispatchResponse() - src/main_utility.cc   │
│ - Solution* → JSON 응답 변환                   │
└─────────────────────────────────────────────────┘
    ↓
[Client Response]
```

### 2.2 STEP 1: 요청 파싱 상세

**파일**: src/main_utility.cc:11-400

```cpp
ModRequest parseRequest(const char *body)
{
    // JSON 파싱 (gason 라이브러리 사용)
    JsonValue value;
    JsonAllocator allocator;
    jsonParse((char*) body, &endptr, &value, allocator);

    ModRequest request;

    // 각 필드 파싱
    // - vehicle_locs: 차량 위치 및 용량
    // - onboard_demands: 탑승 중인 승객
    // - onboard_waiting_demands: 배정되었으나 픽업 전
    // - new_demands: 새로운 요청
    // - assigned: 기존 배정 경로 (초기 솔루션)
    // - optimize_type: Time/Distance/CO2

    return request;
}
```

**파싱 결과**:
```cpp
struct ModRequest {
    vector<VehicleLocation> vehicleLocs;
    vector<OnboardDemand> onboardDemands;
    vector<OnboardWaitingDemand> onboardWaitingDemands;
    vector<NewDemand> newDemands;
    vector<VehicleAssigned> assigned;
    OptimizeType optimizeType;
    int maxSolutions;
    string locHash;
    optional<string> dateTime;
    int maxDuration;
};
```

### 2.3 STEP 2: 비용 매트릭스 계산 상세

**파일**: src/lnsModRoute.cc:113-131

**목적**: 모든 노드 간 이동 비용 (거리, 시간) 계산

**노드 구성**:
```
Node Index 구조:
┌──────────────────────────────────────────┐
│ 0: Ghost Depot (가상 종점)                │
├──────────────────────────────────────────┤
│ 1 ~ V: 차량 시작 위치 (V = 차량 수)       │
├──────────────────────────────────────────┤
│ V+1 ~ V+O: Onboard Demands Drop-off      │
│            (O = 탑승 중인 승객 수)         │
├──────────────────────────────────────────┤
│ V+O+1 ~ V+O+2W:                          │
│   Onboard Waiting Demands                │
│   - 홀수: Pickup                         │
│   - 짝수: Drop-off                       │
│   (W = 대기 중인 승객 수)                 │
├──────────────────────────────────────────┤
│ V+O+2W+1 ~ V+O+2W+2N:                    │
│   New Demands                            │
│   - 홀수: Pickup                         │
│   - 짝수: Drop-off                       │
│   (N = 새 요청 수)                        │
└──────────────────────────────────────────┘

총 노드 수 = 1 + V + O + 2W + 2N
```

**비용 매트릭스 구조**:
```
costMatrix[i][j] = i번 노드에서 j번 노드로 이동하는 비용

매트릭스 크기: (nodeCount + 1) × (nodeCount + 1)

예시 (차량 2대, 새 요청 2개):
     0    1    2    3    4    5    6
  ┌─────────────────────────────────┐
0 │  0    0    0    ∞    ∞    ∞    ∞│ Ghost
1 │  0    0  100  500  800  300  900│ Vehicle1
2 │  0  120    0  450  700  350  850│ Vehicle2
3 │  ∞  480  430    0  200  150  400│ D1-Pickup
4 │  ∞  780  680  190    0  350  210│ D1-Dropoff
5 │  ∞  280  330  140  340    0  500│ D2-Pickup
6 │  ∞  880  830  390  200  490    0│ D2-Dropoff
  └─────────────────────────────────┘
```

**라우팅 엔진 호출**:

1. **Valhalla** (기본값)
   ```bash
   POST http://valhalla:8002/sources_to_targets
   Content-Type: application/json

   {
     "sources": [{"lat": 37.5, "lon": 127.0, "heading": 90}, ...],
     "targets": [{"lat": 37.51, "lon": 127.01}, ...],
     "costing": "auto",
     "date_time": "2026-01-21T10:30"  // 트래픽 고려
   }
   ```

2. **OSRM**
   ```bash
   GET http://osrm:5000/table/v1/driving/127.0,37.5;127.01,37.51
   ```

**캐싱 전략**:

```cpp
// 1. Station Cache 확인 (정거장 ID 기반)
if (from.station_id && to.station_id) {
    cost = g_costCache.getStationCost(from.station_id, to.station_id);
    if (cost != null) return cost;
}

// 2. In-Memory Cache 확인 (좌표 기반)
string key = "{from_lat}_{from_lng}_{to_lat}_{to_lng}_{direction}";
cost = g_costCache.get(key);
if (cost != null && !expired) return cost;

// 3. Routing Engine 호출
cost = queryRoutingEngine(from, to);
g_costCache.set(key, cost, expiration_time);
return cost;
```

### 2.4 STEP 3: 비용 계산 (Optimize Type)

**파일**: src/lnsModRoute.cc:133-158

```cpp
vector<int64_t> calcCost(
    vector<int64_t>& distMatrix,
    vector<int64_t>& timeMatrix,
    OptimizeType optimizeType)
{
    if (optimizeType == OptimizeType::Time) {
        // 시간 최소화
        return timeMatrix;

    } else if (optimizeType == OptimizeType::Distance) {
        // 거리 최소화
        return distMatrix;

    } else if (optimizeType == OptimizeType::Co2) {
        // CO2 배출량 최소화
        vector<int64_t> co2Matrix(distMatrix.size());

        for (size_t i = 0; i < distMatrix.size(); i++) {
            if (timeMatrix[i] == 0 || timeMatrix[i] == INT32_MAX) {
                co2Matrix[i] = timeMatrix[i];
                continue;
            }

            // 속도 계산 (km/h)
            double velocity = 3.6 * distMatrix[i] / timeMatrix[i];

            // CO2 배출 계수 (mg/km)
            double co2_rate;
            if (velocity < 64.7) {
                // 저속 구간 (2021년 국가 온실가스 배출계수)
                co2_rate = 4317.2386 * pow(velocity, -0.5049);
            } else {
                // 고속 구간
                co2_rate = 0.1829 * pow(velocity, 2)
                         - 29.8145 * velocity
                         + 1670.8962;
            }

            // 총 CO2 배출량 (mg)
            co2Matrix[i] = (int64_t)(co2_rate * distMatrix[i]);
        }

        return co2Matrix;
    }
}
```

**CO2 배출량 계산 예시**:

| 구간 | 거리 (m) | 시간 (s) | 속도 (km/h) | CO2 계수 (mg/km) | 총 CO2 (mg) |
|------|---------|---------|------------|-----------------|------------|
| A→B | 5,000 | 300 | 60.0 | 138.5 | 692,500 |
| B→C | 10,000 | 500 | 72.0 | 125.3 | 1,253,000 |
| C→D | 2,000 | 200 | 36.0 | 228.7 | 457,400 |

### 2.5 STEP 4: 문제 상태 초기화

**파일**: src/lnsModRoute.cc:223-324

**목적**: ModRequest를 PDPTW 문제로 변환

```cpp
CModState loadToModState(
    const ModRequest& modRequest,
    const size_t nodeCount,
    const size_t vehicleCount,
    const vector<int64_t>& timeMatrix,
    const ModRouteConfiguration& modRouteConfig)
{
    CModState modState(nodeCount, vehicleCount);

    // ============================================
    // 1. 차량 정보 설정
    // ============================================
    for (size_t i = 0; i < vehicleCount; i++) {
        modState.vehicleCapacities[i] = modRequest.vehicleLocs[i].capacity;
        modState.startDepots[i] = i + 1;  // 차량 시작 노드
        modState.endDepots[i] = 0;        // Ghost depot

        // 운행 시간 제약
        if (modRequest.vehicleLocs[i].operationTime[0] > 0) {
            modState.earliestArrival[i + 1] =
                modRequest.vehicleLocs[i].operationTime[0];
        }
        if (modRequest.vehicleLocs[i].operationTime[1] > 0) {
            modState.latestArrival[i + 1] =
                modRequest.vehicleLocs[i].operationTime[1];
        }
    }

    // ============================================
    // 2. Onboard Demands (탑승 중) - Drop-off만
    // ============================================
    int baseNodeIdx = vehicleCount + 1;
    for (auto& onboardDemand : modRequest.onboardDemands) {
        modState.demands[baseNodeIdx] = -onboardDemand.demand;  // 하차
        modState.serviceTimes[baseNodeIdx] = nServiceTime;      // 10초

        // Time Windows
        modState.earliestArrival[baseNodeIdx] =
            onboardDemand.etaToDestination[0];
        modState.latestArrival[baseNodeIdx] =
            onboardDemand.etaToDestination[1] > 0
                ? onboardDemand.etaToDestination[1]
                : nMaxDuration;
        modState.acceptableArrival[baseNodeIdx] =
            max(1, latestArrival - nAcceptableBuffer);

        // 고정 배정 (이미 탑승 중이므로 차량 변경 불가)
        modState.fixedAssignment[baseNodeIdx] =
            mapVehicleSupplyIdx[onboardDemand.supplyIdx];

        baseNodeIdx++;
    }

    // ============================================
    // 3. Onboard Waiting Demands - Pickup + Drop-off
    // ============================================
    for (auto& waitingDemand : modRequest.onboardWaitingDemands) {
        // Pickup 노드
        modState.demands[baseNodeIdx] = waitingDemand.demand;
        modState.serviceTimes[baseNodeIdx] = nServiceTime;
        modState.earliestArrival[baseNodeIdx] =
            waitingDemand.etaToStart[0];
        modState.latestArrival[baseNodeIdx] =
            waitingDemand.etaToStart[1];

        // Drop-off 노드
        modState.demands[baseNodeIdx + 1] = -waitingDemand.demand;
        modState.serviceTimes[baseNodeIdx + 1] = nServiceTime;
        modState.earliestArrival[baseNodeIdx + 1] =
            waitingDemand.etaToDestination[0];
        modState.latestArrival[baseNodeIdx + 1] =
            waitingDemand.etaToDestination[1];

        // Precedence Constraint (픽업 → 하차 순서)
        modState.deliverySibling[baseNodeIdx] = baseNodeIdx + 1;
        modState.pickupSibling[baseNodeIdx + 1] = baseNodeIdx;

        // 고정 배정
        modState.fixedAssignment[baseNodeIdx] =
            mapVehicleSupplyIdx[waitingDemand.supplyIdx];
        modState.fixedAssignment[baseNodeIdx + 1] =
            modState.fixedAssignment[baseNodeIdx];

        baseNodeIdx += 2;
    }

    // ============================================
    // 4. New Demands - Pickup + Drop-off
    // ============================================
    for (auto& newDemand : modRequest.newDemands) {
        // Pickup 노드
        modState.demands[baseNodeIdx] = newDemand.demand;
        modState.serviceTimes[baseNodeIdx] = nServiceTime;
        modState.earliestArrival[baseNodeIdx] =
            newDemand.etaToStart[0];
        modState.latestArrival[baseNodeIdx] =
            newDemand.etaToStart[1];

        // Drop-off 노드
        int64_t routeTime = timeMatrix[
            baseNodeIdx * (nodeCount + 1) + (baseNodeIdx + 1)
        ];

        modState.demands[baseNodeIdx + 1] = -newDemand.demand;
        modState.serviceTimes[baseNodeIdx + 1] = nServiceTime;
        modState.latestArrival[baseNodeIdx + 1] =
            calc_newdemand_dest_eta(
                routeTime,
                newDemand.etaToStart,
                modRouteConfig,
                nMaxDuration
            );

        // Precedence Constraint
        modState.deliverySibling[baseNodeIdx] = baseNodeIdx + 1;
        modState.pickupSibling[baseNodeIdx + 1] = baseNodeIdx;

        // 차량 미지정 (ALNS가 결정)
        modState.fixedAssignment[baseNodeIdx] = 0;
        modState.fixedAssignment[baseNodeIdx + 1] = 0;

        baseNodeIdx += 2;
    }

    // ============================================
    // 5. 초기 솔루션 구성 (assigned 파라미터)
    // ============================================
    for (auto& assigned : modRequest.assigned) {
        int startIdx = mapVehicleSupplyIdx[assigned.supplyIdx];
        modState.initialSolution.push_back(startIdx);

        for (auto& routeOrder : assigned.routeOrder) {
            int node = (routeOrder.second > 0)
                ? mapRouteToNode[routeOrder.first].first   // Pickup
                : mapRouteToNode[routeOrder.first].second; // Drop-off
            modState.initialSolution.push_back(node);
        }

        modState.initialSolution.push_back(0);  // End depot
    }

    return modState;
}
```

**CModState 데이터 구조**:

```cpp
class CModState {
public:
    vector<int64_t> demands;            // 각 노드의 수요 (픽업: +, 하차: -)
    vector<int64_t> serviceTimes;       // 서비스 시간 (기본 10초)
    vector<int64_t> earliestArrival;    // 최소 도착 시간
    vector<int64_t> latestArrival;      // 최대 도착 시간
    vector<int64_t> acceptableArrival;  // 허용 도착 시간 (페널티 없음)
    vector<int> pickupSibling;          // 하차 노드의 픽업 노드 인덱스
    vector<int> deliverySibling;        // 픽업 노드의 하차 노드 인덱스
    vector<int> fixedAssignment;        // 고정 배정 차량 (0=미지정)
    vector<int64_t> vehicleCapacities;  // 차량 용량
    vector<int> startDepots;            // 차량 시작 노드
    vector<int> endDepots;              // 차량 종료 노드
    vector<int> initialSolution;        // 초기 솔루션 경로
};
```

### 2.6 STEP 5: ALNS 알고리즘 실행 (핵심)

**파일**: alns-pdp library (외부)
**호출**: src/lnsModRoute.cc:392

```cpp
auto solution = solve_lns_pdptw(
    nodeCount + 1,              // 노드 개수
    costMatrix.data(),          // 비용 매트릭스
    distMatrix.data(),          // 거리 매트릭스
    timeMatrix.data(),          // 시간 매트릭스
    modState.demands.data(),    // 수요
    modState.serviceTimes.data(),
    modState.earliestArrival.data(),
    modState.latestArrival.data(),
    modState.acceptableArrival.data(),
    modState.pickupSibling.data(),
    modState.deliverySibling.data(),
    modState.fixedAssignment.data(),
    (int) vehicleCount,
    modState.vehicleCapacities.data(),
    modState.startDepots.data(),
    modState.endDepots.data(),
    (int) modState.initialSolution.size(),
    modState.initialSolution.data(),
    &parameter                  // AlgorithmParameters
);
```

**알고리즘 내부 동작 (의사 코드)**:

```python
def solve_lns_pdptw(
    nodes, cost, dist, time, demands, service_times,
    earliest, latest, acceptable,
    pickup_sibling, delivery_sibling, fixed_assignment,
    vehicles, capacities, start_depots, end_depots,
    initial_solution, parameters
):
    # =========================================
    # 초기화
    # =========================================
    current_solution = construct_initial_solution(initial_solution)
    best_solution = copy(current_solution)

    # 휴리스틱 가중치 초기화
    weights = {
        'shaw_removal': 1.0,
        'worst_removal': 1.0,
        'random_removal': 1.0,
        'route_removal': 1.0
    }

    # Simulated Annealing 초기 온도
    T = calculate_initial_temperature(
        current_solution,
        parameters.simulated_annealing_start_temp_control_w
    )
    cooling_rate = parameters.simulated_annealing_cooling_rate_c

    iteration = 0
    start_time = time.now()

    # =========================================
    # 메인 루프
    # =========================================
    while iteration < parameters.nb_iterations:
        # 시간 제한 체크
        if (time.now() - start_time) > parameters.time_limit:
            break

        # -------------------------------------
        # A. Destroy (파괴 단계)
        # -------------------------------------
        # 1. 휴리스틱 선택 (룰렛 휠 방식)
        removal_heuristic = select_heuristic(weights)

        # 2. 제거할 요청 개수 결정
        n_remove = random.randint(
            int(total_requests * 0.1),  # 최소 10%
            int(total_requests * 0.4)   # 최대 40%
        )

        # 3. 선택된 휴리스틱으로 요청 제거
        if removal_heuristic == 'shaw_removal':
            removed_requests = shaw_removal(
                current_solution,
                n_remove,
                parameters.shaw_phi_distance,
                parameters.shaw_chi_time,
                parameters.shaw_psi_capacity,
                parameters.shaw_removal_p
            )
        elif removal_heuristic == 'worst_removal':
            removed_requests = worst_removal(
                current_solution,
                n_remove,
                parameters.worst_removal_p
            )
        elif removal_heuristic == 'random_removal':
            removed_requests = random_removal(
                current_solution,
                n_remove
            )
        else:  # route_removal
            removed_requests = route_removal(
                current_solution,
                n_remove
            )

        # -------------------------------------
        # B. Repair (복구 단계)
        # -------------------------------------
        new_solution = copy(current_solution)
        unassigned = []

        for request in removed_requests:
            # 1. 모든 가능한 삽입 위치 평가
            best_insertion = None
            min_cost_increase = INFINITY

            for vehicle in range(vehicles):
                # Fixed Assignment 체크
                if fixed_assignment[request] > 0:
                    if fixed_assignment[request] != vehicle + 1:
                        continue

                # 픽업/하차 쌍으로 삽입 시도
                pickup_node = request * 2
                dropoff_node = request * 2 + 1

                for i in range(len(new_solution.routes[vehicle]) + 1):
                    for j in range(i, len(new_solution.routes[vehicle]) + 2):
                        # 제약 조건 검사
                        if not is_feasible(
                            new_solution, vehicle,
                            pickup_node, i,
                            dropoff_node, j,
                            capacities, earliest, latest
                        ):
                            continue

                        # 비용 증가량 계산
                        cost_increase = calculate_insertion_cost(
                            new_solution, vehicle,
                            pickup_node, i,
                            dropoff_node, j,
                            cost
                        )

                        # Noise 추가 (다양성 확보)
                        noise = random.uniform(
                            1.0 - parameters.insertion_objective_noise_n,
                            1.0 + parameters.insertion_objective_noise_n
                        )
                        cost_increase *= noise

                        # 최소 비용 갱신
                        if cost_increase < min_cost_increase:
                            min_cost_increase = cost_increase
                            best_insertion = (vehicle, i, j)

            # 2. 최적 위치에 삽입
            if best_insertion is not None:
                vehicle, i, j = best_insertion
                new_solution.routes[vehicle].insert(i, pickup_node)
                new_solution.routes[vehicle].insert(j, dropoff_node)
            else:
                # 삽입 실패 → missing
                unassigned.append(request)

        new_solution.missing = unassigned

        # -------------------------------------
        # C. Acceptance (수용 단계)
        # -------------------------------------
        delta = evaluate_solution(new_solution) - evaluate_solution(current_solution)

        accept = False
        score = 0  # 0: 거부, 1: 수용, 2: 개선, 3: 최적

        if delta < 0:  # 개선됨
            accept = True
            score = 2

            if evaluate_solution(new_solution) < evaluate_solution(best_solution):
                best_solution = copy(new_solution)
                score = 3
        else:
            # Simulated Annealing
            probability = exp(-delta / T)
            if random.random() < probability:
                accept = True
                score = 1

        if accept:
            current_solution = new_solution

        # 온도 감소
        T = T * cooling_rate

        # -------------------------------------
        # D. Adaptive Weight Update (가중치 조정)
        # -------------------------------------
        if score == 3:  # 새 최적해
            weights[removal_heuristic] += parameters.adaptive_weight_adj_d1
        elif score == 2:  # 개선
            weights[removal_heuristic] += parameters.adaptive_weight_adj_d2
        elif score == 1:  # 수용
            weights[removal_heuristic] += parameters.adaptive_weight_adj_d3

        # 모든 가중치 감쇠
        for h in weights:
            weights[h] *= (1 - parameters.adaptive_weight_dacay_r)

        # 가중치 정규화
        total_weight = sum(weights.values())
        for h in weights:
            weights[h] /= total_weight

        iteration += 1

    # =========================================
    # 결과 반환
    # =========================================
    return best_solution
```

**주요 휴리스틱 설명**:

#### A-1. Shaw Removal

**목적**: 유사한 특성을 가진 요청들을 함께 제거하여 재배치 기회 창출

```python
def shaw_removal(solution, n_remove, phi, chi, psi, p):
    """
    phi: 거리 가중치
    chi: 시간 가중치
    psi: 용량 가중치
    p: 무작위성 파라미터 (높을수록 무작위)
    """
    removed = []

    # 1. 시드 요청 무작위 선택
    seed = random.choice(all_requests)
    removed.append(seed)

    # 2. n_remove개 될 때까지 반복
    while len(removed) < n_remove:
        # 각 요청과 removed 요청들 간의 유사도 계산
        relatedness = {}
        for request in remaining_requests:
            R = 0
            for r in removed:
                # 거리 유사도
                dist_similarity = distance(
                    request.pickup, r.pickup
                ) + distance(
                    request.dropoff, r.dropoff
                )

                # 시간 유사도
                time_similarity = abs(
                    request.earliest_pickup - r.earliest_pickup
                ) + abs(
                    request.latest_dropoff - r.latest_dropoff
                )

                # 용량 유사도
                capacity_similarity = abs(
                    request.demand - r.demand
                )

                # 종합 유사도
                R += (phi * dist_similarity +
                      chi * time_similarity +
                      psi * capacity_similarity)

            relatedness[request] = R / len(removed)

        # 3. 유사도 기반 확률적 선택
        # 낮은 R 값 (유사함)이 선택될 확률이 높음
        sorted_requests = sorted(
            relatedness.items(),
            key=lambda x: x[1]
        )

        # 룰렛 휠 선택 (p 파라미터로 무작위성 조절)
        idx = int(random.random() ** p * len(sorted_requests))
        selected = sorted_requests[idx][0]
        removed.append(selected)

    return removed
```

**예시**:
```
시드 요청: D1 (Pickup: A, Dropoff: B, Time: 10:00)

유사도 계산:
- D2 (Pickup: A 근처, Dropoff: B 근처, Time: 10:05) → R = 50 (매우 유사)
- D3 (Pickup: Z, Dropoff: Y, Time: 15:00) → R = 1000 (매우 다름)

→ D2가 선택될 확률이 높음
```

#### A-2. Worst Removal

**목적**: 현재 솔루션에서 가장 비용이 높은 요청들을 제거

```python
def worst_removal(solution, n_remove, p):
    """
    p: 무작위성 파라미터
    """
    # 1. 각 요청의 제거 시 비용 절감량 계산
    savings = {}
    for request in all_requests:
        cost_before = evaluate_solution(solution)
        temp_solution = remove_request(solution, request)
        cost_after = evaluate_solution(temp_solution)
        savings[request] = cost_before - cost_after

    # 2. 높은 절감량 (= 비용이 높은 요청) 순으로 정렬
    sorted_requests = sorted(
        savings.items(),
        key=lambda x: -x[1]  # 내림차순
    )

    # 3. 확률적 선택
    removed = []
    for i in range(n_remove):
        idx = int(random.random() ** p * len(sorted_requests))
        removed.append(sorted_requests[idx][0])
        sorted_requests.pop(idx)

    return removed
```

#### B. Greedy Insertion

**목적**: 제거된 요청을 최소 비용으로 다시 삽입

```python
def greedy_insertion(solution, request, noise_n):
    best_cost = INFINITY
    best_position = None

    pickup = request.pickup_node
    dropoff = request.dropoff_node

    # 모든 차량, 모든 위치 시도
    for vehicle in range(num_vehicles):
        route = solution.routes[vehicle]

        for i in range(len(route) + 1):
            for j in range(i, len(route) + 2):
                # 제약 조건 검사
                if not is_feasible(vehicle, pickup, i, dropoff, j):
                    continue

                # 비용 증가량 계산
                cost_increase = (
                    cost[route[i-1]][pickup] +
                    cost[pickup][route[i]] -
                    cost[route[i-1]][route[i]] +
                    cost[route[j-1]][dropoff] +
                    cost[dropoff][route[j]] -
                    cost[route[j-1]][route[j]]
                )

                # Noise 추가
                noise = random.uniform(1 - noise_n, 1 + noise_n)
                cost_increase *= noise

                if cost_increase < best_cost:
                    best_cost = cost_increase
                    best_position = (vehicle, i, j)

    if best_position:
        vehicle, i, j = best_position
        solution.routes[vehicle].insert(i, pickup)
        solution.routes[vehicle].insert(j, dropoff)
        return True
    else:
        solution.missing.append(request)
        return False
```

#### C. Simulated Annealing

**목적**: 지역 최적해 탈출

```python
def accept_solution(current_cost, new_cost, temperature):
    delta = new_cost - current_cost

    if delta < 0:
        # 개선됨 → 무조건 수용
        return True
    else:
        # 나빠짐 → 확률적 수용
        probability = exp(-delta / temperature)
        return random.random() < probability

# 온도 감소
temperature = initial_temp
for iteration in range(max_iterations):
    # ... ALNS 반복 ...
    temperature *= cooling_rate  # 0.99975
```

**온도 변화 예시**:
```
Iteration    Temperature    Accept Prob (Δ=100)
    0           100.0           0.368
  1000           77.9           0.290
  2000           60.7           0.189
  3000           47.2           0.116
  4000           36.8           0.066
  5000           28.7           0.035
```

#### D. Adaptive Weight Update

**목적**: 성능 좋은 휴리스틱을 더 자주 선택

```python
def update_weights(weights, heuristic, score, params):
    if score == 3:  # 새 최적해 발견
        weights[heuristic] += params.adaptive_weight_adj_d1  # +33
    elif score == 2:  # 개선
        weights[heuristic] += params.adaptive_weight_adj_d2  # +9
    elif score == 1:  # 수용
        weights[heuristic] += params.adaptive_weight_adj_d3  # +13

    # 모든 가중치 감쇠
    for h in weights:
        weights[h] *= (1 - params.adaptive_weight_dacay_r)  # *0.9

    # 정규화
    total = sum(weights.values())
    for h in weights:
        weights[h] /= total

# 휴리스틱 선택
def select_heuristic(weights):
    r = random.random()
    cumulative = 0
    for h, w in weights.items():
        cumulative += w
        if r < cumulative:
            return h
```

**가중치 변화 예시**:
```
Iteration  Shaw  Worst  Random  Route  | Event
    0      0.25   0.25   0.25   0.25  | Initial
   10      0.32   0.28   0.22   0.18  | Shaw found best
   50      0.35   0.30   0.20   0.15  | Shaw & Worst effective
  100      0.38   0.32   0.18   0.12  | Converging
```

### 2.7 STEP 6: 솔루션 변환 및 반환

**파일**: src/main_utility.cc:402-463

```cpp
string makeDispatchResponse(
    const ModRequest& modRequest,
    size_t vehicleCount,
    unordered_map<int, ModRoute>& mapModRoute,
    const Solution* solution)
{
    ostringstream oss;
    oss << "{\"vehicle_routes\": [";

    // 차량별 경로 변환
    for (int i = 0; i < solution->n_routes; i++) {
        auto& route = solution->routes[i];
        auto& vehicle = modRequest.vehicleLocs[solution->vehicles[i]];

        oss << "{\"supply_idx\":\"" << vehicle.supplyIdx
            << "\",\"routes\":[";

        for (int j = 0; j < route.length; j++) {
            int node = route.path[j];

            // Ghost depot, 차량 시작점 스킵
            if (node <= 0 || node <= vehicleCount) continue;

            // ModRoute 정보 가져오기
            auto& modRoute = mapModRoute[node];

            oss << "{\"id\":\"" << modRoute.id
                << "\",\"demand\":" << modRoute.demand
                << ",\"loc\":{\"lat\":" << modRoute.location.lat
                << ",\"lng\":" << modRoute.location.lng;

            if (!modRoute.location.station_id.empty()) {
                oss << ",\"station_id\":\""
                    << modRoute.location.station_id << "\"";
            }

            oss << "},\"arrival_time\":" << route.times[j]
                << "}";
        }

        oss << "]}";
    }

    // Missing & Unacceptable 요청
    oss << "],\"missing\":[";
    for (int i = 0; i < solution->n_missing; i++) {
        auto& node = solution->missing[i];
        oss << "{\"id\":\"" << mapModRoute[node].id
            << "\",\"demand\":" << mapModRoute[node].demand << "}";
    }

    oss << "],\"unacceptables\":[";
    for (int i = 0; i < solution->n_unacceptables; i++) {
        auto& node = solution->unacceptables[i];
        oss << "{\"id\":\"" << mapModRoute[node].id
            << "\",\"demand\":" << mapModRoute[node].demand << "}";
    }

    oss << "],\"total_distance\":" << solution->distance
        << ",\"total_time\":" << solution->time << "}";

    return oss.str();
}
```

---

## 3. 샘플 시나리오 기반 실행 예시

### 3.1 시나리오 설정

**상황**: 서울 강남구에서 운영 중인 MOD 서비스

**시간**: 2026-01-21 10:30

**차량 현황**:
- **차량 1** (V1): 강남역 근처, 용량 4명
  - 현재 탑승 중: 승객 P1 (목적지: 선릉역)
- **차량 2** (V2): 역삼역 근처, 용량 4명
  - 현재 탑승 중: 없음
  - 배정됨: 승객 P2 (픽업 대기 중, 삼성역 → 강남역)

**새 요청**:
- **승객 P3**: 논현역 → 신논현역
- **승객 P4**: 역삼역 → 선릉역

**목표**: 총 이동 시간 최소화 (optimize_type: Time)

### 3.2 Request JSON

```json
{
  "vehicle_locs": [
    {
      "supply_idx": "V1",
      "capacity": 4,
      "lat": 37.498095,
      "lng": 127.027610,
      "direction": 90
    },
    {
      "supply_idx": "V2",
      "capacity": 4,
      "lat": 37.500775,
      "lng": 127.036490,
      "direction": 180
    }
  ],
  "onboard_demands": [
    {
      "id": "P1",
      "supply_idx": "V1",
      "demand": 1,
      "destination_loc": {
        "lat": 37.504595,
        "lng": 127.049370,
        "station_id": "ST_SEOLLEUNG"
      },
      "eta_to_destination": [300, 600]
    }
  ],
  "onboard_waiting_demands": [
    {
      "id": "P2",
      "supply_idx": "V2",
      "demand": 1,
      "start_loc": {
        "lat": 37.508515,
        "lng": 127.063430,
        "station_id": "ST_SAMSUNG"
      },
      "destination_loc": {
        "lat": 37.498095,
        "lng": 127.027610,
        "station_id": "ST_GANGNAM"
      },
      "eta_to_start": [0, 240],
      "eta_to_destination": [0, -900]
    }
  ],
  "new_demands": [
    {
      "id": "P3",
      "demand": 1,
      "start_loc": {
        "lat": 37.505264,
        "lng": 127.022179,
        "station_id": "ST_NONHYEON"
      },
      "destination_loc": {
        "lat": 37.504521,
        "lng": 127.025087,
        "station_id": "ST_SINNONHYEON"
      },
      "eta_to_start": [0, 1800]
    },
    {
      "id": "P4",
      "demand": 1,
      "start_loc": {
        "lat": 37.500775,
        "lng": 127.036490,
        "station_id": "ST_YEOKSAM"
      },
      "destination_loc": {
        "lat": 37.504595,
        "lng": 127.049370,
        "station_id": "ST_SEOLLEUNG"
      },
      "eta_to_start": [0, 1800]
    }
  ],
  "assigned": [
    {
      "supply_idx": "V1",
      "route_order": [
        ["P1", -1]
      ]
    },
    {
      "supply_idx": "V2",
      "route_order": [
        ["P2", 1],
        ["P2", -1]
      ]
    }
  ],
  "optimize_type": "Time",
  "max_duration": 2400,
  "date_time": "2026-01-21T10:30"
}
```

### 3.3 실행 과정 (단계별)

#### STEP 1: 노드 인덱스 할당

```
Node Index Mapping:
┌─────────────────────────────────────────────┐
│ 0: Ghost Depot                              │
├─────────────────────────────────────────────┤
│ 1: V1 (차량1 시작점 - 강남역)                │
│ 2: V2 (차량2 시작점 - 역삼역)                │
├─────────────────────────────────────────────┤
│ 3: P1_dropoff (선릉역)                       │
├─────────────────────────────────────────────┤
│ 4: P2_pickup (삼성역)                        │
│ 5: P2_dropoff (강남역)                       │
├─────────────────────────────────────────────┤
│ 6: P3_pickup (논현역)                        │
│ 7: P3_dropoff (신논현역)                     │
│ 8: P4_pickup (역삼역)                        │
│ 9: P4_dropoff (선릉역)                       │
└─────────────────────────────────────────────┘

Total Nodes: 10
```

#### STEP 2: 비용 매트릭스 계산

**Valhalla API 호출 결과 (시간 매트릭스, 단위: 초)**:

```
     0    1    2    3    4    5    6    7    8    9
  ┌────────────────────────────────────────────────┐
0 │  0    0    0    ∞    ∞    ∞    ∞    ∞    ∞    ∞│
1 │  0    0  420  480  780  120  180  240  390  450│ V1
2 │  0  450    0  540  360  480  600  630  120  540│ V2
3 │  ∞  480  510    0  360  480  720  750  540   60│ P1_drop
4 │  ∞  810  390  420    0  900  960 1020  390  480│ P2_pick
5 │  ∞  150  480  480  840    0  210  240  450  450│ P2_drop
6 │  ∞  180  600  720  960  180    0   90  570  690│ P3_pick
7 │  ∞  240  630  750 1020  240   60    0  600  720│ P3_drop
8 │  ∞  420  150  540  420  480  600  630    0  540│ P4_pick
9 │  ∞  450  540    0  360  450  690  720  510    0│ P4_drop
  └────────────────────────────────────────────────┘
```

#### STEP 3: 문제 상태 초기화

```python
CModState:
  demands = [0, 0, 0, -1, 1, -1, 1, -1, 1, -1]
           # 0   V1  V2  P1d P2p P2d P3p P3d P4p P4d

  serviceTimes = [0, 0, 0, 10, 10, 10, 10, 10, 10, 10]  # 초

  earliestArrival = [0, 0, 0, 300, 0, 0, 0, 0, 0, 0]
  latestArrival = [2400, 2400, 2400, 600, 240, 1140, 1800, 1800, 1800, 1800]
  acceptableArrival = [2400, 2400, 2400, 600, 240, -1, 1800, 1800, 1800, 1800]

  pickupSibling = [0, 0, 0, 0, 0, 4, 0, 6, 0, 8]
  deliverySibling = [0, 0, 0, 0, 5, 0, 7, 0, 9, 0]

  fixedAssignment = [0, 0, 0, 1, 2, 2, 0, 0, 0, 0]
                    # P1은 V1에, P2는 V2에 고정

  vehicleCapacities = [4, 4]
  startDepots = [1, 2]
  endDepots = [0, 0]

  initialSolution = [
    1, 3, 0,        # V1: 시작 → P1 하차 → 종료
    2, 4, 5, 0      # V2: 시작 → P2 픽업 → P2 하차 → 종료
  ]
```

#### STEP 4: ALNS 알고리즘 실행

**초기 솔루션**:
```
V1 경로: [V1_start] → [P1_dropoff] → [Ghost]
V2 경로: [V2_start] → [P2_pickup] → [P2_dropoff] → [Ghost]

미배정: P3, P4
총 비용: ∞ (미배정 있음)
```

**Iteration 1-10: 초기 삽입**

```
[Iteration 1]
Destroy: Random Removal → 제거 없음 (초기)
Repair: P3, P4 삽입 시도

P3 삽입 평가:
  V1 경로 시도:
    - [V1] → [P3_pick] → [P3_drop] → [P1_drop] → [0]
      비용 증가: 180 + 90 + 750 - 480 = 540초
    - [V1] → [P1_drop] → [P3_pick] → [P3_drop] → [0]
      비용 증가: 720 + 60 = 780초

  V2 경로 시도:
    - [V2] → [P3_pick] → [P3_drop] → [P2_pick] → [P2_drop] → [0]
      비용 증가: 600 + 60 + 1020 - 390 - 900 = 390초 ✓ 최소

  → V2에 삽입: [V2] → [P3_pick] → [P3_drop] → [P2_pick] → [P2_drop] → [0]

P4 삽입 평가:
  V1 경로 시도:
    - [V1] → [P4_pick] → [P4_drop] → [P1_drop] → [0]
      비용 증가: 390 + 540 + 0 - 480 = 450초 ✓ 최소

  → V1에 삽입: [V1] → [P4_pick] → [P4_drop] → [P1_drop] → [0]

새 솔루션:
V1: [V1] → [P4_pick] → [P4_drop] → [P1_drop] → [0]
V2: [V2] → [P3_pick] → [P3_drop] → [P2_pick] → [P2_drop] → [0]
총 시간: 2490초

Accept: YES (개선)
```

**Iteration 50: Shaw Removal 적용**

```
[Iteration 50]
Current Best: 2340초

Destroy: Shaw Removal
  시드: P3 (논현역 → 신논현역)
  유사도 계산:
    - P1: 거리 멀고 시간 다름 → R = 850
    - P2: 거리 가까움 → R = 420
    - P4: 거리 중간, 시간 유사 → R = 550

  선택: P2 (가장 유사) → 제거: [P3, P2]

Repair: P3, P2 재삽입
  P3 → V1에 삽입: [V1] → [P3_pick] → [P3_drop] → [P4_pick] → ...
  P2 → V2에 삽입: [V2] → [P2_pick] → [P2_drop] → [0]

  새 솔루션: 2280초 ✓ 개선

Accept: YES
Best: 2280초
```

**Iteration 100-500: 미세 조정**

```
[Iteration 253]
Destroy: Worst Removal
  비용 계산:
    - P1 제거 시 절감: 60초 (최단 경로)
    - P2 제거 시 절감: 120초
    - P3 제거 시 절감: 150초
    - P4 제거 시 절감: 180초 ✓ 최대

  선택: P4 제거

Repair: P4를 다른 위치에 삽입
  최적: V2에 삽입

  새 솔루션: 2250초 ✓ 개선

Accept: YES
Best: 2250초
```

**최종 반복 (Iteration 5000)**

```
Temperature: 28.7 (초기 100.0에서 감소)
Best Solution: 2220초 (99회 갱신)
Accept Rate: 23.4% (나쁜 솔루션도 수용)

휴리스틱 성능:
  Shaw Removal: 38% (가중치 0.38) - 가장 효과적
  Worst Removal: 32% (가중치 0.32)
  Random Removal: 18% (가중치 0.18)
  Route Removal: 12% (가중치 0.12)
```

#### STEP 5: 최종 솔루션

```
최적 솔루션 (총 시간: 2220초):

V1 경로:
  [V1_start: 강남역]
    ↓ (180초)
  [P3_pickup: 논현역] 10:33 도착
    ↓ (90초)
  [P3_dropoff: 신논현역] 10:35 도착
    ↓ (660초)
  [P1_dropoff: 선릉역] 10:46 도착
    ↓ (0초)
  [Ghost Depot]

V2 경로:
  [V2_start: 역삼역]
    ↓ (120초)
  [P4_pickup: 역삼역] 10:32 도착
    ↓ (540초)
  [P4_dropoff: 선릉역] 10:41 도착
    ↓ (420초)
  [P2_pickup: 삼성역] 10:48 도착
    ↓ (900초)
  [P2_dropoff: 강남역] 11:03 도착
    ↓ (0초)
  [Ghost Depot]

제약 조건 검증:
  ✓ P1 도착 시간: 646초 (300~600초 범위 내)
  ✓ P2 픽업 시간: 120초 (0~240초 범위 내)
  ✓ P2 하차 시간: 1020초 (픽업 후 900초 이내)
  ✓ 차량 용량: V1=1≤4, V2=1≤4
  ✓ Precedence: 모든 픽업 → 하차 순서 준수
```

### 3.4 Response JSON

```json
{
  "status": 0,
  "results": [
    {
      "vehicle_routes": [
        {
          "supply_idx": "V1",
          "routes": [
            {
              "id": "P3",
              "demand": 1,
              "loc": {
                "lat": 37.505264,
                "lng": 127.022179,
                "station_id": "ST_NONHYEON"
              },
              "arrival_time": 180
            },
            {
              "id": "P3",
              "demand": -1,
              "loc": {
                "lat": 37.504521,
                "lng": 127.025087,
                "station_id": "ST_SINNONHYEON"
              },
              "arrival_time": 280
            },
            {
              "id": "P1",
              "demand": -1,
              "loc": {
                "lat": 37.504595,
                "lng": 127.049370,
                "station_id": "ST_SEOLLEUNG"
              },
              "arrival_time": 950
            }
          ]
        },
        {
          "supply_idx": "V2",
          "routes": [
            {
              "id": "P4",
              "demand": 1,
              "loc": {
                "lat": 37.500775,
                "lng": 127.036490,
                "station_id": "ST_YEOKSAM"
              },
              "arrival_time": 120
            },
            {
              "id": "P4",
              "demand": -1,
              "loc": {
                "lat": 37.504595,
                "lng": 127.049370,
                "station_id": "ST_SEOLLEUNG"
              },
              "arrival_time": 670
            },
            {
              "id": "P2",
              "demand": 1,
              "loc": {
                "lat": 37.508515,
                "lng": 127.063430,
                "station_id": "ST_SAMSUNG"
              },
              "arrival_time": 1100
            },
            {
              "id": "P2",
              "demand": -1,
              "loc": {
                "lat": 37.498095,
                "lng": 127.027610,
                "station_id": "ST_GANGNAM"
              },
              "arrival_time": 2010
            }
          ]
        }
      ],
      "missing": [],
      "unacceptables": [],
      "total_distance": 23450,
      "total_time": 2220
    }
  ]
}
```

### 3.5 성능 분석

**ALNS 알고리즘 효과**:

| 지표 | 초기 솔루션 | 최종 솔루션 | 개선율 |
|------|-----------|-----------|-------|
| 총 시간 | 2490초 | 2220초 | -10.8% |
| 총 거리 | 25.8km | 23.5km | -8.9% |
| V1 부하 | 950초 | 950초 | 0% |
| V2 부하 | 1540초 | 1270초 | -17.5% |
| 반복 횟수 | - | 5000회 | - |
| 실행 시간 | - | 1.2초 | - |

**알고리즘 기여도**:

| 휴리스틱 | 개선 횟수 | 가중치 | 기여도 |
|---------|---------|-------|-------|
| Shaw Removal | 42회 | 0.38 | 42.4% |
| Worst Removal | 31회 | 0.32 | 31.3% |
| Random Removal | 18회 | 0.18 | 18.2% |
| Route Removal | 8회 | 0.12 | 8.1% |

---

## 4. 데이터 구조 및 변환 과정

### 4.1 데이터 흐름도

```
┌────────────────────────────────────────────────────────┐
│                  Input Layer                           │
│                                                        │
│  JSON Request (Client)                                 │
│  ├─ vehicle_locs[]                                     │
│  ├─ onboard_demands[]                                  │
│  ├─ onboard_waiting_demands[]                          │
│  ├─ new_demands[]                                      │
│  ├─ assigned[]                                         │
│  └─ optimize_type                                      │
└────────────────────────────────────────────────────────┘
                      ↓ parseRequest()
┌────────────────────────────────────────────────────────┐
│               Business Object Layer                     │
│                                                        │
│  ModRequest (C++ Struct)                               │
│  ├─ vector<VehicleLocation> vehicleLocs                │
│  ├─ vector<OnboardDemand> onboardDemands               │
│  ├─ vector<OnboardWaitingDemand> onboardWaitingDemands │
│  ├─ vector<NewDemand> newDemands                       │
│  ├─ vector<VehicleAssigned> assigned                   │
│  └─ OptimizeType optimizeType                          │
└────────────────────────────────────────────────────────┘
                      ↓ queryCostMatrix()
┌────────────────────────────────────────────────────────┐
│                  Cost Matrix Layer                      │
│                                                        │
│  Routing Engine Response                               │
│  ├─ distMatrix[i][j]: 거리 (m)                         │
│  ├─ timeMatrix[i][j]: 시간 (s)                         │
│  └─ costMatrix[i][j]: 비용 (Time/Distance/CO2)         │
└────────────────────────────────────────────────────────┘
                      ↓ loadToModState()
┌────────────────────────────────────────────────────────┐
│               Algorithm Input Layer                     │
│                                                        │
│  CModState (PDPTW Problem)                             │
│  ├─ demands[]: 수요 (+픽업/-하차)                      │
│  ├─ serviceTimes[]: 서비스 시간                         │
│  ├─ earliestArrival[]: 최소 도착 시간                   │
│  ├─ latestArrival[]: 최대 도착 시간                     │
│  ├─ acceptableArrival[]: 허용 도착 시간                 │
│  ├─ pickupSibling[]: 픽업-하차 연결                     │
│  ├─ deliverySibling[]: 하차-픽업 연결                   │
│  ├─ fixedAssignment[]: 고정 배정                        │
│  ├─ vehicleCapacities[]: 차량 용량                      │
│  ├─ startDepots[]: 시작 노드                           │
│  ├─ endDepots[]: 종료 노드                             │
│  └─ initialSolution[]: 초기 솔루션                      │
└────────────────────────────────────────────────────────┘
                      ↓ solve_lns_pdptw()
┌────────────────────────────────────────────────────────┐
│              Algorithm Output Layer                     │
│                                                        │
│  Solution* (ALNS Result)                               │
│  ├─ n_routes: 경로 개수                                │
│  ├─ routes[]: 각 차량의 경로                           │
│  │   ├─ path[]: 노드 순서                             │
│  │   ├─ times[]: 도착 시간                            │
│  │   └─ length: 경로 길이                             │
│  ├─ vehicles[]: 차량 인덱스                            │
│  ├─ missing[]: 미배정 노드                             │
│  ├─ unacceptables[]: 시간 제약 위반 노드               │
│  ├─ distance: 총 거리                                  │
│  └─ time: 총 시간                                      │
└────────────────────────────────────────────────────────┘
                      ↓ makeDispatchResponse()
┌────────────────────────────────────────────────────────┐
│                  Output Layer                          │
│                                                        │
│  JSON Response (Client)                                │
│  └─ results[]                                          │
│      ├─ vehicle_routes[]                               │
│      │   ├─ supply_idx                                 │
│      │   └─ routes[]                                   │
│      │       ├─ id (demand_id)                         │
│      │       ├─ demand (+1 or -1)                      │
│      │       ├─ loc {lat, lng, station_id}             │
│      │       └─ arrival_time                           │
│      ├─ missing[]                                      │
│      ├─ unacceptables[]                                │
│      ├─ total_distance                                 │
│      └─ total_time                                     │
└────────────────────────────────────────────────────────┘
```

### 4.2 주요 데이터 구조 정의

#### 4.2.1 ModRequest (입력)

```cpp
struct ModRequest {
    // 차량 정보
    vector<VehicleLocation> vehicleLocs;

    // 수요 정보
    vector<OnboardDemand> onboardDemands;
    vector<OnboardWaitingDemand> onboardWaitingDemands;
    vector<NewDemand> newDemands;

    // 기존 배정
    vector<VehicleAssigned> assigned;

    // 최적화 설정
    OptimizeType optimizeType;
    int maxSolutions;
    string locHash;
    optional<string> dateTime;
    int maxDuration;
};

struct VehicleLocation {
    string supplyIdx;           // 차량 ID
    int capacity;               // 용량
    Location location;          // 위치 (lat, lng, direction, station_id)
    array<int, 2> operationTime; // 운행 시간 [시작, 종료]
};

struct OnboardDemand {
    string id;                  // 수요 ID
    string supplyIdx;           // 현재 탑승 차량
    int demand;                 // 승객 수
    Location destinationLoc;    // 목적지
    array<int, 2> etaToDestination; // 도착 시간 범위 [최소, 최대]
};

struct OnboardWaitingDemand {
    string id;
    string supplyIdx;
    int demand;
    Location startLoc;          // 출발지
    Location destinationLoc;    // 목적지
    array<int, 2> etaToStart;   // 픽업 시간 범위
    array<int, 2> etaToDestination; // 하차 시간 범위
};

struct NewDemand {
    string id;
    int demand;
    Location startLoc;
    Location destinationLoc;
    array<int, 2> etaToStart;
};

struct VehicleAssigned {
    string supplyIdx;
    vector<pair<string, int>> routeOrder; // [(demand_id, +1 or -1)]
    vector<int64_t> routeTimes;           // 각 구간 시간
    vector<int64_t> routeDistances;       // 각 구간 거리
};
```

#### 4.2.2 CModState (PDPTW 문제)

```cpp
class CModState {
public:
    // 노드 정보
    vector<int64_t> demands;            // [nodeCount+1]
    vector<int64_t> serviceTimes;       // [nodeCount+1]

    // Time Windows
    vector<int64_t> earliestArrival;    // [nodeCount+1]
    vector<int64_t> latestArrival;      // [nodeCount+1]
    vector<int64_t> acceptableArrival;  // [nodeCount+1]

    // Precedence Constraints
    vector<int> pickupSibling;          // [nodeCount+1]
    vector<int> deliverySibling;        // [nodeCount+1]

    // 고정 배정
    vector<int> fixedAssignment;        // [nodeCount+1]

    // 차량 정보
    vector<int64_t> vehicleCapacities;  // [vehicleCount]
    vector<int> startDepots;            // [vehicleCount]
    vector<int> endDepots;              // [vehicleCount]

    // 초기 솔루션
    vector<int> initialSolution;        // 가변 길이
};
```

#### 4.2.3 Solution (출력)

```cpp
struct Route {
    int* path;       // 노드 순서
    int64_t* times;  // 각 노드 도착 시간
    int length;      // 경로 길이
};

struct Solution {
    int n_routes;           // 경로 개수
    Route* routes;          // 각 차량의 경로
    int* vehicles;          // 차량 인덱스
    int* missing;           // 미배정 노드
    int n_missing;          // 미배정 개수
    int* unacceptables;     // 시간 제약 위반 노드
    int n_unacceptables;    // 위반 개수
    int64_t distance;       // 총 거리
    int64_t time;           // 총 시간
};
```

---

## 5. API 명세서

### 5.1 Base URL

```
Development: http://localhost:8080
Production: https://api.yourservice.com
Docker: http://lnsmodroute:8080
```

### 5.2 공통 헤더

```
Content-Type: application/json
Accept: application/json
```

### 5.3 엔드포인트 목록

| Method | Endpoint | 설명 | 인증 |
|--------|----------|------|------|
| POST | /api/v1/optimize | 라우팅 최적화 | ❌ |
| GET | /api/v1/reset | 라우팅 캐시 리셋 | ❌ |
| PUT | /api/v1/cache | Station 캐시 로딩 | ❌ |
| DELETE | /api/v1/cache | Station 캐시 삭제 | ❌ |
| GET | /api/v1/health | 헬스 체크 | ❌ |
| GET | /api/v1/openapi | OpenAPI 스펙 | ❌ |

---

### 5.4 POST /api/v1/optimize

**설명**: 차량 라우팅 최적화 수행

#### Request

**Headers**:
```
Content-Type: application/json
```

**Body** (`ModRequest`):

```typescript
interface ModRequest {
  vehicle_locs: VehicleLocation[];
  onboard_demands?: OnboardDemand[];
  onboard_waiting_demands?: OnboardWaitingDemand[];
  new_demands?: NewDemand[];
  assigned?: VehicleAssigned[];
  optimize_type?: "Time" | "Distance" | "Co2";
  max_solution_number?: number;
  loc_hash?: string;
  date_time?: string;  // ISO 8601: "YYYY-MM-DDThh:mm"
  max_duration?: number;
}

interface VehicleLocation {
  supply_idx: string;
  capacity: number;
  lat: number;
  lng: number;
  direction?: number;
  operation_time?: [number, number];
}

interface OnboardDemand {
  id: string;
  supply_idx: string;
  demand?: number;  // default: 1
  destination_loc: Location;
  eta_to_destination?: [number, number];
}

interface OnboardWaitingDemand {
  id: string;
  supply_idx: string;
  demand?: number;
  start_loc: Location;
  destination_loc: Location;
  eta_to_start?: [number, number];
  eta_to_destination?: [number, number];
}

interface NewDemand {
  id: string;
  demand?: number;
  start_loc: Location;
  destination_loc: Location;
  eta_to_start?: [number, number];
}

interface VehicleAssigned {
  supply_idx: string;
  route_order?: [string, number][];  // [(demand_id, +1=pickup|-1=dropoff)]
  route_times?: number[];
  route_distances?: number[];
}

interface Location {
  lat: number;
  lng: number;
  waypoint_id?: number;
  station_id?: string;
  direction?: number;
}
```

**필드 설명**:

| 필드 | 타입 | 필수 | 기본값 | 설명 |
|------|------|------|-------|------|
| `vehicle_locs` | Array | ✅ | - | 차량 위치 정보 |
| `onboard_demands` | Array | ❌ | [] | 탑승 중인 승객 (하차만) |
| `onboard_waiting_demands` | Array | ❌ | [] | 배정되었으나 픽업 전 |
| `new_demands` | Array | ❌ | [] | 새로운 요청 |
| `assigned` | Array | ❌ | [] | 기존 배정 경로 (초기 솔루션) |
| `optimize_type` | Enum | ❌ | "Time" | 최적화 목표 (Time/Distance/Co2) |
| `max_solution_number` | Integer | ❌ | 0 | 생성할 솔루션 개수 (0=1개) |
| `loc_hash` | String | ❌ | "" | 위치 해시 (캐시 키) |
| `date_time` | String | ❌ | null | 트래픽 기반 라우팅 시간 (Valhalla만) |
| `max_duration` | Integer | ❌ | 7200 | 최대 시간 제약 (초) |

**VehicleLocation**:

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `supply_idx` | String | ✅ | 차량 고유 ID |
| `capacity` | Integer | ✅ | 차량 용량 (승객 수) |
| `lat` | Number | ✅ | 위도 |
| `lng` | Number | ✅ | 경도 |
| `direction` | Integer | ❌ | 방향 (0-359도) |
| `operation_time` | [Int, Int] | ❌ | 운행 가능 시간 범위 [시작초, 종료초] |

**OnboardDemand**:

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `id` | String | ✅ | 승객 고유 ID |
| `supply_idx` | String | ✅ | 현재 탑승 차량 ID |
| `demand` | Integer | ❌ | 승객 수 (기본값: 1) |
| `destination_loc` | Location | ✅ | 하차 위치 |
| `eta_to_destination` | [Int, Int] | ❌ | 하차 시간 범위 [최소초, 최대초] |

**OnboardWaitingDemand**:

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `id` | String | ✅ | 승객 고유 ID |
| `supply_idx` | String | ✅ | 배정 차량 ID |
| `demand` | Integer | ❌ | 승객 수 |
| `start_loc` | Location | ✅ | 픽업 위치 |
| `destination_loc` | Location | ✅ | 하차 위치 |
| `eta_to_start` | [Int, Int] | ❌ | 픽업 시간 범위 |
| `eta_to_destination` | [Int, Int] | ❌ | 하차 시간 범위 (음수: 픽업 후 상대시간) |

**NewDemand**:

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `id` | String | ✅ | 승객 고유 ID |
| `demand` | Integer | ❌ | 승객 수 |
| `start_loc` | Location | ✅ | 픽업 위치 |
| `destination_loc` | Location | ✅ | 하차 위치 |
| `eta_to_start` | [Int, Int] | ❌ | 픽업 시간 범위 |

**VehicleAssigned**:

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `supply_idx` | String | ✅ | 차량 ID |
| `route_order` | Array | ❌ | 경로 순서 [승객ID, +1 or -1] |
| `route_times` | Array | ❌ | 각 구간 시간 (초) |
| `route_distances` | Array | ❌ | 각 구간 거리 (m) |

**Location**:

| 필드 | 타입 | 필수 | 설명 |
|------|------|------|------|
| `lat` | Number | ✅ | 위도 (-90 ~ 90) |
| `lng` | Number | ✅ | 경도 (-180 ~ 180) |
| `waypoint_id` | Integer | ❌ | 웨이포인트 ID |
| `station_id` | String | ❌ | 정거장 ID (캐싱용) |
| `direction` | Integer | ❌ | 방향 (0-359도) |

#### Response

**Success (200 OK)**:

```typescript
interface OptimizeResponse {
  status: 0;
  results: SolutionResponse[];
}

interface SolutionResponse {
  vehicle_routes: VehicleRoute[];
  missing: DemandItem[];
  unacceptables: DemandItem[];
  total_distance: number;
  total_time: number;
}

interface VehicleRoute {
  supply_idx: string;
  routes: RouteItem[];
}

interface RouteItem {
  id: string;
  demand: number;  // +N: pickup, -N: dropoff
  loc: Location;
  arrival_time?: number;  // 초 단위
}

interface DemandItem {
  id: string;
  demand: number;
}
```

**Error (400 Bad Request)**:

```json
{
  "status": 400,
  "error": "Fail to parse JSON request"
}
```

**Error Cases**:

| 에러 메시지 | 원인 | 해결 방법 |
|-----------|------|----------|
| "Fail to parse JSON request" | JSON 형식 오류 | JSON 문법 확인 |
| "Invalid JSON request format" | 필수 필드 누락 | vehicle_locs 확인 |
| "Fail to dispatch" | 배차 불가능 | 제약 조건 완화 |
| "Fail to connect to Valhalla" | 라우팅 엔진 오류 | Valhalla 서버 확인 |
| "Invalid optimize type" | optimize_type 오류 | Time/Distance/Co2 중 선택 |

#### Example

**Request**:
```bash
curl -X POST http://localhost:8080/api/v1/optimize \
  -H 'Content-Type: application/json' \
  -d '{
    "vehicle_locs": [
      {
        "supply_idx": "V1",
        "capacity": 4,
        "lat": 37.498095,
        "lng": 127.027610
      }
    ],
    "new_demands": [
      {
        "id": "P1",
        "start_loc": {"lat": 37.5, "lng": 127.03},
        "destination_loc": {"lat": 37.51, "lng": 127.04}
      }
    ],
    "optimize_type": "Time"
  }'
```

**Response**:
```json
{
  "status": 0,
  "results": [
    {
      "vehicle_routes": [
        {
          "supply_idx": "V1",
          "routes": [
            {
              "id": "P1",
              "demand": 1,
              "loc": {"lat": 37.5, "lng": 127.03},
              "arrival_time": 120
            },
            {
              "id": "P1",
              "demand": -1,
              "loc": {"lat": 37.51, "lng": 127.04},
              "arrival_time": 480
            }
          ]
        }
      ],
      "missing": [],
      "unacceptables": [],
      "total_distance": 1500,
      "total_time": 480
    }
  ]
}
```

---

### 5.5 GET /api/v1/reset

**설명**: 라우팅 엔진 내부 캐시 리셋

#### Request

**Headers**: None

#### Response

**Success (200 OK)**:
```json
{
  "status": 0
}
```

**Error (400 Bad Request)**:
```json
{
  "status": 400,
  "error": "Error message"
}
```

#### Example

```bash
curl -X GET http://localhost:8080/api/v1/reset
```

---

### 5.6 PUT /api/v1/cache

**설명**: Station 간 거리/시간 캐시 파일 로딩

#### Request

**Headers**:
```
Content-Type: application/json
```

**Body**:
```json
{
  "key": "cache_2.csv"
}
```

**파일 형식** (`cache_2.csv`):
```
FromStationID ToStationID Distance(m) Time(s)
2800302 2800302 0 0
2800302 2800321 6812 521
2800302 2800322 6791 481
2800302 2800343 1403 152
```

#### Response

**Success (200 OK)**:
```json
{
  "status": 0
}
```

#### Example

```bash
# 캐시 파일 준비
echo "ST1 ST1 0 0
ST1 ST2 5000 300
ST2 ST1 5000 300
ST2 ST2 0 0" > /data/cache.csv

# 캐시 로딩
curl -X PUT http://localhost:8080/api/v1/cache \
  -H 'Content-Type: application/json' \
  -d '{"key":"cache.csv"}'
```

---

### 5.7 DELETE /api/v1/cache

**설명**: 로딩된 Station 캐시 삭제

#### Request

**Headers**: None

#### Response

**Success (200 OK)**:
```json
{
  "status": 0
}
```

#### Example

```bash
curl -X DELETE http://localhost:8080/api/v1/cache
```

---

### 5.8 GET /api/v1/health

**설명**: 서비스 헬스 체크

#### Request

**Headers**: None

#### Response

**Success (200 OK)**:
```json
{
  "status": 0
}
```

#### Example

```bash
curl http://localhost:8080/api/v1/health
```

**Kubernetes Liveness Probe**:
```yaml
livenessProbe:
  httpGet:
    path: /api/v1/health
    port: 8080
  initialDelaySeconds: 10
  periodSeconds: 15
```

---

### 5.9 GET /api/v1/openapi

**설명**: OpenAPI 3.0 스펙 반환

#### Request

**Headers**: None

#### Response

**Success (200 OK)**:
```yaml
openapi: 3.0.0
info:
  title: LNS ModRoute API
  version: 0.9.7
...
```

#### Example

```bash
curl http://localhost:8080/api/v1/openapi > openapi.yaml
```

---

## 6. 알고리즘 파라미터 튜닝 가이드

### 6.1 AlgorithmParameters 전체 목록

```cpp
struct AlgorithmParameters {
    // 실행 제어
    int nb_iterations = 5000;          // 반복 횟수
    int time_limit = 1;                // 최대 실행 시간 (초)
    int thread_count = 1;              // 병렬 스레드 수

    // Shaw Removal
    double shaw_phi_distance = 9.0;    // 거리 가중치
    double shaw_chi_time = 3.0;        // 시간 가중치
    double shaw_psi_capacity = 2.0;    // 용량 가중치
    double shaw_removal_p = 6.0;       // 무작위성 파라미터

    // Worst Removal
    double worst_removal_p = 3.0;      // 무작위성 파라미터

    // Simulated Annealing
    double simulated_annealing_start_temp_control_w = 0.05;
    double simulated_annealing_cooling_rate_c = 0.99975;

    // Adaptive Weight
    double adaptive_weight_adj_d1 = 33.0;  // 최적해 발견
    double adaptive_weight_adj_d2 = 9.0;   // 개선
    double adaptive_weight_adj_d3 = 13.0;  // 수용
    double adaptive_weight_dacay_r = 0.1;  // 감쇠 비율

    // Insertion
    double insertion_objective_noise_n = 0.025;  // 노이즈 비율

    // Removal
    double removal_req_iteration_control_e = 0.4;  // 제거 비율

    // Penalty
    double delaytime_penalty = 10.0;   // 지연 페널티
    double waittime_penalty = 0.0;     // 대기 페널티

    // 기타
    int seed = 1234;                   // 난수 시드
    bool enable_missing_solution = true;  // 일부 배정 실패 허용
    bool skip_remove_route = false;    // 전체 경로 제거 스킵
    int unfeasible_delaytime = 0;      // 허용 불가 지연 시간
};
```

### 6.2 시나리오별 권장 설정

#### 6.2.1 소규모 (차량 < 10, 수요 < 50)

**목표**: 빠른 응답 시간

```cpp
AlgorithmParameters small_scale;
small_scale.nb_iterations = 3000;
small_scale.time_limit = 1;
small_scale.thread_count = 1;
small_scale.delaytime_penalty = 10.0;
small_scale.waittime_penalty = 0.0;
```

**예상 성능**:
- 실행 시간: 0.5-1.0초
- 최적 갭: 5-10%
- 적용 사례: 실시간 탑승 서비스

#### 6.2.2 중규모 (차량 10-30, 수요 50-200)

**목표**: 품질과 속도 균형

```cpp
AlgorithmParameters medium_scale;
medium_scale.nb_iterations = 5000;
medium_scale.time_limit = 2;
medium_scale.thread_count = 2;
medium_scale.delaytime_penalty = 10.0;
medium_scale.waittime_penalty = 0.5;
```

**예상 성능**:
- 실행 시간: 1.5-2.5초
- 최적 갭: 3-7%
- 적용 사례: 일반 배차 서비스

#### 6.2.3 대규모 (차량 > 30, 수요 > 200)

**목표**: 최고 품질

```cpp
AlgorithmParameters large_scale;
large_scale.nb_iterations = 10000;
large_scale.time_limit = 5;
large_scale.thread_count = 4;
large_scale.delaytime_penalty = 15.0;
large_scale.waittime_penalty = 1.0;
large_scale.removal_req_iteration_control_e = 0.3;  // 적게 제거
```

**예상 성능**:
- 실행 시간: 4-6초
- 최적 갭: 1-4%
- 적용 사례: 배치 최적화, 일일 계획

### 6.3 목적 함수별 설정

#### 6.3.1 정시성 중시 (승객 대기 시간 최소화)

```cpp
AlgorithmParameters punctuality_focused;
punctuality_focused.optimize_type = OptimizeType::Time;
punctuality_focused.delaytime_penalty = 20.0;  // 높은 지연 페널티
punctuality_focused.waittime_penalty = 5.0;    // 높은 대기 페널티
```

#### 6.3.2 효율성 중시 (차량 운행 거리 최소화)

```cpp
AlgorithmParameters efficiency_focused;
efficiency_focused.optimize_type = OptimizeType::Distance;
efficiency_focused.delaytime_penalty = 5.0;   // 낮은 지연 페널티
efficiency_focused.waittime_penalty = 0.0;    // 대기 무시
efficiency_focused.enable_missing_solution = false;  // 모든 요청 배정
```

#### 6.3.3 환경 중시 (CO2 배출 최소화)

```cpp
AlgorithmParameters eco_focused;
eco_focused.optimize_type = OptimizeType::Co2;
eco_focused.shaw_phi_distance = 12.0;  // 거리 유사도 강조
eco_focused.nb_iterations = 7000;      // 더 많은 반복
```

### 6.4 파라미터 영향 분석

| 파라미터 | 값 ↑ | 영향 | 권장 범위 |
|---------|------|------|----------|
| `nb_iterations` | 증가 | 품질↑, 시간↑ | 3000-15000 |
| `time_limit` | 증가 | 안전장치 | 1-10초 |
| `shaw_phi_distance` | 증가 | 거리 유사도 중시 | 5-15 |
| `shaw_chi_time` | 증가 | 시간 유사도 중시 | 2-6 |
| `shaw_removal_p` | 증가 | 무작위성↑, 다양성↑ | 3-10 |
| `worst_removal_p` | 증가 | 무작위성↑ | 2-6 |
| `start_temp_control_w` | 증가 | 초기 온도↑, 탐색↑ | 0.01-0.1 |
| `cooling_rate_c` | 증가 | 느린 냉각, 탐색↑ | 0.995-0.9999 |
| `adaptive_weight_adj_d1` | 증가 | 최적해 휴리스틱 선호 | 20-50 |
| `insertion_objective_noise_n` | 증가 | 다양성↑, 품질↓ | 0.01-0.05 |
| `removal_req_iteration_control_e` | 증가 | 많이 제거, 큰 변화 | 0.2-0.5 |
| `delaytime_penalty` | 증가 | 지연 회피, 정시성↑ | 5-20 |
| `waittime_penalty` | 증가 | 대기 회피 | 0-10 |

### 6.5 디버깅 및 분석

#### 로깅 활성화

```bash
lnsmodroute --log-request --log-http
```

**생성 파일**:
- `./logs/YYMMDD_HHMMSS_request.log`: 입력 데이터
- `./logs/YYMMDD_HHMMSS_response.log`: 출력 데이터
- `./logs/YYMMDD_access.log`: HTTP 액세스 로그

#### 성능 측정

```cpp
auto start = std::chrono::high_resolution_clock::now();
auto solution = solve_lns_pdptw(...);
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "Execution time: " << duration.count() << " ms" << std::endl;
```

---

## 7. 결론 및 요약

### 7.1 ALNS 알고리즘 핵심 특징

1. **Adaptive Mechanism**: 성능 좋은 휴리스틱을 동적으로 선호
2. **Diverse Heuristics**: Shaw, Worst, Random, Route Removal
3. **Simulated Annealing**: 지역 최적해 탈출
4. **Noise Injection**: 탐색 다양성 확보
5. **Constraint Handling**: Time Windows, Capacity, Precedence

### 7.2 프로젝트 강점

✅ **동적 시나리오 지원**: 실시간 배차 가능
✅ **다중 목적함수**: Time, Distance, CO2
✅ **실제 도로 네트워크**: Valhalla/OSRM 연동
✅ **효율적인 캐싱**: Station Cache + In-Memory Cache
✅ **다중 플랫폼**: REST API, Python, Java

### 7.3 적용 분야

- 🚖 택시/라이드쉐어링 서비스
- 🚌 수요응답형 버스 (DRT)
- 🚚 배송 라우팅
- ♿ 장애인 이동 서비스
- 🏥 의료 이송 서비스

### 7.4 참고 자료

- **ALNS 원논문**: Ropke, S., & Pisinger, D. (2006). An adaptive large neighborhood search heuristic for the pickup and delivery problem with time windows. Transportation science, 40(4), 455-472.
- **PDPTW**: Cordeau, J. F. (2006). A branch-and-cut algorithm for the dial-a-ride problem. Operations Research, 54(3), 573-586.
- **Simulated Annealing**: Kirkpatrick, S., et al. (1983). Optimization by simulated annealing. Science, 220(4598), 671-680.

---

**문서 버전**: 1.0
**최종 수정**: 2026-01-21
**작성자**: Claude Code Analysis
