# headless window/backend와 앱 진입점 상세

이 문서는 M2에서 `DISPLAY` 없는 환경에서도 finite offscreen render를 수행할 수 있도록 바뀐 `Window`와 app entry를 정리한다.

## 디렉터리: `src/core/graphics`

### 파일: `src/core/graphics/CMakeLists.txt`

#### 초기 코드

```cmake
if (NOT WIN32)
    target_link_libraries(${PROJECT_NAME}
        rt m dl X11 pthread Xrandr Xinerama Xxf86vm Xcursor
    )
endif()
```

#### 현재 코드

```cmake
if (NOT WIN32)
    target_link_libraries(${PROJECT_NAME}
        rt m dl X11 pthread Xrandr Xinerama Xxf86vm Xcursor
    )
    if (EGL_FOUND)
        target_link_libraries(${PROJECT_NAME} ${EGL_LIBRARIES})
    endif()
endif()
```

#### 바뀐 이유

- M2에서 direct EGL pbuffer context를 쓰기 시작했으므로, Linux 빌드에 `libEGL` 링크가 실제로 들어가야 했다.
- 이 링크가 없으면 소스에서 EGL 분기를 추가해도 link 단계에서 닫히지 않는다.

### 파일 묶음

- `src/core/graphics/Window.hpp`
- `src/core/graphics/Window.cpp`

#### 초기 코드

```cpp
Window::AutoInitializer::AutoInitializer(const WindowArgs & args) : _useGUI(!args.no_gui && !args.offscreen)
{
    if (windowCounter == 0)
    {
        glfwSetErrorCallback(glfwErrorCallback);
        if (!glfwInit())
            SIBR_ERR << "cannot init glfw" << std::endl;
    }
}
```

```cpp
glfwWindowHint(GLFW_CONTEXT_CREATION_API,
    (args.offscreen) ? GLFW_EGL_CONTEXT_API : GLFW_NATIVE_CONTEXT_API);
_glfwWin = GLFWwindowptr(glfwCreateWindow(...), glfwDestroyWindow);
```

#### 현재 코드

```cpp
static bool hasDisplayServerEnvironment()
{
    const char* display = std::getenv("DISPLAY");
    ...
}

static bool shouldUseDirectHeadlessEGL(const WindowArgs& args)
{
    return args.offscreen && !hasDisplayServerEnvironment();
}

static EGLDisplay createHeadlessEGLDisplay()
{
    ...
    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}
```

```cpp
if (shouldUseDirectHeadlessEGL(args)) {
    _usesHeadlessEGL = true;
    _eglDisplay = createHeadlessEGLDisplay();
    ...
    _eglSurface = eglCreatePbufferSurface(_eglDisplay, config, surfaceAttribs);
    _eglContext = eglCreateContext(_eglDisplay, config, EGL_NO_CONTEXT, attrs);
} else {
#ifdef GLFW_PLATFORM_NULL
    if (args.offscreen) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
    }
#endif
    _glfwWin = GLFWwindowptr(glfwCreateWindow(...), glfwDestroyWindow);
}
```

```cpp
inline void Window::makeContextCurrent(void) {
#ifdef GLEW_EGL
    if (_usesHeadlessEGL) {
        eglMakeCurrent(_eglDisplay, _eglSurface, _eglSurface, _eglContext);
        return;
    }
#endif
    glfwMakeContextCurrent(_glfwWin.get());
}
```

#### 바뀐 이유

- 초기 상태의 `offscreen`은 숨겨진 GLFW 창을 띄우는 수준이어서, 실제 display server가 없는 SSH/no-display 환경에서는 충분하지 않았다.
- M2에서는 `DISPLAY`/`WAYLAND_DISPLAY`가 없는 경우 GLFW를 거치지 않고 direct EGL pbuffer context를 만드는 경로를 추가했다.
- 동시에 `size`, `position`, `isOpened`, `swapBuffer`, `setVsynced`, `enableCursor` 같은 window API도 headless EGL backend에서 동작하도록 보강했다.
- 이렇게 해야 viewer/app 상위 로직은 기존 `Window` 인터페이스를 그대로 쓰면서 headless 경로만 내부에서 갈라질 수 있다.

## 디렉터리: `src/projects/extended_gaussian/apps/extended_gaussianViewer`

### 파일: `src/projects/extended_gaussian/apps/extended_gaussianViewer/main.cpp`

#### 초기 코드

```cpp
CommandLineArgs::parseMainArgs(ac, av);
BasicIBRAppArgs myArgs;

sibr::Window window("Extended Gaussian Viewer", sibr::Vector2i(50, 50), myArgs);
ExtendedGaussianViewer viewer(window, false);

while (window.isOpened())
{
    sibr::Input::poll();
    ...
    viewer.onUpdate(sibr::Input::global());
    viewer.onRender(window);
    viewer.onSwapBuffer(window);
}
```

#### 현재 코드

```cpp
struct ExtendedGaussianViewerAppArgs : virtual BasicIBRAppArgs {
    Arg<std::string> manifest = { "manifest", "", "path to a manifest json file" };
    Arg<bool> headless = { "headless", "run a finite offscreen render loop and exit" };
    Arg<int> render_width = { "render-width", 1280, ... };
    Arg<int> render_height = { "render-height", 720, ... };
    Arg<std::string> snapshot = { "snapshot", "", ... };
    Arg<bool> wait_for_streaming_idle = { "wait-for-streaming-idle", ... };
    Arg<int> max_headless_frames = { "max-headless-frames", 600, ... };
};

int runHeadless(ExtendedGaussianViewer& viewer, Window& window, const ExtendedGaussianViewerAppArgs& args)
{
    ...
    viewer.onUpdate(Input::global());
    viewer.onRender(window);
    ...
    if (!viewer.captureGaussianViewSnapshot(snapshotPath)) {
        return EXIT_FAILURE;
    }
}
```

```cpp
if (myArgs.headless.get()) {
    myArgs.offscreen = true;
    myArgs.no_gui = true;
    myArgs.vsync = 0;
    myArgs.win_width = myArgs.render_width.get();
    myArgs.win_height = myArgs.render_height.get();
}
```

#### 바뀐 이유

- 초기 상태의 app entry는 영구 interactive loop만 있었고, snapshot만 찍고 종료하는 finite path가 없었다.
- M2에서는 `--headless`, `--manifest`, `--render-width`, `--render-height`, `--snapshot`, `--wait-for-streaming-idle`, `--max-headless-frames`를 추가해 one-shot offscreen 렌더 경로를 만들었다.
- interactive path와 headless path를 분리한 이유는 이후 M4+에서 server lifecycle이 생겨도 app entry가 단계별로 확장되기 쉽도록 하기 위해서다.

## 요약

- M2의 첫 축은 `Window` backend 자체를 no-display 환경까지 확장하는 것이고,
- 두 번째 축은 viewer app entry에 finite headless 실행 경로를 넣는 것이다.
- 이 둘이 합쳐져야 이후 실제 PNG snapshot smoke와 remote stream 캡처 경계 검증이 가능해진다.
