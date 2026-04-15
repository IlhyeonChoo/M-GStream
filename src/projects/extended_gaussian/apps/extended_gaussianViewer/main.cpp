#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <memory>
#include <string>

#include <core/graphics/Window.hpp>
#include <core/system/CommandLineArgs.hpp>
#include "projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp"
#include "projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.hpp"
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
#include "projects/extended_gaussian/renderer/subsystem/rendering_system/SwapManager.hpp"
#include "projects/extended_gaussian/renderer/server/CameraPoseAdapter.hpp"
#include "projects/extended_gaussian/renderer/server/RemoteStreamServer.hpp"
#include "projects/extended_gaussian/renderer/server/ServerProtocol.hpp"
#endif

using namespace sibr;

namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;

void handleProcessSignal(int /*signal_number*/)
{
    g_shutdown_requested = 1;
}

bool shutdownRequested()
{
    return g_shutdown_requested != 0;
}

struct ExtendedGaussianViewerAppArgs : virtual BasicIBRAppArgs {
    Arg<std::string> manifest = { "manifest", "", "path to a manifest json file" };
    Arg<bool> headless = { "headless", "run without a visible window; without --server it renders a finite snapshot and exits" };
    Arg<int> render_width = { "render-width", 1280, "offscreen render width for headless mode" };
    Arg<int> render_height = { "render-height", 720, "offscreen render height for headless mode" };
    Arg<std::string> snapshot = { "snapshot", "", "png file path for headless snapshot output" };
    Arg<bool> wait_for_streaming_idle = { "wait-for-streaming-idle", "wait until manifest streaming queues drain before capturing" };
    Arg<int> max_headless_frames = { "max-headless-frames", 600, "maximum number of frames to render in headless mode" };
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
    Arg<bool> server = { "server", "start the remote browser stream HTTP server" };
    Arg<std::string> listen_host = { "listen-host", "127.0.0.1", "HTTP listen host for remote browser streaming" };
    Arg<int> listen_port = { "listen-port", 8080, "HTTP listen port for remote browser streaming" };
    Arg<std::string> bind = { "bind", "", "alias for --listen-host" };
    Arg<int> port = { "port", 0, "alias for --listen-port" };
    Arg<int> stream_width = { "stream-width", 1280, "output MJPEG stream width" };
    Arg<int> stream_height = { "stream-height", 720, "output MJPEG stream height" };
    Arg<int> stream_fps = { "stream-fps", 15, "target MJPEG stream FPS" };
    Arg<std::string> www_root = { "www-root", "", "override directory for remote stream static assets" };
#endif
};

#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
RendererHealthSnapshot makeRendererHealthSnapshot(const ExtendedGaussianViewer& viewer)
{
    RendererHealthSnapshot snapshot;
    const RenderingSystem* rendering_system = viewer.getRenderingSystem();
    snapshot.initialized = rendering_system != nullptr;
    snapshot.has_manifest = rendering_system != nullptr && rendering_system->hasManifest();
    snapshot.frame_index = viewer.getFrameIndex();
    snapshot.app_time_sec = viewer.getAppTimeSeconds();

    sibr::InputCamera gaussian_view_camera;
    std::string camera_error;
    if (viewer.tryGetGaussianViewCamera(gaussian_view_camera, camera_error)) {
        snapshot.has_camera_pose = true;
        snapshot.camera_pose = ExportRemoteCameraPose(gaussian_view_camera);
    }

    snapshot.current_phase = viewer.getCurrentPhase();
    snapshot.available_phases = viewer.getAvailablePhases();
    snapshot.total_asset_count = viewer.getManifestAssetCount();

    if (rendering_system != nullptr) {
        if (const SwapManager::Stats* swap = rendering_system->getSwapStats()) {
            snapshot.required_gpu_count = swap->required_gpu_count;
            snapshot.warm_cpu_count = swap->warm_cpu_count;
            snapshot.pending_disk_loads = swap->pending_disk_loads;
            snapshot.pending_gpu_uploads = swap->pending_gpu_uploads;
            snapshot.pending_gpu_evictions = swap->pending_gpu_evictions;
            snapshot.cpu_resident_bytes = swap->cpu_resident_bytes;
            snapshot.gpu_resident_bytes = swap->gpu_resident_bytes;
            snapshot.skipped_instances_last_frame = swap->skipped_instances_last_frame;
            snapshot.swap_hits = swap->swap_hits;
            snapshot.swap_misses = swap->swap_misses;
        }
    }
    return snapshot;
}

