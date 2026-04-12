#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>

#include <core/graphics/Window.hpp>
#include <core/system/CommandLineArgs.hpp>
#include "projects/extended_gaussian/renderer/ExtendedGaussianViewer.hpp"
#include "projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.hpp"

using namespace sibr;

namespace {

struct ExtendedGaussianViewerAppArgs : virtual BasicIBRAppArgs {
	Arg<std::string> manifest = { "manifest", "", "path to a manifest json file" };
	Arg<bool> headless = { "headless", "run a finite offscreen render loop and exit" };
	Arg<int> render_width = { "render-width", 1280, "offscreen render width for headless mode" };
	Arg<int> render_height = { "render-height", 720, "offscreen render height for headless mode" };
	Arg<std::string> snapshot = { "snapshot", "", "png file path for headless snapshot output" };
	Arg<bool> wait_for_streaming_idle = { "wait-for-streaming-idle", "wait until manifest streaming queues drain before capturing" };
	Arg<int> max_headless_frames = { "max-headless-frames", 600, "maximum number of frames to render in headless mode" };
};

int runInteractive(ExtendedGaussianViewer& viewer, Window& window)
{
	while (window.isOpened())
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
		viewer.onRender(window);
		viewer.onSwapBuffer(window);
	}

	return EXIT_SUCCESS;
}

int runHeadless(ExtendedGaussianViewer& viewer, Window& window, const ExtendedGaussianViewerAppArgs& args)
{
	const std::string manifestPath = args.manifest.get();
	const std::string datasetPath = getCommandLineArgs().get<std::string>("path", "");
	if (args.snapshot.get().empty()) {
		SIBR_WRG << "Headless mode requires --snapshot <png_path>." << std::endl;
		return EXIT_FAILURE;
	}
	if (args.render_width.get() <= 0 || args.render_height.get() <= 0) {
		SIBR_WRG << "Headless mode requires positive --render-width and --render-height values." << std::endl;
		return EXIT_FAILURE;
	}

	if (!manifestPath.empty()) {
		const RenderingSystem* renderingSystem = viewer.getRenderingSystem();
		if (!renderingSystem || !renderingSystem->hasManifest()) {
			SIBR_WRG << "Failed to load manifest for headless render: " << manifestPath << std::endl;
			return EXIT_FAILURE;
		}
	}
	else if (!datasetPath.empty() && !viewer.loadModelDirectoryAsInstance(datasetPath)) {
		SIBR_WRG << "Failed to load model directory for headless render: " << datasetPath << std::endl;
		return EXIT_FAILURE;
	}

	window.makeContextCurrent();

	const std::string snapshotPath = args.snapshot.get();
	const bool waitForStreamingIdle = args.wait_for_streaming_idle.get();
	const int maxFrames = std::max(1, args.max_headless_frames.get());
	int consecutiveIdleFrames = 0;
	bool streamingIdle = false;

	for (int frameIndex = 0; frameIndex < maxFrames; ++frameIndex)
	{
		if (window.GLFW() != nullptr) {
			Input::poll();
		}
		viewer.onUpdate(Input::global());
		viewer.onRender(window);
		streamingIdle = viewer.isStreamingIdle();

		if (!waitForStreamingIdle) {
			break;
		}

		if (streamingIdle) {
			++consecutiveIdleFrames;
			if (consecutiveIdleFrames >= 2) {
				break;
			}
		}
		else {
			consecutiveIdleFrames = 0;
		}
	}

	if (!viewer.captureGaussianViewSnapshot(snapshotPath)) {
		SIBR_WRG << "Failed to capture headless snapshot to " << snapshotPath << std::endl;
		return EXIT_FAILURE;
	}

	if (waitForStreamingIdle && consecutiveIdleFrames < 2) {
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

	CommandLineArgs::parseMainArgs(ac, av);
	ExtendedGaussianViewerAppArgs myArgs;
	myArgs.displayHelpIfRequired();
	if (myArgs.showHelp.get()) {
		return EXIT_SUCCESS;
	}

	if (myArgs.headless.get()) {
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

	if (myArgs.headless.get()) {
		return runHeadless(viewer, *window, myArgs);
	}

	return runInteractive(viewer, *window);
}
