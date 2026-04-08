# Extended Gaussian 프로젝트 분석과 코드 실행 흐름 (Phase 0 기준)

## 관련 문서

- `docs/extended_gaussian_vs_sibr_viewer_ko.md`
  - 이 프로젝트가 SIBR viewer와 얼마나 비슷하고 얼마나 다른지 정리한 문서
- `docs/sibr_gaussian_swap_detailed_design.md`
  - `.ply`/Gaussian 자산을 외부 메타파일 기준으로 자동 로드/언로드하는 구조를 설계한 문서

## 1. 이 프로젝트는 무엇인가

이 저장소는 단순한 "가우시안 스플래팅 렌더러 하나"가 아니라, **SIBR(System for Image-Based Rendering) 코어 프레임워크 위에 `extended_gaussian` 프로젝트를 얹은 C++/CUDA 기반 뷰어 애플리케이션**이다.

코드를 기준으로 보면 이 프로젝트의 핵심 역할은 아래와 같다.

- SIBR의 공용 윈도우/뷰/카메라/렌더타깃 프레임워크를 재사용한다.
- 학습 결과 디렉터리에서 Gaussian Splatting 자산을 읽는다.
- 읽어온 가우시안 필드를 에셋으로 보관한다.
- 에셋을 씬 인스턴스로 배치하고 위치/회전/스케일을 수정할 수 있게 한다.
- 인스턴스를 GPU 메모리로 올린 뒤 CUDA rasterizer를 이용해 실시간 렌더링한다.
- ImGui 기반 UI로 에셋 import, 인스턴스 생성/삭제, 캡처, 뷰 토글을 제공한다.

즉, **"학습된 Gaussian Splatting 결과를 불러와서 여러 개 배치하고, 편집하며, 실시간으로 보는 SIBR 기반 편집형 뷰어"**라고 이해하면 가장 정확하다.

반대로 이 저장소가 직접 하는 일은 다음이 아니다.

- 학습(training) 자체
- dataset preprocessing 파이프라인의 본체
- diff-gaussian-rasterization 알고리즘 구현 전체

실제 rasterization 핵심은 CMake에서 외부 의존성으로 가져오는 `diff-gaussian-rasterization`에 맡기고, 이 저장소는 **장면 관리, 데이터 로딩, GPU 자원 관리, 뷰어 UI와 프레임 orchestration**을 담당한다.

빠른 파일 책임표나 편집 대상 lookup이 필요하면 `AGENTS.md`를 먼저 보고, 이 문서는 그 구조가 왜 그렇게 잡혀 있는지와 호출 흐름 맥락을 길게 설명하는 long-form 워크스루로 읽는 편이 좋다.

## 2. 저장소 구조를 한눈에 보면

### 루트 기준 핵심 폴더

- `src/core/*`
  - SIBR 공용 프레임워크.
  - `Window`, `ViewBase`, `MultiViewBase`, `RenderingMode`, 카메라 핸들러, 렌더타깃 같은 기반 기능이 여기에 있다.
- `src/projects/extended_gaussian/*`
  - 이번 프로젝트 전용 코드.
  - 앱 진입점, 렌더러, 리소스 로더, 씬, CUDA 커널, 서브시스템이 들어있다.
- `docs/*`
  - 기존 SIBR 문서와 이번 문서를 두는 위치.
- `cmake/*`
  - 플랫폼별 빌드 유틸리티와 의존성 설정.

### 빌드 관점에서 중요한 파일

- `CMakeLists.txt`
  - 루트 빌드 엔트리.
- `src/CMakeLists.txt`
  - SIBR core와 개별 project를 켜고 끄는 허브.
- `src/projects/extended_gaussian/CMakeLists.txt`
  - `apps`, `renderer`를 묶는 프로젝트 엔트리.
- `src/projects/extended_gaussian/apps/extended_gaussianViewer/CMakeLists.txt`
  - 실행 파일 생성.
- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
  - `extended_gaussian` 공유 라이브러리 생성.
  - 외부 의존성 `diff-gaussian-rasterization`를 `CudaRasterizer`로 가져온다.

## 3. 빌드 결과물과 의존성

코드를 기준으로 보면 실제 산출물은 두 개로 이해하면 된다.

- `extended_gaussian` 공유 라이브러리
  - 프로젝트 전용 렌더링/로더/씬/UI 코드가 들어간다.
- `extended_gaussianViewer_app` 실행 파일
  - `main.cpp`를 포함하며, 실제 윈도우를 띄우고 메인 루프를 돌린다.

주요 의존성은 다음과 같다.

- SIBR core 라이브러리들
  - `sibr_system`, `sibr_view`, `sibr_assets`, `sibr_renderer` 등
- OpenGL / GLEW / GLFW
- CUDA runtime
- `diff-gaussian-rasterization`
- Boost / OpenCV / ASSIMP / OpenMP

이 프로젝트는 구조상 **Windows 중심 SIBR 코드베이스** 위에 만들어져 있고, Gaussian 렌더링 경로는 **CUDA 사용이 필수**다.

## 4. 가장 중요한 실행 흐름 요약

실제 런타임 호출 순서를 짧게 쓰면 아래와 같다.

```text
main
 -> Window 생성
 -> ExtendedGaussianViewer 생성
    -> GaussianScene 생성
    -> ResourceManager 생성
    -> RenderingSystem 생성
       -> GaussianView 생성
       -> 카메라 핸들러 생성
       -> RenderGaussianScene 생성
 -> while(window.isOpened())
    -> Input::poll()
    -> viewer.onUpdate()
       -> MultiViewBase가 각 subview와 camera handler 업데이트
    -> viewer.onRender()
       -> 메뉴/패널 GUI 그림
       -> MultiViewBase::onRender()
          -> renderSubView()
             -> MonoRdrMode::render()
                -> GaussianView::onRenderIBR()
                   -> GPU world buffer 구성
                   -> CudaRasterizer::Rasterizer::forward()
                   -> 결과를 화면용 RT로 복사
    -> viewer.onSwapBuffer()
```

이 한 줄 호출 체인을 먼저 잡고, 세부적인 렌더 단계는 아래 §9~§10에서 따라가면 된다.

## 5. 시작점부터 끝까지 자세히 따라가기

### 5.1 `main.cpp`: 프로그램 진입점

파일:

- `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

역할:

- 커맨드라인 인자를 파싱한다.
- `sibr::Window`를 만든다.
- `ExtendedGaussianViewer`를 만든다.
- OS 창이 열려 있는 동안 입력, 업데이트, 렌더, 버퍼 스왑을 반복한다.

핵심 흐름:

1. `CommandLineArgs::parseMainArgs(ac, av)`
   - SIBR 공용 인자 파서 호출.
2. `BasicIBRAppArgs myArgs`
   - 윈도우/앱 관련 기본 설정 컨테이너를 준비한다.
3. `Window window("Extended Gaussian Viewer", ...)`
   - 실제 OS 창과 OpenGL context를 만든다.
4. `ExtendedGaussianViewer viewer(window, false)`
   - 프로젝트 전용 뷰어를 구성한다.
5. 메인 루프
   - `Input::poll()`
   - `window.makeContextCurrent()`
   - ESC 입력 시 종료
   - `viewer.onUpdate(...)`
   - `viewer.onRender(window)`
   - `viewer.onSwapBuffer(window)`

즉 `main.cpp`는 로직을 거의 가지지 않고, **뷰어 객체에 모든 프레임 책임을 위임하는 얇은 엔트리 포인트**다.

### 5.2 `ExtendedGaussianViewer`: 프로젝트 전용 앱 셸

파일:

- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/extended_gaussian/renderer/ExtendedGaussianViewer.cpp`

