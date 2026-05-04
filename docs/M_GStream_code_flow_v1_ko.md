# MGStream 코드 실행 흐름 (Version 1)

작성일: 2026-04-08  
기준 브랜치: `develop/phase1-manifest-swap`

> 이 문서는 `docs/M_GStream_code_flow_phase0_ko.md` 이후의 **현재 기준 실행 흐름 스냅샷**이다.  
> 앞으로 흐름이 다시 바뀌면 같은 형식으로 `v2`, `v3` 문서를 추가하고, 기존 버전 문서는 과거 기록으로 유지한다.

## 관련 문서

- `docs/M_GStream_code_flow_phase0_ko.md`
  - manifest / swap 도입 전의 Phase 0 기준 흐름
- `docs/M_GStream_modification_log_ko.md`
  - 구조 변경과 빌드 복구 이력
- `docs/sibr_gaussian_swap_detailed_design.md`
  - manifest 기반 CPU/GPU swap 설계 배경
- `docs/M_GStream_renderer_scene_review_notes_ko.md`
  - renderer / scene split 후속 변경의 점검 포인트

## 1. 이번 버전에서 무엇이 달라졌나

Phase 0 문서는 "수동 import + 수동 instance 생성 + 렌더 직전 GPU 보장" 흐름을 기준으로 작성되었다.  
현재 버전은 여기에 **manifest 기반 asset 등록, 정책 기반 CPU/GPU residency, per-frame streaming tick, swap 통계/UI**가 추가된 상태다.

핵심 변화만 먼저 요약하면 다음과 같다.

- 앱은 이제 `--manifest <path>` 로 시작할 수 있다.
- manifest는 에셋의 **descriptor만 먼저 등록**하고, 실제 Gaussian 데이터 로드는 정책이 필요하다고 판단할 때만 수행한다.
- `MGStreamViewer::onRender()` 안에서 `ViewerContext`를 만들고, `RenderingSystem::tickStreaming()`이 매 프레임 실행된다.
- `SwapPolicy`가 현재 phase / camera bounds / distance rule을 평가해서
  - `required_gpu`
  - `warm_cpu`
  - `protected_assets`
  를 계산한다.
- `SwapManager`가 그 결과를 바탕으로 CPU load, GPU upload, GPU eviction, CPU eviction을 스케줄한다.
- `ResourceManager`의 CPU 상태는 이제 `Unloaded -> Loading -> Resident -> EvictQueued -> Unloaded` 흐름을 명시적으로 가진다.
- `GaussianView`의 핫패스는 여전히 "월드 버퍼 구성 -> CUDA rasterizer 호출"이지만, 내부적으로
  - `TransformPosRotScaleToWorld(...)`
  - `appendSHsToWorld(...)`
  로 분리되었다.

## 2. Phase 0 대비 빠른 비교

| 항목 | Phase 0 | Version 1 |
|---|---|---|
| 앱 시작 입력 | 수동 UI 중심 | 수동 UI + `--manifest` |
| 에셋 등록 | `Import PLY`로 즉시 CPU resident | manifest descriptor 등록 + 필요 시 CPU load |
| GPU 업로드 | 렌더링 직전 수동 보장 | policy 결과에 따라 per-frame upload |
| CPU residency | 사실상 수동 관리 | `desired_cpu` / `warm_cpu` / eviction hysteresis 기반 |
| GPU residency | instance가 참조하면 유지 | `required_gpu` / `protected_assets` 기반 |
| 카메라 기반 스트리밍 | 없음 | `camera_bounds`, `distance` rule 지원 |
| phase 개념 | 없음 | phase string + matching rule 지원 |
| UI | import / instance 편집 중심 | manifest load / current phase / swap stats 추가 |

## 3. 현재 기준 전체 실행 흐름

```text
main
 -> create Window
 -> create MGStreamViewer
    -> create GaussianScene
    -> create ResourceManager
    -> create RenderingSystem
       -> create GaussianView
       -> register "Gaussian View" subview
       -> create camera handler
       -> create RenderGaussianScene
       -> create SwapManager
    -> if --manifest:
       -> ManifestStore::load()
       -> ResourceManager::registerManifest()
       -> RenderingSystem::setManifest()
       -> createManifestInstances(true)   # scene가 비어 있으면
       -> focusCameraOnManifest()
 -> main loop
    -> Input::poll()
    -> viewer.onUpdate()
    -> viewer.onRender()
       -> viewer.onGui()
       -> build ViewerContext from "Gaussian View"
       -> RenderingSystem::tickStreaming(context)
       -> MultiViewBase::onRender()
          -> renderSubView()
             -> MonoRdrMode::render()
                -> GaussianView::onRenderIBR()
                   -> build world buffers from GPU-resident instances
                   -> call CUDA rasterizer
                   -> copy output to render target
    -> viewer.onSwapBuffer()
```

