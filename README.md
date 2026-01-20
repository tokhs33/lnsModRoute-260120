## Library Download

[cpp-httplib](https://github.com/yhirose/cpp-httplib.git)  
[gason](https://github.com/vivkin/gason.git)  

[alns-pdp](https://github.com/cielmobilityDev/alns-pdp.git)

cpp-httplib, gason 은 include 내에 포함되어 있음

alns-pdp는 설치해야 됨 


### MOD Requests

```javscript
export type Vehicle_loc = {
		supply_idx: number; //차량번호
		lat: number;
		lng: number;
		capacity: number;
}
export type LatLng = { lng: number; lat: number };
export type Onboard_demand = {
		id: number;
		supply_idx: number; // 차량번호
		destination_loc: LatLng;
		eta_to_destination: [number, number]; // 목적지 도착 예상 소요 시간 . seconds
}
export type Onboard_waiting_demand ={
		id: number;
		supply_idx: number; // 할당차량
		start_loc: LatLng;
		destination_loc: LatLng;
		eta_to_start: [number, number]; //  출발지 차량 도착 소요 시간, seconds
		eta_to_destination: [number, number]; // 목적지 도착 예상 소요 시간 . seconds
}
export type New_demand = {
		id: number;
		start_loc: LatLng;
		destination_loc: LatLng;
        eta_to_start: [number, number];
}
export type Assigned = {
	supply_idx: number;		// 차량의 id
	route_order: [number, number][];	// [demand id, 1=pickup|-1=dropoff]
}
export enum Optimize_type {
		Time = 0,
		Distance,
		Co2,
}
export interface LnsSearchRequest {
		vehicle_locs: Vehicle_loc[];
		onboard_demands: Onboard_demand[];
		onboard_waiting_demands: Onboard_waiting_demand[];
		new_demands: New_demand[];
		assigned: Assigned[];
		optimize_type: Optimize_type;
}
```

### MOD Response 

```
export type Route = {
    id: number;
    demand: number
    loc: LatLng;
}
export type Vehicle_Route = {
    supply_idx: number;
    routes: Route[];
};
export interface LnsSearchResponse {
    vehicle_routes: Vehicle_Route[];
    total_distance: number;
    total_time: number;
}
```

## build

```
cmake -S . -B build
cmake --build build
```

debug build

```
cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug
cmake --build debug
```

## running argument



## API specification

### Overview

The `lnsmodroute` API provides programmatic access to our routing optimization services, allowing users to calculate optimized routes for logistics and transportation needs.

### Authentication

Currently, the `lnsmodroute` API does not require authentication.

### Endpoints

#### Calculate Optimized Route

- **URL**: `/api/v1/optimize`
- **Method**: `POST`
- **Body**: `LnsSearchRequest`
	```json
	{
		"vehicle_locs": [
			{
				"supply_idx": 26,
				"lat": 40.75700342,
				"lng": -73.96772186,
				"capacity": 16
			},
			{
				"supply_idx": 45,
				"lat": 40.72895219,
				"lng": -73.97838591,
				"capacity": 16
			},
			{
				"supply_idx": 55,
				"lat": 40.74457421,
				"lng": -73.97745449,
				"capacity": 16
			}
		],
		"onboard_demands": [
			{
				"id": 6431,
				"supply_idx": 26,
				"destination_loc": {
					"lng": -73.97241211,
					"lat": 40.74962616
				},
				"eta_to_destination": [
					180,
					1098
				]
			}
		],
		"onboard_waiting_demands": [
			{
				"id": 8207,
				"supply_idx": 55,
				"start_loc": {
					"lng": -73.99073792,
					"lat": 40.7509613
				},
				"destination_loc": {
					"lng": -73.9903717,
					"lat": 40.73142242
				},
				"eta_to_start": [
					180,
					1082
				],
				"eta_to_destination": [
					780,
					1729
				]
			},
		],
		"new_demands": [
			{
				"id": 8036,
				"start_loc": {
					"lng": -73.98267365,
					"lat": 40.72340012
				},
				"destination_loc": {
					"lng": -74.00027466,
					"lat": 40.7353096
				},
				"eta_to_start": [
					0,
					1800
				]
			},
		],
		"assigned": [
			{
				"supply_idx": 26,
				"route_order": [
					[6431, -1]
				]
			},
			{
				"supply_idx": 55,
				"route_order": [
					[8207, 1],
					[8207, -1]
				]
			}
		],
		"optimize_type": "Distance"
	}
	```
- **Success Response**:
	```json
	{
		"vehicle_routes": [
			{
				"supply_idx":26,
				"routes":[
					{
						"id":6431,
						"demand":-1,
						"loc":{
							"lat":40.74962616,
							"lng":-73.97241211
						}
					}
				]
			},
			{
				"supply_idx":55,
				"routes":[
					{
						"id":8207,
						"demand":1,
						"loc":{
							"lat":40.75096130,
							"lng":-73.99073792
						}
					},
					{
						"id":8207,
						"demand":-1,
						"loc":{
							"lat":40.73142242,
							"lng":-73.99037170
						}
					}
				]
			},
			{
				"supply_idx":45,
				"routes":[
					{
						"id":8036,
						"demand":1,
						"loc":{
							"lat":40.72340012,
							"lng":-73.98267365
						}
					},
					{
						"id":8036,
						"demand":-1,
						"loc":{
							"lat":40.73530960,
							"lng":-74.00027466
						}
					}
				]
			}
		],
		"total_distance":9695.00000000,
		"total_time":1388.00000000
	}	
	```

### docker image 빌드

SSH Agent Forward을 사용하면 로컬 머신의 SSH key를 Docker 컨테이너 내에서 사용할 수 있습니다.
이를 통해 Docker 빌드 과정에서 Github 리포지토리를 clone 할 수 있습니다.
이를 위해서 몇가지 단계를 거쳐야 합니다.

1. SSH Agent 실행 및 키 추가

```sh
eval $(ssh-agent -s)
ssh-add ~/.ssh/id_rsa		# replace with your actual key file
```

2. `ssh_config` 파일 작성

`ssh_config` 파일을 생성하고 다음 내용을 추가합니다. 이 파일을 Dockerfile과 동일한 디렉토리에 저장합니다.

```sh
Host github.com
	User git
	IdentityFile /root/.ssh/id_rsa
	StrictHostKeyChecking no
	ForwardAgent yes
```

3. Docker 빌드

```sh
cd docker
DOCKER_BUILDKIT=1 docker build --ssh default --no-cache -t ciel/lnsmodroute:0.0.1-SNAPSHOT .
```

4. Docker 실행

```sh
docker run -d --name lnsmodroute_container --network my_network -v "${PWD}/test:/data" -p 8080:8080 ciel/lnsmodroute:0.0.1-SNAPSHOT lnsmodroute --host 0.0.0.0 --route-type VALHALLA --route-path http://valhalla:8002 --bypass-time 600  --service-time 5 --max-duration 2400 --cache-directory /data
```

### 캐싱 기능

LnsSearchRequest 에서 loc 에 station_id 를 옵션으로 추가 (정거장ID)

1. 캐싱 파일

캐싱 파일을 다음과 같은 형태로 생성

```
2800302 2800302 0 0
2800302 2800321 6812 521
2800302 2800322 6791 481
2800302 2800343 1403 152
...
```

순서대로 다음의 내용을 가지고 있어야 됨

FromNode ToNode Distance(m) Time(s)

Distance, Time 은 다 integer 형식이어야 정상적으로 내용을 읽어서 처리할 수 있다.


2. 캐싱 디렉토리 설정

캐싱 파일을 프로그램에서 로딩하는 방식은 캐싱 디렉토리를 실행할 때 설정하고 REST 명령으로 캐싱 디렉토리의 파일을 로딩하는 방식

캐싱 디렉토리 설정 방법

|실행 parameter|설명|
|-|-|
|--cache-directory|캐싱 디렉토리 path|


```
$ lnsmodroute --cache-directory ./test
```

3. 캐싱 로딩

아래의 JSON 파일을 data로 REST 명령로 캐싱 로딩, 캐싱이 로딩되지 않으면 예전 방식의 캐싱을 이용  
만약 캐싱이 로딩되어 있는데 request의 loc 에 station_id 가 없으면 어떤 캐싱 방식도 동작하지 않고 다 routing engine에 cost 조회

cache loading request JSON

```json
{ "key": "cache_2.csv" }
```

key : 캐싱 디렉토리에 있는 캐싱 파일 이름

다음의 REST 명령으로 로딩 처리

PUT /api/v1/cache

예를 들어 다음의 명령으로 캐싱을 로딩처리함

```
$ curl -X PUT -H 'Content-Type: application/json' -d '{"key":"cache_2.csv"}' http://localhost:8080/api/v1/cache
```

4. 캐싱 리셋

현재 위의 캐싱 로딩으로 로딩된  Station 별 캐싱을 지울 때 사용

다음의 REST 명령어 이용

DELETE /api/v1/cache

예
```
$ curl -X DELETE http://localhost:8080/api/v1/cache
```

## Python Wheel build

```
cd python
python3 -m build -w -v
```