이 클래스는 SIBR의 `MultiViewBase`를 상속한 뒤, extended_gaussian 프로젝트에 필요한 자원과 UI를 올린 앱 셸이다.

생성자에서 하는 일:

1. 윈도우 참조와 GUI 여부 저장
2. 렌더링 모드를 `MonoRdrMode`로 설정
3. 기본 subview 해상도 설정
4. `GaussianScene` 생성
   - CPU 측 씬 인스턴스 목록 담당
5. `ResourceManager` 생성
   - CPU 측 GaussianField 에셋 목록 담당
6. `RenderingSystem` 생성
   - 실제 렌더링 서브시스템
7. `RenderingSystem::onSystemAdded(*this)` 호출
   - 여기서 실제 Gaussian view와 카메라가 붙는다.

프레임 단계별 역할:

- `onUpdate`
  - 기본 `MultiViewBase::onUpdate`를 호출해 subview, 입력, 카메라 업데이트를 맡긴다.
  - `Ctrl+Alt+G`로 GUI 표시를 토글한다.

- `onRender`
  - 윈도우 전체 clear
  - `onGui` 호출
  - `MultiViewBase::onRender` 호출
  - FPS counter 갱신

- `onGui`
  - 메인 메뉴 바
  - View 켜기/끄기
  - 전체화면, vsync, HiDPI
  - 캡처/비디오 export
  - `Scene Outliner` 패널
  - `Resource Browser` 패널

이 클래스는 쉽게 말해 **"프로젝트의 최상위 편집 UI + 씬/리소스/서브시스템 조립자"**다.

## 6. 서브시스템 초기화: 실제 렌더러가 붙는 시점

### 6.1 `RenderingSystem`

파일:

- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.cpp`

이 클래스는 현재 코드 기준으로 **사실상 유일하게 동작하는 서브시스템**이다.

생성자에서 하는 일:

1. `type = RENDERING_SYSTEM`
2. CUDA device 개수 확인
3. device 0 선택
4. portable bundle 기준으로 compute capability 7.5 이상인지 검사

즉 프로그램 시작 시점에 **CUDA 실행 가능 환경 검증**을 먼저 한다.

`onSystemAdded(ExtendedGaussianViewer&)`에서 하는 일:

1. 윈도우 크기를 읽어 `GaussianView` 생성
2. ImGui subview 플래그 구성
3. 뷰어에 `"Gaussian View"`라는 이름으로 IBR subview 등록
4. 시작 카메라 생성
   - 위치 `(5, 5, 5)`
   - 원점 `(0, 0, 0)`을 바라보게 설정
5. `InteractiveCameraHandler` 생성 및 연결
6. 뷰어에 `"Gaussian View"`용 카메라 핸들러 등록
7. `RenderGaussianScene` 생성

중요한 점:

- CPU 씬은 `GaussianScene`
- 렌더 전용 씬은 `RenderGaussianScene`

으로 분리된다.

이 분리는 아주 중요하다. CPU 씬은 사람이 편집하는 원본 데이터이고, 렌더 씬은 GPU 자원과 연결된 **렌더링 친화적 표현**이다.

### 6.2 아직 비어 있는 서브시스템

관련 파일:

- `src/projects/extended_gaussian/renderer/subsystem/Subsystem.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/archive_system/ArchiveSystem.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/archive_system/ArchiveSystem.cpp`
- `src/projects/extended_gaussian/renderer/subsystem/UI_system/UISystem.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/UI_system/UISystem.cpp`

관찰 결과:

- `Subsystem.hpp`에는 공통 인터페이스만 있다.
- `ArchiveSystem.*`, `UISystem.*`는 현재 비어 있다.
- enum에는 `ARCHIVE_SYSTEM` 이 정의되어 있다.
- 오래된 문서나 과거 커밋에는 이전 typo 표기가 남아 있을 수 있다.

따라서 현재 구조는 **서브시스템 확장을 염두에 두었지만, 실제 동작은 RenderingSystem 중심으로만 구현된 상태**다.

## 7. UI를 통한 데이터 흐름

### 7.1 에셋 import 흐름

사용자 동작:

- Resource Browser에서 `Import PLY` 클릭

호출 흐름:

```text
ExtendedGaussianViewer::onShowResourceBrowser
 -> GaussianLoader::load(path)
 -> ResourceManager::addField(...)
