# `develop/windows-portable-bundle` 브랜치 변경 기록

작성일: 2026-04-08
대상 브랜치: `develop/windows-portable-bundle`
비교 기준: `main..HEAD`

## 1. 문서 목적

이 문서는 현재 작업 브랜치 `develop/windows-portable-bundle` 에서 `main` 대비 추가된 변경만 따로 기록한다.

즉 아래 내용은 전체 저장소 변경 이력이 아니라, **현재 브랜치에만 존재하는 커밋과 그 결과물**만 요약한 것이다.

## 2. 브랜치 범위 요약

`main...HEAD` 기준 변경 통계는 다음과 같다.

- 변경 파일 수: 46
- 추가 라인 수: 8298
- 삭제 라인 수: 361

현재 브랜치 변경은 크게 네 축으로 나뉜다.

1. manifest 기반 Gaussian swap 파이프라인 추가
2. CPU/GPU asset 수명주기와 렌더 hot path 후속 정리
3. Windows install/portable bundle 스크립트 및 runtime packaging 정리
4. 관련 한국어 문서, 리뷰 메모, 계획 문서 추가

## 3. 커밋 목록

`main..HEAD` 기준 커밋은 아래와 같다.

- `34482fd` `feat: add manifest-driven gaussian swap pipeline`
- `28107b8` `build: sync viewer runtime dlls after build`
- `e747cca` `feat: keep manifest rule assets warm on CPU`
- `aa72d3a` `chore: add local manifest presets`
- `1f1c262` `chore: cosmetic cleanup in renderer headers`
- `e91e4cf` `rename: normalize euler naming in scene/render path`
- `678288a` `refactor: extract SH append from GaussianView hot path`
- `d208d5a` `refactor: tighten cpu asset eviction state transitions`
- `e6f679b` `fix: treat zero eviction budget as unlimited and allow unknown-size upload`
- `9b74f04` `fix: restore renderer split follow-up corrections`
- `e9361fd` `docs: add versioned code flow snapshot v1`
- `edadf74` `docs: record phase1 OOM known issue`
- `d08f7bb` `docs: add windows portability work plan`
- `8824363` `docs: add windows portable bundle guide`
- `218f024` `build: add windows portable bundle scripts`
- `75da267` `build: install windows launcher with viewer app`
- `0944122` `docs: standardize windows install target usage`
- `4259f0d` `wip: back up windows portable bundle hardening`
- `395a399` `fix: tighten windows portable bundle packaging`
- `8777379` `docs: update windows portable bundle guide`

## 4. 변경 내용 요약

### 4.1 Manifest swap 파이프라인

이 브랜치에서 가장 큰 기능 추가는 manifest 기반 swap 흐름이다.

- manifest JSON을 읽고 asset rule을 관리하는 `ManifestStore`를 추가했다.
- 비동기 asset load를 담당하는 `AssetLoadWorker`를 추가했다.
- `SwapManager`, `SwapPolicy`를 추가해 CPU/GPU 상주 정책과 swap 흐름을 분리했다.
- viewer와 resource manager가 manifest 입력을 받아 asset warm-up 및 교체 흐름을 다룰 수 있게 확장됐다.
- local 테스트용 manifest preset도 함께 추가했다.

주요 파일:

- `src/projects/M_GStream/renderer/resource/ManifestStore.{hpp,cpp}`
- `src/projects/M_GStream/renderer/resource/AssetLoadWorker.{hpp,cpp}`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapManager.{hpp,cpp}`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapPolicy.{hpp,cpp}`
- `manifests/*.json`
- `tools/generate_cell_neighbor_manifest.py`

### 4.2 Asset 수명주기 / 렌더 경로 정리

Phase 1 swap 흐름을 받치기 위해 CPU/GPU asset ownership과 렌더 경로도 후속 정리됐다.

- `ResourceManager`가 CPU asset eviction 상태와 로딩 상태를 더 엄격하게 관리하도록 변경됐다.
- zero eviction budget을 사실상 unlimited로 취급하는 보정이 들어갔다.
- unknown-size upload를 허용하도록 GPU upload 경로가 보강됐다.
- `GaussianView` hot path에서 SH append 관련 로직을 분리해 구조를 정리했다.
- scene/render 경로의 Euler naming을 통일하는 rename이 포함됐다.

