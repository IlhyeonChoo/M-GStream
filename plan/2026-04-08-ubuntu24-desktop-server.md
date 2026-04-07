# Ubuntu 24.04 Desktop / Server 포팅 계획

작성일: 2026-04-08  
작업 브랜치: `develop/ubuntu24-desktop-server`

## 1. 목표

- Ubuntu 24.04 Desktop에서 `extended_gaussian` GUI viewer를 실행 가능하게 만든다.
- Ubuntu 24.04 Server에서 GUI 없이 EGL 기반 headless 렌더를 실행 가능하게 만든다.
- 두 모드는 가능한 한 같은 바이너리와 같은 코드 경로를 공유하고, CLI 플래그로만 분기한다.

## 2. 완료 조건

- Ubuntu 24.04 Desktop
  - configure / build 성공
  - viewer 창 실행 성공
  - manifest load 및 기본 렌더링 확인
- Ubuntu 24.04 Server
  - configure / build 성공
  - `--headless` 모드 실행 성공
  - `--snapshot <path>` 로 이미지 출력 성공
- 문서에는 다음이 포함된다.
  - apt 패키지 의존성
  - CUDA / EGL 전제
  - Desktop 실행 방법
  - Server headless 실행 방법
  - baseline known issue

## 3. 구현 범위

### 3.1 build / dependency

- Ubuntu 24.04 기준 패키지 목록을 고정한다.
- Linux CMake dependency resolution을 Ubuntu 24.04 기준으로 다시 검증한다.
- 필요한 경우 `find_package` / include / library name 차이를 보정한다.

### 3.2 실행 모드

- GUI mode
  - 현재 viewer main loop를 유지한다.
- headless mode
  - EGL context 생성
  - offscreen render target 구성
  - 지정된 해상도로 한 프레임 또는 연속 프레임 렌더
  - `--snapshot` 출력

### 3.3 CLI

- 다음 플래그를 추가한다.
  - `--headless`
  - `--render-width`
  - `--render-height`
  - `--snapshot`
- GUI 실행 경로는 기존 동작을 유지한다.

### 3.4 범위 제외

- 원격 스트리밍
- 인증/네트워크 기능
- OOM 수정

## 4. 검증

- Ubuntu 24.04 Desktop
  - configure / build
  - manifest 로드
  - scene 렌더링
- Ubuntu 24.04 Server
  - configure / build
  - `--headless --snapshot` 실행
  - 출력 이미지 확인
- OOM 검증
  - GUI / headless 모두 장시간 카메라 이동 또는 연속 렌더에서 메모리 증가 여부 관찰

## 5. baseline known issue

- 현재 viewer에서 카메라를 계속 이동하면 OOM이 발생할 수 있다.
- 관측 지점:
  - `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp:81`
  - `sibr::resizeFunctional::<lambda>::operator()`
  - `cudaMalloc(ptr, 2 * N)`
- 이 브랜치에서는 포팅을 우선하고, OOM 수정은 별도 브랜치로 분리 가능하게 유지한다.