```

#### `ResourceManager`

파일:

- `src/projects/extended_gaussian/renderer/resource/ResourceManager.hpp`
- `src/projects/extended_gaussian/renderer/resource/ResourceManager.cpp`

역할:

- `std::unordered_map<std::string, GaussianField::UPtr>` 보관
- 이름 중복 방지
- field 추가/조회/삭제

즉 ResourceManager는 **CPU 쪽 Gaussian 에셋 라이브러리**다.

#### `GaussianField`

파일:

- `src/projects/extended_gaussian/renderer/resource/GaussianField.hpp`

역할:

- 하나의 Gaussian asset을 나타내는 순수 데이터 구조체

주요 필드:

- `path`
  - 원본 모델 디렉터리
- `name`
  - 에셋 이름
- `count`
  - Gaussian 개수
- `pos`
  - 각 Gaussian의 위치
- `scale`
  - 각 Gaussian의 scale
- `rot`
  - quaternion 회전
- `opacities`
  - opacity
- `SHs`
  - spherical harmonics 계수
- `sh_degree`
  - SH 차수
- `min_edges`, `max_edges`
  - bounding box
- `root`
  - 선택적 `GaussianSet` 계층 루트. 데이터 모델에는 있지만 현재 렌더 경로에는 연결되지 않은 spatial hierarchy 컨테이너

이 타입은 CPU 메모리의 원본 데이터 컨테이너이고, 아직 GPU 자원은 아니다.

#### `GaussianLoader`

파일:

- `src/projects/extended_gaussian/renderer/resource/GaussianLoader.hpp`
- `src/projects/extended_gaussian/renderer/resource/GaussianLoader.cpp`

이 로더는 일반적인 "아무 PLY나 읽는" 로더가 아니다. **Gaussian Splatting 학습 결과 디렉터리 구조를 전제로 동작**한다.

기대하는 디렉터리 형태:

```text
<modelPath>/
  cfg_args
  point_cloud/
    iteration_XXXX/
      point_cloud.ply
```

구체적인 로딩 절차:

1. 에셋 이름 결정
   - 폴더 이름을 `GaussianField::name`으로 사용
2. `cfg_args` 파일 열기
   - `sh_degree=...` 추출
3. `point_cloud/iteration_*` 폴더 중 번호가 가장 큰 폴더를 찾기
   - 최신 학습 결과를 선택하는 전략
4. `point_cloud.ply` 바이너리 읽기
5. SH degree에 맞는 템플릿 `loadPly<D>` 호출

`loadPly<D>`에서 하는 중요한 후처리:

- vertex 개수 파악
- 바이너리 블록을 한 번에 읽기
- `scale`은 `exp()`로 복원
- `opacity`는 `sigmoid()` 적용
- quaternion은 정규화
- AABB 계산
- 21비트 축 좌표를 interleave한 Morton code로 정렬
  - AABB 기준 상대 좌표를 정수 격자로 양자화한 뒤 정렬하여 공간적 지역성을 높인다.
- SH 계수를 내부 레이아웃에 맞게 재배치

지원하는 SH degree:

- 0
- 1
- 2
- 3

즉 로더는 단순 파일 로더가 아니라 **학습 결과 디렉터리 해석 + Gaussian 데이터 정규화 + 내부 표현으로의 재배열기**다.

### 7.2 씬 인스턴스 생성 흐름

사용자 동작:

- Scene Outliner에서 `Create New Instance` 클릭

호출 흐름:

```text
ExtendedGaussianViewer::onShowScenePanel
 -> GaussianScene::createInstance(...)
 -> RenderingSystem::onInstanceCreated(...)
 -> RenderGaussianScene::createInstance(...)
 -> RenderGaussianInstance 생성
 -> RenderingSystem::syncRenderInstanceAsset(...)
    -> RenderGaussianInstance::setAssetId(asset_id, cpu_field)
    -> 필요 시 GPUResourceManager lookup/create