Phase 0와 가장 큰 차이는 `viewer.onRender()` 안에 **streaming tick**이 들어왔다는 점이다.  
렌더링 자체는 여전히 `GaussianView::onRenderIBR()`가 맡지만, 그 직전에 어떤 asset이 CPU/GPU에 있어야 하는지는 `SwapPolicy`와 `SwapManager`가 결정한다.

## 4. 앱 시작 경로

### 4.1 manifest 없이 시작하는 경우

이 경로는 기존 수동 흐름과 거의 같다.

- 사용자가 `Import PLY`로 모델 디렉터리를 고른다.
- `GaussianLoader::load(modelPath)`가
  - `cfg_args`
  - 최신 `point_cloud/iteration_*/point_cloud.ply`
  를 읽어 `GaussianField`를 만든다.
- `ResourceManager::addField(...)`가 이 asset을 즉시 CPU resident 상태로 등록한다.
- 사용자가 `Create New Instance`를 누르면 `GaussianScene::createInstance(...)`가 scene instance를 만든다.
- `RenderingSystem::onInstanceCreated(...)`가 대응되는 `RenderGaussianInstance`를 만든다.
- manifest가 없으면 `RenderingSystem::tickStreaming()`은 `ensureManualGpuResidency()`로 fallback 하여, scene이 참조하는 CPU asset을 GPU에 올린다.

즉 manifest가 없을 때는 여전히 "수동 import -> 수동 instance 생성 -> 수동 GPU 보장" 흐름이 살아 있다.

### 4.2 manifest로 시작하는 경우

`MGStreamViewer` 생성자에서 `--manifest`가 있으면 바로 `loadManifestFile(...)`가 호출된다.

이 함수는 아래 순서로 동작한다.

1. `ManifestStore::load(path)`
   - JSON 파싱
   - `global`, `assets`, `rules` 로딩
   - 상대 경로 `model_dir`를 manifest 파일 기준으로 해석
   - 각 rule이 참조하는 asset 집합을 `referencedAssets_`로 계산
2. `ResourceManager::registerManifest(_manifestStore)`
   - 각 asset의 descriptor를 registry에 등록
   - 이 시점에는 대부분 실제 Gaussian 데이터가 아직 메모리에 없다
3. manifest에 정의된 phase 목록을 읽고, 현재 phase가 비어 있거나 유효하지 않으면 첫 phase로 맞춤
4. `RenderingSystem::setManifest(&_manifestStore)`
   - 내부 `SwapManager`가 manifest를 참조하도록 설정
5. scene가 비어 있으면 `createManifestInstances(true)`
   - manifest asset마다 scene instance를 생성
   - 단, 이것도 "씬 인스턴스 생성"이지 "CPU/GPU 데이터 로드 완료"를 뜻하지는 않는다
6. `focusCameraOnManifest()`
   - manifest asset들의 bounds를 합쳐 AABB를 만든다
   - 카메라를 그리드 내부로 옮겨 `camera_bounds` rule이 즉시 발동할 수 있게 한다

핵심은 **manifest load = descriptor 등록 + scene 생성 준비**이지, 곧바로 모든 Gaussian 데이터를 읽는 것은 아니라는 점이다.

## 5. 프레임 단위 스트리밍 흐름

현재 프레임 흐름의 실질적인 분기점은 `MGStreamViewer::onRender()`다.

여기서 viewer는 먼저 `ViewerContext`를 만든다.

- `camera_pos`
- `camera_forward`
- `camera_up`
- `current_phase`
- `app_time_sec`
- `dt_sec`
- `frame_index`

그 후 `RenderingSystem::tickStreaming(context)`를 호출한다.

### manifest가 없는 경우

- `swapManager->hasManifest()`가 false
- `ensureManualGpuResidency()` 실행
- scene instance가 참조하는 asset이 CPU resident이면 GPU 캐시에 올린다

### manifest가 있는 경우

- `GaussianView::lastSkippedInstances()`를 stats에 반영
- `SwapManager::tick(context)` 실행
- 이 안에서 CPU/GPU residency가 정책적으로 갱신된다

즉 Version 1부터는 **렌더링 직전 residency 보장 로직이 `RenderingSystem` 내부의 streaming tick으로 승격**된 상태다.

## 6. 정책 평가 흐름 (`SwapPolicy`)

`SwapPolicy::evaluate(...)`는 현재 카메라와 phase를 바탕으로 세 집합을 계산한다.

