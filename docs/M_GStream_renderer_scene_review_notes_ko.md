# MGStream Renderer/Scene Re-review Notes

## 목적

이 문서는 2026-04-08 기준으로 **현재 PR 범위에서 의도적으로 제외한 `renderer/scene` 로컬 변경**을 다시 검토할 때,
어떤 부분을 중점적으로 확인해야 하는지 정리한 노트다.

이번 PR에는 다음 성격의 변경만 포함했다.

- manifest rule asset을 CPU warm 대상으로 유지하는 정책
- manifest preset 및 로컬 실행용 보조 파일

반대로 아래 문서에서 다루는 변경은 **동작 영향 범위가 더 넓거나, 회귀 가능성을 추가 검증해야 하므로 보류**했다.

## 재검토 대상 파일

### CPU asset lifecycle / swap 관련

- `src/projects/M_GStream/renderer/resource/ResourceManager.hpp`
- `src/projects/M_GStream/renderer/resource/ResourceManager.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapManager.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/RenderingSystem.cpp`
- `src/projects/M_GStream/renderer/MGStreamViewer.cpp`

### scene transform / hot path refactor 관련

- `src/projects/M_GStream/renderer/scene/GaussianInstance.hpp`
- `src/projects/M_GStream/renderer/scene/GaussianInstance.cpp`
- `src/projects/M_GStream/renderer/scene/GaussianScene.hpp`
- `src/projects/M_GStream/renderer/scene/GaussianScene.cpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.hpp`
- `src/projects/M_GStream/renderer/subsystem/rendering_system/GaussianView.cpp`

### 문서성 변경

- `src/projects/M_GStream/renderer/subsystem/rendering_system/SwapPolicy.hpp`

## 그룹별 검토 포인트

### 1. CPU residency 상태머신 변경

핵심 변경:

- `CpuState::EvictQueued` 추가
- `requestCpuEvict()` 호출 시 즉시 `EvictQueued` 전이
- `evictCpuNow()`가 `EvictQueued` 상태에서만 실제 해제
- UI가 `EvictQueued`를 `"Evicting"`으로 표시

주의할 점:

- 기존 코드가 `Resident -> Unloaded`를 바로 가정하던 부분이 있는지 확인
- `snapshotAssets()`가 `cpu_field != nullptr`가 아니라 `cpu_state == Resident`를 기준으로 `cpu_resident`를 계산해도 UI/로직에 모순이 없는지 확인
- `getCpuFieldShared()`로 얻은 `shared_ptr`가 남아 있는 동안 실제 메모리가 살아있다는 점과, 상태값은 이미 `Unloaded`로 바뀔 수 있다는 점이 UI/렌더링에서 혼동되지 않는지 확인
- `beginCpuLoad()`가 `EvictQueued`일 때 재진입을 막는 것이 실제 의도와 맞는지 확인

반드시 다시 봐야 할 회귀 후보:

- CPU evict 직후 같은 frame 또는 직후 frame에서 같은 asset이 다시 required가 되는 경우
- `desired_cpu == false`인데 아직 `shared_ptr` 참조가 남아 있는 경우 메모리 accounting이 정확한지
- `GPU resident`인 asset을 CPU evict 대상에서 여전히 잘 제외하는지

## 2. ResourceManager API 정리

핵심 변경:

- `getDescriptor()` 제거
- raw pointer 기반 `getField()` 제거
- `getCpuFieldShared()` 중심으로 접근 통일

주의할 점:

- 제거된 API를 직접 또는 간접으로 기대하던 호출부가 더 없는지 전체 검색으로 확인
- `RenderingSystem`, `MGStreamViewer`, `SwapManager` 외에 숨은 호출부가 없는지 확인
- `getCpuFieldShared()`를 받은 후 `.get()`으로 raw pointer를 넘기는 경로가 object lifetime을 충분히 보장하는지 확인

권장 확인:

- `rg "getField\\(|getDescriptor\\(" src/projects/M_GStream`
- `rg "getCpuFieldShared\\(" src/projects/M_GStream`

## 3. swap budget semantics 변경

핵심 변경:

- `max_gpu_evictions_per_frame == 0`을 "후보 수만큼"이 아니라 "무제한"으로 해석
- `max_cpu_evictions_per_frame == 0`도 같은 의미로 해석
- `estimated_gpu_bytes == 0`인 asset은 upload budget 체크를 건너뛰고 업로드 허용
- priority / hysteresis 조회 기준을 `ResourceManager`가 아닌 manifest descriptor로 통일

주의할 점:

- budget `0`의 의미를 바꾸는 것은 설정 호환성에 직접 영향이 있다
- `estimated_gpu_bytes == 0` 허용은 small manifest에는 편하지만, large manifest에서는 VRAM spike를 만들 수 있다
- descriptor 조회 기준을 manifest로 옮기면 runtime에서 갱신된 CPU-side metadata와 어긋날 가능성이 없는지 확인해야 한다

반드시 다시 봐야 할 회귀 후보:

- manifest에 `estimated_gpu_bytes`가 없는 상태에서 대량 asset이 한 frame에 GPU 업로드되는 경우
- `priority`를 runtime에서 수정할 계획이 있다면 manifest 기준 조회가 future-proof한지
- hysteresis가 manifest bounds / descriptor 값과 일관되게 동작하는지

권장 검증:

- `target_vram_mb = 0`, `max_gpu_evictions_per_frame = 0`
- `target_vram_mb > 0`, `max_gpu_evictions_per_frame = 1`
- `estimated_gpu_bytes` 없는 manifest
- `estimated_gpu_bytes`를 넣은 manifest

## 4. scene transform API rename (`eular` -> `euler`)

핵심 변경:

- `GaussianInstance` / `GaussianScene` / `GaussianView`에서 `getEular`, `setEular`, `getEularRef`, `eular_angle`를 `Euler` 표기로 변경

주의할 점:

- 저장소에는 과거에 instance lifecycle callback 쪽 load-bearing typo가 있었고, 이런 public API 이름은 부분 rename이 아니라 인터페이스-구현-호출부를 함께 바꾸는 일괄 리네이밍으로만 정리해야 한다
- 이번 rename은 보기에는 단순 정리지만, scene/UI/render hot path를 가로지르는 public API 이름을 바꾼다
- 외부 직렬화, reflection, editor extension이 없더라도 grep 범위를 놓치면 컴파일은 돼도 런타임 의미가 바뀔 수 있다

현재 판단:

- 이 rename은 별도 커밋 또는 별도 PR로 다루는 편이 낫다
- 특히 기능 PR 안에 섞으면 "정책 변경"과 "이름 정리"가 같이 리뷰되어 diff 해석이 어려워진다

권장 확인:

- `rg "getEular|getEuler|setEular|setEuler|eular_angle|euler_angle" src/projects/M_GStream`

## 5. GaussianView hot path refactor

핵심 변경:

- `TransformPosRotScaleSHsToWorld(...)`를
  `TransformPosRotScaleToWorld(...)` + `appendSHsToWorld(...)` 호출 구조로 분리
- log 출력 정리
- 주석 추가

주의할 점:

- 이름만 바뀐 것처럼 보여도, render hot path에 있는 함수 분리는 미세한 동작 차이를 만들 수 있다
- SH buffer append 시점이 기존과 완전히 동일한지 확인해야 한다
- transform 계산 순서가 바뀌지 않았는지, 특히 rotation order가 동일한지 확인해야 한다

반드시 다시 봐야 할 회귀 후보:

- 동일 scene에서 frame-to-frame 결과가 이전과 다르지 않은지
- SH coeff append offset이 누락/중복되지 않는지
- scale / rotation / translation 조합에서 instance 배치가 바뀌지 않는지

권장 검증:

- 단일 instance 하나만 둔 장면
- 서로 다른 회전값을 가진 instance 여러 개
- camera 이동 전후 동일 frame capture 비교

## 6. UI/adapter 변경

핵심 변경:

- `MGStreamViewer.cpp`에서 `EvictQueued` 라벨 표시
- asset detail 조회를 `getCpuFieldShared()`로 변경
- rotation editor가 `getEulerRef()`를 사용하도록 변경

주의할 점:

- 이 변경은 대부분 adapter 성격이지만, 기반 rename / 상태머신 변경에 묶여 있으므로 단독으로 보기 어렵다
- UI에서 `"missing"` 표기가 `cpu_state`와 `cpu_field`의 엇갈린 상태를 잘 표현하는지 확인해야 한다

## 추천 분리 기준

### 지금 PR에 넣지 않는 편이 좋은 항목

- `eular -> euler` rename 전체
- `GaussianView` hot path refactor
- `ResourceManager` 상태머신 변경
- `SwapManager` budget/eviction semantics 변경

### 별도 PR로 만들기 좋은 항목

- `rename: normalize euler naming in scene/render path`
- `refactor: tighten cpu asset eviction state transitions`
- `fix: treat zero eviction budget as unlimited`

## 최소 검증 체크리스트

재검토 후 실제로 머지 후보로 올리려면 최소한 아래를 다시 확인하는 편이 좋다.

1. `M_GStreamViewer_app` 빌드 성공
2. manifest 로드 후 asset tile의 CPU/GPU state 라벨이 기대대로 변하는지 확인
3. 카메라 이동 시 `Required GPU`, `CPU Resident`, `GPU Resident` 수치가 논리적으로 맞는지 확인
4. instance rotation 조작 시 배치가 이전과 동일하게 보이는지 확인
5. CPU eviction 후 다시 required 될 때 reload가 정상 동작하는지 확인
6. VRAM / RAM budget을 0과 양수로 각각 바꿔도 비정상 spike나 stuck state가 없는지 확인

## 결론

현재 남아 있는 로컬 변경은 "리뷰 없이 바로 현재 PR에 얹기 좋은 사소한 정리"가 아니라,
대부분이 다음 중 하나에 해당한다.

- 상태머신 변경
- hot path refactor
- public-ish API rename
- budget semantics 변경

따라서 이번 PR에는 넣지 않고, 기능별로 쪼개서 다시 검토하는 것이 안전하다.