void updateServerHealth(RemoteStreamServer* server, const ExtendedGaussianViewer& viewer)
{
    if (!server) {
        return;
    }
    server->setRendererHealthSnapshot(makeRendererHealthSnapshot(viewer));
}

void pumpRemoteControl(RemoteStreamServer* server, ExtendedGaussianViewer& viewer, uint64_t* visible_control_sequence)
{
    if (!server) {
        return;
    }

    ControlMessage message;
    uint64_t sequence = 0;
    std::chrono::steady_clock::time_point received_at = std::chrono::steady_clock::time_point::min();
    if (!server->consumePendingControlMessage(message, sequence, received_at)) {
        return;
    }

    bool applied = false;
    std::string apply_error;
    switch (message.type) {
    case ControlMessageType::SetCameraPose: {
        sibr::InputCamera camera;
        if (viewer.tryGetGaussianViewCamera(camera, apply_error) &&
            TryBuildInputCamera(message.camera_pose, camera, apply_error)) {
            applied = viewer.applyGaussianViewCamera(camera, apply_error);
        }
        break;
    }
    case ControlMessageType::SetPhase: {
        viewer.setCurrentPhase(message.phase);
        applied = true;
        break;
    }
    }

    const auto apply_end = std::chrono::steady_clock::now();
    double receive_to_apply_ms = 0.0;
    if (received_at != std::chrono::steady_clock::time_point::min()) {
        receive_to_apply_ms = std::chrono::duration<double, std::milli>(apply_end - received_at).count();
    }

    if (!applied) {
        SIBR_WRG << "Failed to apply remote control message seq=" << sequence << ": " << apply_error << std::endl;
    } else if (visible_control_sequence != nullptr) {
        *visible_control_sequence = sequence;
    }
    server->recordControlMessageApplied(sequence, applied, receive_to_apply_ms);
}
#endif

int runInteractive(
    ExtendedGaussianViewer& viewer,
    Window& window
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
    , RemoteStreamServer* server
#endif
)
{
    uint64_t visible_control_sequence = 0;
    while (window.isOpened() && !shutdownRequested())
    {
        if (window.GLFW() != nullptr) {
            Input::poll();
        }
        window.makeContextCurrent();

        if (Input::global().key().isPressed(Key::Escape))
        {
            window.close();
        }

        viewer.onUpdate(Input::global());
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
        pumpRemoteControl(server, viewer, &visible_control_sequence);
#endif
        viewer.onRender(window);
#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
        if (server) {
            if (const RenderTargetRGB* stream_target = viewer.getGaussianViewRenderTarget()) {
                server->submitRenderedFrame(*stream_target, viewer.getFrameIndex(), visible_control_sequence);
            }
            updateServerHealth(server, viewer);
        }
#endif
        viewer.onSwapBuffer(window);
    }

#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
    if (server) {
        window.makeContextCurrent();
        server->releaseRenderThreadResources();
    }
#endif

    return EXIT_SUCCESS;
}