- `required_gpu`
  - 이번 프레임에 GPU에 있어야 하는 asset
- `warm_cpu`
  - CPU 메모리에 남겨두고 싶은 asset
- `protected_assets`
  - pin 설정 등으로 eviction 대상에서 제외되는 asset

평가 순서는 대략 다음과 같다.

1. manifest asset 전체를 순회
   - `pin_gpu`면 `required_gpu` + `protected_assets`
   - `pin_cpu` 또는 사용자 pin이면 `warm_cpu` + `protected_assets`
2. `global.warm_rule_assets_cpu == true`이면
   - manifest rule 어디에서든 참조된 asset 전체를 `warm_cpu`에 넣음
3. rule 순회
   - `phase`
     - 현재 phase가 맞으면 `required`, `warm`, tag 기반 selector 반영
   - `camera_bounds`
     - 카메라가 bounds 안에 있을 때만 `required`, `warm` 반영
   - `distance`
     - 카메라와 asset bounds 사이 거리가 threshold 이하일 때 반영

이 구조 덕분에 지금은 다음 같은 정책이 가능하다.

- manifest가 참조하는 asset 전체는 CPU RAM에 warm 유지
- 그중 현재 카메라 셀에서 `required`로 지정된 일부만 VRAM에 업로드

즉 "CPU는 넓게, GPU는 좁게" 유지하는 흐름이 설계대로 들어와 있다.

## 7. swap 실행 흐름 (`SwapManager`)

`SwapManager::tick(...)`는 정책 결과를 실제 상태 전이로 옮긴다.

실행 순서는 다음과 같다.

1. 이전 프레임에 끝난 async CPU load 결과를 회수
   - 성공: `ResourceManager::completeCpuLoad(...)`
   - 실패: `ResourceManager::failCpuLoad(...)`
2. `SwapPolicy::evaluate(...)`로 현재 `PolicyResult` 계산
3. `updateDesiredStates(result)`
   - manifest의 모든 asset에 대해 `desired_cpu` 플래그 갱신
4. `updateEvictableTimers(result, context)`
   - 지금 더 이상 필요하지 않은 asset의 eviction 시작 시각 기록
5. `scheduleCpuLoads(result)`
   - 필요한 CPU asset을 worker queue에 넣음
   - RAM budget이 모자라면 먼저 CPU eviction 시도
6. `scheduleGpuUploads(result)`
   - `required_gpu`를 priority 순으로 정렬
   - CPU resident 확인 후 GPU 업로드
   - `estimated_gpu_bytes == 0`이면 per-frame upload budget 체크를 건너뛰고 업로드 허용
7. `scheduleGpuEvictions(result, context)`
   - hysteresis 경과 + 비보호 + 비required asset을 GPU에서 내림
8. `scheduleCpuEvictions(result, context)`
   - hysteresis 경과 + 비보호 + 비desired asset을 CPU에서 내림

이번 버전에서 주의할 현재 semantics는 다음과 같다.

- `max_gpu_evictions_per_frame == 0`이면 unlimited로 해석
- `max_cpu_evictions_per_frame == 0`이면 unlimited로 해석
- `estimated_gpu_bytes == 0`이면 upload budget 강제는 약해진다
  - small manifest에는 편하지만, 큰 asset에서는 VRAM spike 위험이 있다

## 8. CPU asset 상태 모델 (`ResourceManager`)

`ResourceManager`는 이제 descriptor registry와 CPU residency state machine을 함께 가진다.

현재 CPU 상태는 다음 다섯 가지다.

- `Unloaded`
- `Loading`
- `Resident`
- `EvictQueued`
- `Failed`

중요한 동작은 다음과 같다.

- `registerManifest(...)`
  - descriptor만 등록
- `beginCpuLoad(id)`
  - `Loading`, `Resident`, `EvictQueued` 상태에서는 재진입 차단
- `completeCpuLoad(id, field)`
  - 실제 `GaussianField`와 measured byte 수를 기록하고 `Resident`로 전이
- `requestCpuEvict(id)`
  - `Resident`에서만 `EvictQueued`로 전이
- `evictCpuNow(id)`
  - `EvictQueued`에서만 실제 해제
- `getCpuFieldShared(id)`
  - 현재 CPU asset 접근의 표준 경로

특히 `AssetRecord`의 lifetime note가 중요하다.

- 내부 보관은 `shared_ptr`
- record 상태가 `EvictQueued` 또는 `Unloaded`로 바뀌어도
- 외부가 `getCpuFieldShared()`로 잡고 있는 참조가 남아 있으면 실제 `GaussianField` 객체는 마지막 참조가 사라질 때까지 살아 있다