주요 파일:

- `src/projects/M_GStream/renderer/resource/ResourceManager.{hpp,cpp}`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.{hpp,cpp}`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/RenderingSystem.{hpp,cpp}`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.{hpp,cpp}`
- `src/projects/M_GStream/renderer/scene/GaussianInstance.{hpp,cpp}`
- `src/projects/M_GStream/renderer/ExtendedGaussianViewer.{hpp,cpp}`

### 4.3 Windows install / portable bundle 정리

브랜치 후반부에서는 Windows portable bundle 쪽 구현이 추가되고, 그 위에 follow-up hardening이 더해졌다.

- portable bundle 생성 스크립트 3종을 추가했다.
  - `build_windows_portable_bundle.ps1`
  - `package_windows_portable_bundle.ps1`
  - `check_windows_runtime.ps1`
- install tree에서 바로 viewer를 실행할 수 있는 launcher를 추가했다.
- viewer app install 단계가 runtime DLL 포함의 단일 진실 원천이 되도록 CMake install 로직을 정리했다.
- package 단계는 `install/` 수정 없이 번들링과 preflight만 수행하도록 정리했다.
- build tree config와 bundle executable 선택이 어긋나지 않도록 config-aware packaging을 추가했다.
- preflight가 `cudart`와 `xatlas`를 포함한 runtime 누락을 잡도록 강화했다.
- bundle launcher가 `selected_viewer_exe.txt`를 우선 읽도록 수정했다.

주요 파일:

- `src/projects/M_GStream/apps/M_GStreamViewer/CMakeLists.txt`
- `tools/windows/build_windows_portable_bundle.ps1`
- `tools/windows/package_windows_portable_bundle.ps1`
- `tools/windows/check_windows_runtime.ps1`
- `tools/windows/run_installed_viewer.cmd`
- `tools/windows/run_portable_bundle.cmd`

### 4.4 문서 / 리뷰 / 계획

이번 브랜치에는 구현과 함께 한국어 설계/리뷰 문서도 다수 추가됐다.

- 코드 흐름 스냅샷 문서 추가
- renderer/scene review notes 추가
- phase1 OOM known issue 기록
- Windows portability 작업 계획 문서 추가
- Windows portable bundle 사용 가이드 추가
- Windows portable bundle 리뷰 문서 및 후속 수정 계획 문서 추가

주요 파일:

- `docs/M_GStream_code_flow_v1_ko.md`
- `docs/M_GStream_renderer_scene_review_notes_ko.md`
- `docs/M_GStream_modification_log_ko.md`
- `docs/M_GStream_windows_portable_bundle_ko.md`
- `docs/M_GStream_windows_portable_bundle_review_ko.md`
- `plan/2026-04-08-windows-portable-bundle.md`
- `plan/2026-04-08-windows-portable-bundle-review-followups.md`

## 5. 이번 브랜치의 최종 portable bundle 후속 반영

브랜치 최신 상태에서 portable bundle 쪽 추가 보정은 아래 두 커밋에 모였다.

- `395a399` `fix: tighten windows portable bundle packaging`
- `8777379` `docs: update windows portable bundle guide`

이 두 커밋에서 정리한 핵심은 다음과 같다.

- 기본 portable build root를 `build/`로 고정
- single-config build tree의 `CMAKE_BUILD_TYPE`와 요청 `-Config` 불일치 즉시 실패
- package 단계가 요청 config에 맞는 viewer executable만 선택
- bundle root에 `selected_viewer_exe.txt` 기록
- preflight가 선택된 executable suffix 기준으로 runtime DLL 검사
- `xatlas*.dll` 검사를 runtime 필수 항목에 추가
- explicit manifest path 오류를 `exit 3`으로 유지
- 문서가 install 책임과 package 책임을 현재 구현에 맞게 설명하도록 갱신

## 6. 브랜치 기준 검증 메모

현재 브랜치 최신 상태에서 확인한 portable bundle 관련 검증은 아래와 같다.

- 기본 인자 `tools/windows/build_windows_portable_bundle.ps1`
  - `build/` 기준 `RelWithDebInfo` build -> install -> package -> runtime-only preflight 통과
  - bundle이 `M_GStreamViewer_app_rwdi.exe`를 선택함
- `tools/windows/build_windows_portable_bundle.ps1 -BuildRoot .\\build-ninja -Config Debug`
  - single-config `Debug` build tree 기준 build -> install -> package -> runtime-only preflight 통과
  - bundle이 `M_GStreamViewer_app_d.exe`를 선택함
- `tools/windows/check_windows_runtime.ps1`
  - 없는 manifest path는 `exit 3`
  - asset data root 누락 manifest는 `exit 2`
  - `xatlas_rwdi.dll` 누락 install 복사본은 `exit 1`
- `tools/windows/package_windows_portable_bundle.ps1`
  - package 전후 `install/bin` 스냅샷 비교 결과 변경 없음
  - bundle root `selected_viewer_exe.txt`에 실제 선택 executable이 기록됨

## 7. 현재 브랜치에서 수정된 파일 목록

`main...HEAD` 기준 변경 파일은 아래와 같다.

- `.gitignore`
- `CLAUDE.md`
- `docs/M_GStream_code_flow_v1_ko.md`
- `docs/M_GStream_modification_log_ko.md`
- `docs/M_GStream_renderer_scene_review_notes_ko.md`
- `docs/M_GStream_windows_portable_bundle_ko.md`
- `docs/M_GStream_windows_portable_bundle_review_ko.md`
- `manifests/mc_small_aerial_c36_cell20_neighbors_only.json`
- `manifests/mc_small_aerial_c36_neighbors_3x3.json`
- `manifests/mc_small_aerial_c36_neighbors_3x3_cpu36_gpu9.json`
- `plan/2026-04-08-windows-portable-bundle-review-followups.md`
- `plan/2026-04-08-windows-portable-bundle.md`
- `src/projects/M_GStream/apps/M_GStreamViewer/CMakeLists.txt`
- `src/projects/M_GStream/renderer/ExtendedGaussianViewer.cpp`
- `src/projects/M_GStream/renderer/ExtendedGaussianViewer.hpp`
- `src/projects/M_GStream/renderer/resource/AssetLoadWorker.cpp`
- `src/projects/M_GStream/renderer/resource/AssetLoadWorker.hpp`
- `src/projects/M_GStream/renderer/resource/GaussianLoader.cpp`
- `src/projects/M_GStream/renderer/resource/GaussianLoader.hpp`
- `src/projects/M_GStream/renderer/resource/ManifestStore.cpp`
- `src/projects/M_GStream/renderer/resource/ManifestStore.hpp`
- `src/projects/M_GStream/renderer/resource/ResourceManager.cpp`
- `src/projects/M_GStream/renderer/resource/ResourceManager.hpp`
- `src/projects/M_GStream/renderer/scene/GaussianInstance.cpp`
- `src/projects/M_GStream/renderer/scene/GaussianInstance.hpp`
- `src/projects/M_GStream/renderer/scene/GaussianScene.cpp`
- `src/projects/M_GStream/renderer/scene/GaussianScene.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/RenderGaussianInstance.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/RenderingSystem.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/RenderingSystem.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapManager.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapManager.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapPolicy.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapPolicy.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/gpu_resource_manager/GPUGaussianField.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/gpu_resource_manager/GPUGaussianField.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.hpp`
- `tools/generate_cell_neighbor_manifest.py`
- `tools/windows/build_windows_portable_bundle.ps1`
- `tools/windows/check_windows_runtime.ps1`
- `tools/windows/package_windows_portable_bundle.ps1`
- `tools/windows/run_installed_viewer.cmd`
- `tools/windows/run_portable_bundle.cmd`

## 8. 남겨 둔 메모

현재 브랜치는 이름상 Windows portable bundle 작업 브랜치지만, 실제 diff 범위에는 그보다 앞선 manifest swap / resource manager / rendering follow-up 변경도 함께 포함돼 있다.

즉 이 브랜치를 리뷰하거나 `main`으로 보낼 때는, 아래 둘을 분리해서 보는 편이 좋다.

- 기능 축: manifest-driven swap + resource/rendering follow-up
- 배포/운영 축: Windows install + portable bundle hardening