int runHeadless(ExtendedGaussianViewer& viewer, Window& window, const ExtendedGaussianViewerAppArgs& args)
{
    const std::string manifest_path = args.manifest.get();
    const std::string dataset_path = getCommandLineArgs().get<std::string>("path", "");
    if (args.snapshot.get().empty()) {
        SIBR_WRG << "Headless mode requires --snapshot <png_path>." << std::endl;
        return EXIT_FAILURE;
    }
    if (args.render_width.get() <= 0 || args.render_height.get() <= 0) {
        SIBR_WRG << "Headless mode requires positive --render-width and --render-height values." << std::endl;
        return EXIT_FAILURE;
    }

    if (!manifest_path.empty()) {
        const RenderingSystem* rendering_system = viewer.getRenderingSystem();
        if (!rendering_system || !rendering_system->hasManifest()) {
            SIBR_WRG << "Failed to load manifest for headless render: " << manifest_path << std::endl;
            return EXIT_FAILURE;
        }
    }
    else if (!dataset_path.empty() && !viewer.loadModelDirectoryAsInstance(dataset_path)) {
        SIBR_WRG << "Failed to load model directory for headless render: " << dataset_path << std::endl;
        return EXIT_FAILURE;
    }

    window.makeContextCurrent();

    const std::string snapshot_path = args.snapshot.get();
    const bool wait_for_streaming_idle = args.wait_for_streaming_idle.get();
    const int max_frames = std::max(1, args.max_headless_frames.get());
    int consecutive_idle_frames = 0;
    bool streaming_idle = false;

    for (int frame_index = 0; frame_index < max_frames; ++frame_index)
    {
        if (window.GLFW() != nullptr) {
            Input::poll();
        }
        viewer.onUpdate(Input::global());
        viewer.onRender(window);
        streaming_idle = viewer.isStreamingIdle();

        if (!wait_for_streaming_idle) {
            break;
        }

        if (streaming_idle) {
            ++consecutive_idle_frames;
            if (consecutive_idle_frames >= 2) {
                break;
            }
        }
        else {
            consecutive_idle_frames = 0;
        }
    }

    if (!viewer.captureGaussianViewSnapshot(snapshot_path)) {
        SIBR_WRG << "Failed to capture headless snapshot to " << snapshot_path << std::endl;
        return EXIT_FAILURE;
    }

    if (wait_for_streaming_idle && consecutive_idle_frames < 2) {
        SIBR_WRG << "Headless render reached max frame budget before streaming became idle." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

} // namespace

int main(int ac, char** av)
{
#ifdef _WIN32
    _putenv_s("CUDA_MODULE_LOADING", "LAZY");
#else
    setenv("CUDA_MODULE_LOADING", "LAZY", 1);
#endif

    std::signal(SIGINT, handleProcessSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, handleProcessSignal);
#endif

    CommandLineArgs::parseMainArgs(ac, av);
    ExtendedGaussianViewerAppArgs myArgs;
    myArgs.displayHelpIfRequired();
    if (myArgs.showHelp.get()) {
        return EXIT_SUCCESS;
    }

#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
    const ServerOptions server_options = ParseServerOptions(getCommandLineArgs());
    const bool server_enabled = server_options.enabled;
#else
    const bool server_enabled = false;
    const bool requested_server = getCommandLineArgs().get<bool>("server", false);
    if (requested_server) {
        SIBR_WRG << "This build does not include remote stream server support. Reconfigure with -DSIBR_BUILD_REMOTE_STREAM=ON." << std::endl;
        return EXIT_FAILURE;
    }
#endif

    if (myArgs.headless.get() && server_enabled) {
        myArgs.offscreen = true;
        myArgs.no_gui = true;
        myArgs.vsync = 0;
        myArgs.win_width = myArgs.render_width.get();
        myArgs.win_height = myArgs.render_height.get();
    }

    if (myArgs.headless.get() && !server_enabled) {
        if (myArgs.snapshot.get().empty()) {
            SIBR_WRG << "Headless mode requires --snapshot <png file>." << std::endl;
            return EXIT_FAILURE;
        }
        if (myArgs.render_width.get() <= 0 || myArgs.render_height.get() <= 0) {
            SIBR_WRG << "Headless mode requires positive --render-width and --render-height values." << std::endl;
            return EXIT_FAILURE;
        }
        myArgs.offscreen = true;
        myArgs.no_gui = true;
        myArgs.vsync = 0;
        myArgs.win_width = myArgs.render_width.get();
        myArgs.win_height = myArgs.render_height.get();
    }

    std::unique_ptr<Window> window;
    if (myArgs.headless.get() || myArgs.offscreen.get()) {
        window = std::make_unique<Window>("Extended Gaussian Viewer", myArgs);
    }
    else {
        window = std::make_unique<Window>("Extended Gaussian Viewer", Vector2i(50, 50), myArgs);
    }

    ExtendedGaussianViewer viewer(*window, false);

    const std::string dataset_path = getCommandLineArgs().get<std::string>("path", "");
    const std::string manifest_path = myArgs.manifest.get();
    if (server_enabled && manifest_path.empty() && !dataset_path.empty()) {
        if (!viewer.loadModelDirectoryAsInstance(dataset_path)) {
            SIBR_WRG << "Failed to load model directory for remote stream server: " << dataset_path << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (myArgs.headless.get() && !server_enabled) {
        return runHeadless(viewer, *window, myArgs);
    }

#if defined(SIBR_EXTENDED_GAUSSIAN_REMOTE_STREAM_BUILD)
    std::unique_ptr<RemoteStreamServer> server;
    if (server_enabled) {
        server = std::make_unique<RemoteStreamServer>(server_options);
        updateServerHealth(server.get(), viewer);
        std::string server_error;
        if (!server->start(server_error)) {
            SIBR_WRG << "Failed to start RemoteStreamServer: " << server_error << std::endl;
            return EXIT_FAILURE;
        }
    }

    const int result = runInteractive(viewer, *window, server.get());
    if (server) {
        const auto stop_start = std::chrono::steady_clock::now();
        server->stop();
        const double stop_elapsed_sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - stop_start).count();
        SIBR_LOG << "RemoteStreamServer stop elapsed: " << stop_elapsed_sec << " sec" << std::endl;
    }
    return result;
#else
    return runInteractive(viewer, *window);
#endif
}