즉 "registry 상에서는 unload 되었는데, 이미 잡힌 shared_ptr 때문에 객체는 잠깐 더 살아 있음"이 현재 의도된 동작이다.

## 9. 렌더링 핫패스 (`GaussianView::onRenderIBR()`)

렌더링 자체의 큰 흐름은 Phase 0와 비슷하지만, Version 1에서는 swap과 더 강하게 연결된다.

핫패스 순서는 다음과 같다.

1. output buffer clear
2. GPU field가 있는 instance만 세서 `totalCount` 계산
3. count가 0이면 즉시 return
4. 영구 world buffer를 필요 시 확장
5. view / projection / camera position 업로드
6. instance 순회
   - GPU field가 없으면 skip
   - asset id가 있는 instance가 skip되면 `lastSkippedInstances_` 증가
7. 각 instance를 world buffer에 append
   - `TransformPosRotScaleToWorld(...)`
   - `appendSHsToWorld(...)`
8. `CudaRasterizer::Rasterizer::forward(...)` 호출
9. 결과를 render target에 복사
10. `current_world_gausians_count` 리셋

여기서 `lastSkippedInstances_`는 다시 `RenderingSystem::tickStreaming()`으로 전달되어 swap 통계에 반영된다.  
즉 현재는 "렌더에서 못 그린 인스턴스 수"가 streaming 상태 진단용 숫자로 되돌아간다.

## 10. UI 관점에서 달라진 흐름

현재 viewer UI에는 이전보다 다음 흐름이 추가되어 있다.

- `Load Manifest`
  - manifest JSON 로딩
- `Current Phase`
  - 현재 phase 문자열 직접 편집
- `Create Manifest Instances`
  - manifest asset 중 scene에 빠진 instance만 자동 생성
- `Focus Manifest Camera`
  - manifest bounds 중앙 쪽으로 카메라를 다시 맞춤
- `Resource Browser` 통계
  - CPU resident bytes
  - GPU resident bytes
  - current phase
  - required GPU count
  - warm CPU count
  - disk loads / uploads / evictions / swap hits / misses
- asset tile 상태 표시
  - `Unloaded`
  - `Loading`
  - `CPU`
  - `Evicting`
  - `GPU`

즉 UI도 더 이상 단순 asset browser가 아니라, **현재 스트리밍 상태를 관찰하는 패널** 역할을 같이 하게 되었다.

## 11. 디버깅 시작점

현재 버전에서 문제가 생겼을 때는 증상에 따라 아래 순서로 들어가는 것이 가장 빠르다.

### manifest가 안 읽히는 경우

1. `MGStreamViewer::loadManifestFile(...)`
2. `ManifestStore::load(...)`
3. `ResourceManager::registerManifest(...)`

### 카메라 위치에 따라 로딩 셋이 이상한 경우

1. `MGStreamViewer::focusCameraOnManifest()`
2. `MGStreamViewer::onRender()`
3. `SwapPolicy::evaluate(...)`

### CPU는 있는데 GPU가 안 올라가는 경우

1. `SwapManager::scheduleGpuUploads(...)`
2. `ResourceManager::getCpuFieldShared(...)`
3. `GPUResourceManager`

### asset이 내려가지 않거나 너무 빨리 내려가는 경우

1. `SwapManager::updateEvictableTimers(...)`
2. `SwapManager::hysteresisFor(...)`
3. `scheduleGpuEvictions(...)`
4. `scheduleCpuEvictions(...)`

### 화면이 비어 있는데 instance는 있는 경우

1. `GaussianView::onRenderIBR()`
2. `lastSkippedInstances_`
3. `RenderGaussianInstance::getGPUField()`
4. `RenderingSystem::tickStreaming(...)`

## 12. 다음 버전 문서를 쓸 때 규칙

다음 흐름 문서는 아래 규칙으로 추가하는 것을 권장한다.

- 파일명 패턴:
  - `docs/M_GStream_code_flow_v2_ko.md`
  - `docs/M_GStream_code_flow_v3_ko.md`
- 각 문서 상단에 반드시 기록:
  - 작성일
  - 기준 브랜치
  - 이전 버전 대비 핵심 변화 3~5개
- 기존 문서는 덮어쓰지 말고 남긴다
- 새 버전 문서에는 항상 아래 두 항목을 넣는다
  - "이전 버전 대비 무엇이 바뀌었나"
  - "디버깅 시작점이 어디로 이동했나"

이렇게 해야 나중에 흐름이 복잡해져도 `phase0 -> v1 -> v2 -> v3` 순서로 따라가며 어느 시점에 구조가 바뀌었는지 추적할 수 있다.
