# Windows Portable Bundle 작업 계획

작성일: 2026-04-08  
작업 브랜치: `develop/windows-portable-bundle`

## 1. 목표

- 다른 Windows 10/11 환경에서 `extended_gaussian`를 새로 clone 후 configure / build / install / run 가능하게 만든다.
- `install/` 기준 실행을 표준 경로로 고정한다.
- 사용자에게 전달 가능한 설치 번들 생성 절차를 문서화하고, 번들 생성 스크립트를 제공한다.
- 저장소에 포함되지 않은 데이터셋(`swaptest/`)은 사용자 제공 데이터 전제로 명시한다.

## 2. 완료 조건

- `install/bin/extended_gaussianViewer_app_rwdi.exe`를 `--appPath <install>`로 실행했을 때 로컬 스모크 테스트가 통과한다.
- 설치 번들 생성 스크립트 하나로 아래 구성이 만들어진다.
  - `install/`
  - `manifests/`
  - Windows 실행 가이드
  - 번들 루트 실행 스크립트
- 문서에는 아래가 모두 포함된다.
  - 요구 환경
  - build / install 명령
  - 번들 생성 방법
  - 다른 Windows PC에서 실행하는 방법
  - manifest와 사용자 제공 데이터 배치 규칙
  - baseline known issue

## 3. 구현 범위

### 3.1 문서

- Windows 전용 사용/배포 문서를 `docs/`에 추가한다.
- 문서에는 다음을 포함한다.
  - 권장 빌드 구성: `RelWithDebInfo`
  - 실행 기본형: `install/bin` + `--appPath`
  - `--manifest` 예시
  - 데이터가 Git에 포함되지 않는 점
  - known issue: `GaussianView.cpp:81` OOM

### 3.2 스크립트

- `tools/windows/` 아래에 설치 번들 생성 PowerShell 스크립트를 추가한다.
- 스크립트는 다음을 수행한다.
  - 입력: 프로젝트 루트, build/install 경로, 출력 경로
  - 출력: bundle 디렉터리 또는 zip
  - 포함 항목:
    - `install/`
    - `manifests/`
    - Windows 가이드 문서
    - 번들 루트 실행 `.cmd`
- 실행 `.cmd`는 번들 루트 기준 상대 경로로 `install/bin/extended_gaussianViewer_app_rwdi.exe`를 실행한다.

### 3.3 범위 제외

- OOM 수정
- 인스톨러 제작
- 예제 데이터 포함
- Ubuntu 관련 변경

## 4. 검증

- 로컬에서 `cmake --build build-ninja --target extended_gaussianViewer_app`
- 로컬에서 `cmake --install build-ninja`
- `install/bin` 실행 5초 스모크
- 번들 생성 스크립트 실행
- 번들 루트 `.cmd` 실행 5초 스모크

## 5. baseline known issue

- 현재 viewer에서 카메라를 계속 이동하면 OOM이 발생할 수 있다.
- 관측 지점:
  - `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp:81`
  - `sibr::resizeFunctional::<lambda>::operator()`
  - `cudaMalloc(ptr, 2 * N)`
- 이번 브랜치에서는 수정하지 않고 문서에 명시만 한다.
