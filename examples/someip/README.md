# SOME/IP 및 TIMPANI RT 연동 애플리케이션 예제 패키지

본 패키지는 **DDR-008** 설계 사양을 바탕으로, SOME/IP 디스패치 경계 기반의 이벤트 감시 및 스케줄링(TT 및 CBS) 연동 흐름을 보여주는 예제 모음입니다.

---

## 📂 폴더 구조 및 구성

```
someip/
├── CMakeLists.txt              # 통합 빌드 파일
├── README.md                   # 본 도움말 파일
├── include/
│   ├── timpani_rt.h            # DDR-008 표준 TIMPANI FFI C 헤더
│   └── vsomeip/
│       └── vsomeip.hpp         # 루프백 UDP 통신 기능이 내장된 Mock vsomeip 헤더
├── src/
│   └── libtimpani_rt.cpp       # FNV-1a 해싱 및 바인딩 시뮬레이터 (공통)
├── radar_based_fcw/            # 1. ADAS FCW 시뮬레이션 예제 (Mock vsomeip 기반)
│   ├── CMakeLists.txt
│   ├── mock_radar_producer.cpp # L1 TT 레이더 센서 노드 모사 (Publisher, 20ms 주기)
│   ├── fcw_consumer.cpp        # L2 CBS 전방 추돌 제어 모사 (Subscriber, 경보 및 FFI 연동)
│   └── dummy_tasks.cpp         # 백그라운드 L1 TT 더미 태스크 시뮬레이터
└── hello_world/                # 2. COVESA 표준 헬로월드 예제 (실제 vsomeip3 연동)
    ├── CMakeLists.txt
    ├── hello_world_service_main.cpp
    ├── hello_world_service.hpp # L2 CBS 스케줄러 바인딩 적용
    ├── hello_world_client_main.cpp
    ├── hello_world_client.hpp  # L1 TT 주기적 전송 루프 스레드 적용
    └── helloworld-local.json   # vsomeip 통신 설정 파일
```

---

## 🛠️ 빌드 방법

컴파일 및 빌드는 표준 CMake를 사용합니다. 실제 COVESA `vsomeip3` 라이브러리가 호스트 시스템에 설치되어 있어야 `hello_world` 서브프로젝트가 정상 빌드됩니다.

```bash
# examples/someip 폴더로 이동
cd TIMPANI/examples/someip

# 빌드 디렉토리 생성 및 빌드
mkdir build && cd build
cmake ..
make
```

빌드가 성공하면 `build/` 내부의 각 하위 폴더에 실행 파일들이 생성됩니다.

---

## 🚦 예제 상세 설명

### 1. `radar_based_fcw` (ADAS FCW 시뮬레이션)
호스트 환경에 별도의 vsomeip 라이브러리가 없는 상태에서도 즉시 빌드 및 실행해 볼 수 있도록 루프백 UDP 소켓 기반의 mock_vsomeip 헤더를 사용하여 구동됩니다.
* **mock_radar_producer (L1 TT)**: 20ms마다 장애물 거리 데이터를 생성해 SOME/IP Event로 송출합니다.
* **fcw_consumer (L2 CBS)**: 이벤트를 수신하면 디스패치 경계 마킹(`timpani_evt_mark_arrival`) 및 CBS 스레드 바인딩(`timpani_cbs_bind_thread`) 하에 전방 충돌 경고(FCW) 알고리즘을 연산합니다.

### 2. `hello_world` (COVESA 표준 vsomeip 연동 + Timpani 적용)
실제 COVESA vsomeip3 미들웨어를 사용하여 통신하며, Timpani API를 실제 연동한 예제입니다.
* **vsomeip_hello_world_client (L1 TT)**:
  * 서비스 사용 가능 상태가 되면 별도의 L1 TT 송신 스레드(`hello_tt_task`)를 기동합니다.
  * `timpani_tt_register()` 및 `timpani_tt_wait_period()`를 사용해 100ms 주기로 `"World"` 데이터를 담은 request를 정밀 전송합니다.
* **vsomeip_hello_world_service (L2 CBS)**:
  * `timpani_evt_register()`를 통해 서비스/인스턴스/메서드 계약을 등록합니다.
  * request를 수신하는 디스패치 콜백 `on_message_cbk` 진입 직후 `timpani_evt_mark_arrival()`로 이벤트 도착 시간 $t_0$를 마킹합니다.
  * 비즈니스 로직 수행 전 `timpani_cbs_bind_thread()`를 통해 스레드를 CBS로 바인딩하고, 완료 후 `timpani_cbs_unbind_thread()`로 바인딩을 해제합니다.

---

## 🏃 실행 및 테스트 방법

### 1. `radar_based_fcw` 실행
```bash
# Terminal 1: Consumer 실행 (CBS 과부하 시뮬레이션 옵션: --overrun-ms 5)
LD_LIBRARY_PATH=. ./radar_based_fcw/fcw_consumer

# Terminal 2: Producer 실행 (MIT 위반 시뮬레이션 옵션: --burst-ms 5)
./radar_based_fcw/mock_radar_producer
```

### 2. `hello_world` 실행
실제 vsomeip 공유 라이브러리 및 설정 파일 경로를 환경 변수로 지정해야 합니다.

```bash
# Terminal 1: Service 기동
export VSOMEIP_CONFIGURATION=../hello_world/helloworld-local.json
export VSOMEIP_APPLICATION_NAME=hello_world_service
LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./hello_world/vsomeip_hello_world_service

# Terminal 2: Client 기동
export VSOMEIP_CONFIGURATION=../hello_world/helloworld-local.json
export VSOMEIP_APPLICATION_NAME=hello_world_client
LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH ./hello_world/vsomeip_hello_world_client
```
* **동작 관찰**: Client 스레드가 L1 TT 정밀 주기에 맞춰 Request를 주기 송신하고, Service 측의 콜백 스레드가 L2 CBS 바인딩 하에 응답을 처리하는 로깅을 콘솔에서 실시간으로 확인할 수 있습니다.
