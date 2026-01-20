## jni_modroute

**A solver for the Pickup-Dropoff TimeWindow Vehicle Routing Problem (PDPVRPTW)**

This package provides a Java Wrapper for the Ciel lnspdptw library

### build

maven 으로 빌드, maven 이 설치되어 있어야 함  
openjdk 17 이상에서 빌드, openjdk 설치되어 있어야 함

사전에 alns_pdp 라이브러리의 위치를 환경변수 LNSPDPTW_PATH 에 export 시켜야 됩니다.

```
export LNSPDPTW_PATH="$PWD/../shared"
cd java
mvn package
```

그러면 target 디렉토리 아래에 mod_route-0.9.7-SNAPSHOT.jar 파일 생성

