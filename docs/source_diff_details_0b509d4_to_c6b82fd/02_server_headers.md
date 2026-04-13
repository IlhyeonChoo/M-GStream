# `renderer/server` 헤더 소유권 상세

이 문서는 M3에서 `renderer/server` 헤더들이 어느 라이브러리 소속인지 명확하게 분리한 변경을 정리한다.

## 디렉터리: `src/projects/extended_gaussian/renderer/server`

### 파일: `src/projects/extended_gaussian/renderer/server/Config.hpp`

#### 초기 코드

```text
기준 커밋 `0b509d4` 에는 파일이 없었다.
```

#### 현재 코드

```cpp
#pragma once

#include <projects/extended_gaussian/renderer/Config.hpp>

#ifdef SIBR_OS_WINDOWS
#ifndef SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT
#if defined(SIBR_EXTENDED_GAUSSIAN_SERVER_STATIC_DEFINE)
#define SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT
#elif defined(SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORTS)
#define SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT __declspec(dllexport)
#else
#define SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT __declspec(dllimport)
#endif
#endif
...
#endif
```

#### 바뀐 이유

- M2까지는 server 관련 타입도 renderer shared library의 export macro를 그대로 사용했다.
- M3에서 `extended_gaussian_server`라는 별도 target이 생겼으므로, ABI 소유권도 같이 분리해야 했다.
- 특히 Windows에서는 static/shared build에 따라 import/export 처리가 달라지므로 전용 config header가 필요하다.

### 파일: `src/projects/extended_gaussian/renderer/server/ServerProtocol.hpp`

#### 초기 코드

```cpp
#pragma once

#include <projects/extended_gaussian/renderer/Config.hpp>
...
struct SIBR_EXTENDED_GAUSSIAN_EXPORT ServerOptions {
    ...
};
...
SIBR_EXTENDED_GAUSSIAN_EXPORT ServerOptions ParseServerOptions(const CommandLineArgs& args);
SIBR_EXTENDED_GAUSSIAN_EXPORT ParseControlMessageResult ParseControlMessageJson(const std::string& payload);
```

#### 현재 코드

```cpp
#pragma once

#include "Config.hpp"
...
struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ServerOptions {
    ...
};
...
SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ServerOptions ParseServerOptions(const CommandLineArgs& args);
SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ParseControlMessageResult ParseControlMessageJson(const std::string& payload);
```

#### 바뀐 이유

- `ServerOptions`, `RemoteCameraPose`, `ControlMessage`, parser API는 논리적으로 server 계층 계약인데, 초기 상태에서는 renderer 라이브러리 심볼처럼 선언돼 있었다.
- M3에서는 wire contract 자체를 바꾸지 않고, “이 심볼이 어느 타깃에 속하는가”만 바로잡았다.
- 로컬 `Config.hpp`를 포함하도록 바꾼 것도 같은 이유다. 이제 이 헤더는 renderer shared lib가 아니라 server target의 public contract가 된다.

### 파일: `src/projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp`

#### 초기 코드

```cpp
#pragma once

#include "ServerProtocol.hpp"
...
SIBR_EXTENDED_GAUSSIAN_EXPORT bool ValidateRemoteCameraPose(...);
SIBR_EXTENDED_GAUSSIAN_EXPORT bool TryBuildInputCamera(...);
SIBR_EXTENDED_GAUSSIAN_EXPORT RemoteCameraPose ExportRemoteCameraPose(...);
```

#### 현재 코드

```cpp
#pragma once

#include "ServerProtocol.hpp"
...
SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT bool ValidateRemoteCameraPose(...);
SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT bool TryBuildInputCamera(...);
SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT RemoteCameraPose ExportRemoteCameraPose(...);
```

#### 바뀐 이유

- `CameraPoseAdapter` 역시 renderer hot path API가 아니라 remote stream/server 준비 계층에 속한다.
- 따라서 `ServerProtocol.hpp`와 같은 기준으로 심볼 소유권을 server target 쪽으로 맞춰야 이후 M4+에서 링크 경계가 뒤섞이지 않는다.
- 데이터 검증 로직이나 카메라 변환 규칙은 M3에서 바꾸지 않았다. 이 단계는 ABI 정리 단계다.

## 요약

- M3의 헤더 변경은 기능 변경이 아니라 **심볼 소유권 분리**가 본질이다.
- `renderer/server` public API는 이제 `extended_gaussian_server`의 public contract로 해석할 수 있다.
- 이 정리가 있어야 이후 Windows/Linux 모두에서 server target을 독립적으로 다루기 쉬워진다.