```

#### `GaussianScene`

파일:

- `src/projects/extended_gaussian/renderer/scene/GaussianScene.hpp`
- `src/projects/extended_gaussian/renderer/scene/GaussianScene.cpp`

역할:

- 에디터/씬 관점의 인스턴스 저장소
- 이름 기준으로 인스턴스 생성/조회/삭제

이 클래스는 단순하지만 중요하다. 씬 편집 결과의 "진실의 원본(source of truth)"이 여기 있기 때문이다.

#### `GaussianInstance`

파일:

- `src/projects/extended_gaussian/renderer/scene/GaussianInstance.hpp`
- `src/projects/extended_gaussian/renderer/scene/GaussianInstance.cpp`

역할:

- 하나의 씬 오브젝트
- 어떤 logical `assetId`를 참조하는지
- 어디에 놓였는지
- 어떤 회전/스케일을 가지는지

주요 데이터:

- `name`
- `assetId`
- `position`
- `eular_angle`
- `scale`

보조 메서드:

- translation matrix 계산
- rotation quaternion / matrix 계산
- scale matrix 계산

중요한 점:

- 인스턴스는 Gaussian 데이터를 복사하지 않는다.
- `GaussianField*`를 직접 들지 않고 `std::string assetId`로 logical identity만 가진다.
- 이 변경 덕분에 CPU asset unload / reload 이후에도 씬 인스턴스의 identity를 유지할 수 있다.

즉 인스턴스는 **에셋에 대한 배치 정보(transform) 레이어**다.

## 8. CPU 씬에서 렌더 씬으로 넘어가는 과정

### 8.1 `RenderGaussianScene`

파일:

- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderGaussianScene.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderGaussianScene.cpp`

역할:

- `GaussianInstance*`를 key로 하여 `RenderGaussianInstance`를 관리

즉 CPU 씬의 인스턴스를 직접 렌더하지 않고, 렌더 전용 래퍼를 둔다. 이 레이어는 CPU 씬과 GPU 캐시 사이의 연결점이지만 ownership은 여기서 가지지 않는다.

이렇게 분리하면 다음이 쉬워진다.

- GPU 자원 생명주기 관리
- 렌더링 캐시
- 나중에 culling이나 batching 확장

### 8.2 `RenderGaussianInstance`

파일:

- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderGaussianInstance.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/RenderGaussianInstance.cpp`

역할:

- CPU 인스턴스(`GaussianInstance`)를 참조
- 현재 인스턴스의 `assetId`를 보관
- 필요할 때 `GPUResourceManager`에서 `GPUGaussianField`를 lookup하는 브릿지 역할

생성자/업데이트 시 하는 일:

1. 원본 CPU 인스턴스 포인터 저장
2. 원본 인스턴스의 `assetId`를 복사
3. `setAssetId(asset_id, cpu_field)`가 호출되면, CPU field가 있는 경우에만 `GPUResourceManager`에 GPU 캐시 엔트리가 있는지 확인
4. 실제 렌더 시점에는 `getGPUField()`가 `assetId`로 매니저를 다시 조회

왜 중요한가:

- render instance가 더 이상 GPU strong owner가 아니다.
- GPU ownership을 `GPUResourceManager` 한 곳으로 모을 수 있다.
- CPU asset unload / reload 이후에도 `assetId` 기준으로 logical identity를 유지할 수 있다.

즉 이 클래스는 **"씬 인스턴스 <-> 공유 GPU 자산" 연결 어댑터**다.

### 8.3 `GPUResourceManager`

파일:

- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.cpp`

역할:

- `std::string assetId`를 key로 GPU 자산 캐시를 관리하는 singleton
- `GPUGaussianField`의 sole strong ownership을 유지하는 곳

제공 기능:

- GPU field 추가
- GPU field 조회
- GPU field 삭제
- `CleanUp()` 인터페이스는 있으나 현재 구현은 placeholder

이 매니저 덕분에 같은 에셋을 여러 인스턴스가 써도 GPU 업로드를 반복하지 않고, CPU 포인터 수명과 GPU 캐시 identity도 분리할 수 있다.

### 8.4 `GPUGaussianField`

파일:

- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUGaussianField.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/gpu_resource_manager/GPUGaussianField.cpp`

역할:

- CPU `GaussianField`를 GPU 메모리에 복사한 버전

생성자에서 하는 일:

- `pos`, `rot`, `scale`, `opacity`, `SHs`를 각각 `cudaMalloc` + `cudaMemcpy`로 업로드

즉 이 타입은 **GPU 전용 구조체**다. CPU 측 STL 벡터를 그대로 쓰지 않고, CUDA 커널과 rasterizer가 읽기 좋은 raw device buffer로 재구성한다. lifetime은 개별 `RenderGaussianInstance`가 아니라 `GPUResourceManager`가 관리한다.

## 9. 실제 프레임 렌더링 흐름

이 프로젝트에서 가장 중요한 호출 체인은 아래와 같다.

```text
main loop
 -> ExtendedGaussianViewer::onRender
 -> MultiViewBase::onRender
 -> renderSubView(IBRSubView)
 -> IBRSubView::render
 -> MonoRdrMode::render
 -> GaussianView::onRenderIBR
```

### 9.1 왜 `MultiViewBase`가 중요한가

파일:

- `src/core/view/MultiViewManager.hpp`
- `src/core/view/MultiViewManager.cpp`
- `src/core/view/RenderingMode.cpp`

`ExtendedGaussianViewer`는 자체 렌더러처럼 보이지만, 사실 대부분의 프레임 orchestration은 SIBR의 `MultiViewBase`가 맡는다.

#### `MultiViewBase::onUpdate`

하는 일:

- pause 상태 확인
- delta time 계산
- 모든 일반 subview 업데이트
- 모든 IBR subview 업데이트
- 연결된 camera handler 업데이트
- IBR subview의 경우 이번 프레임에 사용할 camera를 확정

즉 카메라 이동과 입력 라우팅은 여기서 정리된다.

#### `MultiViewBase::onRender`

하는 일:

- 등록된 IBR subview들을 순회하며 렌더
- 그 뒤 일반 subview 렌더
- 필요하면 subview GUI와 camera GUI도 그림

#### `renderSubView`

하는 일:

1. subview용 render target에 렌더
2. 필요 시 frame capture / video frame 저장
3. 추가 렌더 콜백 수행
4. camera overlay 렌더
5. ImGui 창에 최종 texture 표시

즉 이 프로젝트는 "화면에 바로 그리기"보다 **각 뷰를 오프스크린 RT에 렌더한 뒤 ImGui 창에 붙이는 구조**다.

### 9.2 `MonoRdrMode::render`

파일:

- `src/core/view/RenderingMode.cpp`

역할:

- IBR view를 표준 방식으로 렌더해 주는 어댑터

순서:

1. 내부 destination RT 준비
2. `view.preRender(...)`
3. `view.onRenderIBR(*_destRT, eye)` 호출
4. 결과 texture를 screen quad로 최종 대상에 복사

즉 `GaussianView`는 OpenGL 윈도우에 직접 그리는 것이 아니라, **SIBR 렌더 모드 시스템을 통해 offscreen RT를 거쳐 출력**된다.

## 10. `GaussianView` 안에서 실제로 무엇을 하는가

파일:

- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.hpp`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp`

이 클래스가 진짜 렌더링 핵심이다.

### 10.1 생성 시 준비하는 것

생성자에서 하는 일:

1. 화면 복사용 `BufferCopyRenderer` 생성
2. view / projection / camera position / background 용 CUDA 버퍼 생성
3. 결과 이미지 저장용 OpenGL buffer 생성
4. CUDA-OpenGL interop 등록 시도
5. interop 실패 시 fallback device buffer + CPU 복사 경로 준비
6. rasterizer용 geometry/binning/image 임시 버퍼 리사이즈 함수 준비

즉 생성자 단계에서 이미 **한 프레임 렌더를 위한 CUDA/GL 브리지**를 모두 준비한다.

### 10.2 `onRenderIBR`: 매 프레임 핵심 루틴

`GaussianView::onRenderIBR`의 흐름을 단계별로 보면:

1. 출력 이미지 버퍼를 background 색으로 초기화
2. `RenderGaussianScene`의 모든 인스턴스를 순회하며 총 Gaussian 수 계산
3. 필요하면 world buffer 재할당
4. 카메라 view/proj 행렬을 rasterizer 좌표계에 맞게 변환
5. 배경색, view, proj, camera position을 GPU로 복사
6. 각 인스턴스를 world buffer에 append
   - 위치/회전/스케일 변환
   - SH 복사
   - opacity 복사
7. CUDA-GL interop가 가능하면 OpenGL buffer를 CUDA에서 직접 매핑
8. `CudaRasterizer::Rasterizer::forward(...)` 호출
9. interop이면 unmap, fallback이면 CPU로 한 번 받아 OpenGL buffer 갱신
10. copy shader로 최종 render target에 화면 출력
11. `current_world_gausians_count`를 0으로 초기화

중요한 해석:

- GPU 자산은 필드 단위로 유지된다.
- 하지만 프레임마다 **현재 씬에 놓인 모든 인스턴스를 하나의 world buffer에 다시 모은다**.
- 즉 "공유 원본 GPU field"와 "이번 프레임용 world buffer"가 분리되어 있다.

이 구조의 장점:

- 동일 에셋의 여러 인스턴스를 쉽게 배치 가능
- 인스턴스별 transform 적용이 쉬움
- 렌더러는 최종적으로 하나의 큰 Gaussian 배열만 보면 됨

### 10.3 `resizeWorldBuffersIfNeeded`

역할:

- 현재 프레임에 필요한 Gaussian 수가 기존 capacity를 넘으면 world buffer를 다시 할당

재할당 대상:

- `world_pos_cuda`
- `world_rot_cuda`
- `world_scale_cuda`
- `world_opacity_cuda`
- `world_shs_cuda`
- `world_rect_cuda`

특징:

- 필요한 count보다 약 1.2배 크게 잡아 재할당 빈도를 줄인다.
- 한 번 커진 버퍼는 현재 코드에서 다시 줄이지 않는다. 즉 확장-only 정책이다.

### 10.4 `AppendGaussianToWorld`

역할:

- 인스턴스 하나의 GPU field를 이번 프레임 world buffer에 붙인다.

하는 일:

1. instance transform을 반영한 위치/회전/스케일 변환
2. SH 계수 복사
3. opacity 복사
4. global offset 갱신

즉 이 함수가 **"공유 에셋 -> 프레임별 world representation"** 변환의 중심이다.

## 11. CUDA 커널은 어떤 역할을 하나

파일:

- `src/projects/extended_gaussian/renderer/cuda/TransformKernels.cuh`
- `src/projects/extended_gaussian/renderer/cuda/TransformKernels.cu`

이 커널은 rasterization 자체를 하지 않는다. 그 대신 **각 인스턴스가 가진 transform을 Gaussian들의 로컬 데이터에 적용해 world 데이터로 바꾸는 일**을 맡는다.

`transformGaussiansKernel`이 하는 일:

- local position에 world matrix 적용
- local quaternion과 instance quaternion을 곱해 world rotation 계산
- local scale에 instance scale 곱하기

즉 이 커널의 역할은:

- "모델 공간 Gaussian 필드"를
- "씬에 배치된 월드 공간 Gaussian 묶음"으로 바꾸는 것

실제 splatting/rasterization은 여기서 하지 않고, 그 다음 단계의 외부 rasterizer가 한다.

## 12. 외부 rasterizer와의 경계

파일 근거:

- `src/projects/extended_gaussian/renderer/CMakeLists.txt`
- `src/projects/extended_gaussian/renderer/subsystem/rendering_system/GaussianView.cpp`

관찰 결과:

- CMake에서 `diff-gaussian-rasterization`을 `CudaRasterizer`로 가져온다.
- `GaussianView.cpp`는 `rasterizer.h`를 include하고 `CudaRasterizer::Rasterizer::forward(...)`를 호출한다.

따라서 이 저장소는 rasterizer의 "호출자"다.

정리하면:

- 이 저장소가 하는 일
  - 데이터 로딩
  - 씬/인스턴스 관리
  - GPU 자원 관리
  - 프레임용 world buffer 생성
  - rasterizer 호출과 결과 표시
- 외부 라이브러리가 하는 일
  - 실제 Gaussian splatting rasterization

즉 렌더링 파이프라인의 마지막 고비용 계산은 외부 CUDA 라이브러리에 위임되어 있다.

## 13. 빠른 파일 책임표는 `AGENTS.md` 우선

파일별 책임 요약과 편집 대상 lookup은 `AGENTS.md`가 더 짧고 최신 상태를 유지한다. 이 문서에서는 같은 표를 다시 복제하지 않고, 호출 흐름과 설계 이유 설명에 집중한다.

## 14. 이 프로젝트의 설계 포인트

이 코드를 읽으며 가장 눈에 띄는 설계 포인트는 아래 네 가지였다.

### 14.1 CPU asset / 씬 / GPU asset / frame world buffer가 분리되어 있다

한 덩어리로 다루지 않고 네 층으로 나눴다.

- CPU asset: `GaussianField`
- CPU scene placement: `GaussianInstance`, `GaussianScene`
- 공유 GPU asset: `GPUGaussianField`, `GPUResourceManager`
- 프레임 전용 최종 버퍼: `GaussianView`의 world buffers

이 분리 덕분에 편집성과 렌더 효율을 동시에 챙긴다.

### 14.2 SIBR 프레임워크를 적극적으로 재사용한다

프로젝트 코드가 직접 하는 일은 생각보다 좁다.

- 윈도우 루프
- subview 배치
- 입력 라우팅
- 카메라 핸들링
- 오프스크린 RT 표시

같은 범용 기능은 SIBR core가 맡고, 프로젝트는 Gaussian 렌더링과 UI에 집중한다.

### 14.3 렌더러는 "에셋 공유 + 프레임 재조립" 전략을 쓴다

이 프로젝트는 인스턴스마다 전체 Gaussian을 새로 업로드하지 않는다. 대신:

- 에셋은 GPU에 한 번 올려 공유하고
- 매 프레임 transform만 반영해서 world buffer를 다시 만든다

이 패턴은 여러 인스턴스 배치에 잘 맞는다.

### 14.4 구조는 확장형인데 구현은 아직 초기 단계다

코드상으로 보이는 신호:

- 비어 있는 `ArchiveSystem`, `UISystem`
- enum 오타
- 일부 멤버는 아직 쓰이지 않음

즉 현재 상태는 **기본적인 편집형 Gaussian viewer는 동작하지만, 전체 에디터 시스템으로는 아직 확장 중인 구조**에 가깝다.

## 15. 한 문장으로 다시 요약

이 프로젝트는 **SIBR 기반의 멀티뷰 프레임워크 위에서, Gaussian Splatting 학습 결과를 에셋으로 읽어 씬 인스턴스로 배치하고, 이를 GPU world buffer로 재조립해 CUDA rasterizer로 그려 주는 실시간 편집형 뷰어**다.
