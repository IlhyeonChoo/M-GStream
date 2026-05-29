
#include "MGStreamViewer.hpp"
#include <projects/M_GStream/renderer/resource/GaussianLoader.hpp>
#include <projects/M_GStream/renderer/subsystem/rendering_system/RenderingSystem.hpp>

#include <core/system/CommandLineArgs.hpp>
#include "picojson/picojson.hpp"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <cmath>
#include <cstdio>
#include <cuda_runtime.h>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <imgui_internal.h>
#include <optional>
#include <sstream>
#include <vector>

namespace {
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kMinFocusVectorNorm = 1e-5f;

	struct ParsedCameraPose {
		sibr::Vector3f position = sibr::Vector3f::Zero();
		sibr::Vector3f forward = sibr::Vector3f(0.0f, 0.0f, -1.0f);
		sibr::Vector3f up = sibr::Vector3f(0.0f, 1.0f, 0.0f);
		bool has_orientation = false;
		bool has_fovy = false;
		float fovy = 0.0f;
	};

	struct CameraFocusPose {
		sibr::Vector3f eye = sibr::Vector3f::Zero();
		sibr::Vector3f target = sibr::Vector3f(0.0f, 0.0f, -1.0f);
		sibr::Vector3f up = sibr::Vector3f(0.0f, 1.0f, 0.0f);
		bool has_fovy = false;
		float fovy = 0.0f;
	};

	bool isFiniteFloat(float value)
	{
		return std::isfinite(value) != 0;
	}

	bool isFiniteVector(const sibr::Vector3f& value)
	{
		return isFiniteFloat(value.x()) && isFiniteFloat(value.y()) && isFiniteFloat(value.z());
	}

	bool isValidDirection(const sibr::Vector3f& value)
	{
		return isFiniteVector(value) && value.norm() > kMinFocusVectorNorm;
	}

	std::optional<sibr::Vector3f> normalizedVector(const sibr::Vector3f& value)
	{
		if (!isValidDirection(value)) {
			return std::nullopt;
		}
		return value.normalized();
	}

	std::optional<float> parseJsonFloat(const picojson::value& value)
	{
		if (!value.is<double>()) {
			return std::nullopt;
		}

		const float parsed = static_cast<float>(value.get<double>());
		if (!isFiniteFloat(parsed)) {
			return std::nullopt;
		}
		return parsed;
	}

	std::optional<float> parseJsonFloatField(const picojson::object& object, const char* key)
	{
		const auto it = object.find(key);
		if (it == object.end()) {
			return std::nullopt;
		}
		return parseJsonFloat(it->second);
	}

	std::optional<sibr::Vector3f> parseJsonVector3(const picojson::value& value)
	{
		if (!value.is<picojson::array>()) {
			return std::nullopt;
		}

		const auto& values = value.get<picojson::array>();
		if (values.size() != 3) {
			return std::nullopt;
		}

		const auto x = parseJsonFloat(values[0]);
		const auto y = parseJsonFloat(values[1]);
		const auto z = parseJsonFloat(values[2]);
		if (!x || !y || !z) {
			return std::nullopt;
		}

		const sibr::Vector3f parsed(*x, *y, *z);
		if (!isFiniteVector(parsed)) {
			return std::nullopt;
		}
		return parsed;
	}

	std::optional<sibr::Vector3f> parseJsonVector3Field(
		const picojson::object& object,
		std::initializer_list<const char*> keys)
	{
		for (const char* key : keys) {
			const auto it = object.find(key);
			if (it == object.end()) {
				continue;
			}

			const auto parsed = parseJsonVector3(it->second);
			if (parsed) {
				return parsed;
			}
		}
		return std::nullopt;
	}

	std::optional<sibr::Matrix3f> parseJsonMatrix3(const picojson::value& value)
	{
		if (!value.is<picojson::array>()) {
			return std::nullopt;
		}

		const auto& rows = value.get<picojson::array>();
		sibr::Matrix3f matrix;
		if (rows.size() == 3 && rows[0].is<picojson::array>()) {
			for (size_t row = 0; row < 3; ++row) {
				if (!rows[row].is<picojson::array>()) {
					return std::nullopt;
				}
				const auto& values = rows[row].get<picojson::array>();
				if (values.size() != 3) {
					return std::nullopt;
				}
				for (size_t col = 0; col < 3; ++col) {
					const auto parsed = parseJsonFloat(values[col]);
					if (!parsed) {
						return std::nullopt;
					}
					matrix(static_cast<int>(row), static_cast<int>(col)) = *parsed;
				}
			}
			return matrix;
		}

		if (rows.size() == 9) {
			for (size_t index = 0; index < rows.size(); ++index) {
				const auto parsed = parseJsonFloat(rows[index]);
				if (!parsed) {
					return std::nullopt;
				}
				matrix(static_cast<int>(index / 3), static_cast<int>(index % 3)) = *parsed;
			}
			return matrix;
		}

		return std::nullopt;
	}

	std::optional<sibr::Matrix4f> parseJsonMatrix4(const picojson::value& value)
	{
		if (!value.is<picojson::array>()) {
			return std::nullopt;
		}

		const auto& rows = value.get<picojson::array>();
		sibr::Matrix4f matrix;
		if (rows.size() == 4 && rows[0].is<picojson::array>()) {
			for (size_t row = 0; row < 4; ++row) {
				if (!rows[row].is<picojson::array>()) {
					return std::nullopt;
				}
				const auto& values = rows[row].get<picojson::array>();
				if (values.size() != 4) {
					return std::nullopt;
				}
				for (size_t col = 0; col < 4; ++col) {
					const auto parsed = parseJsonFloat(values[col]);
					if (!parsed) {
						return std::nullopt;
					}
					matrix(static_cast<int>(row), static_cast<int>(col)) = *parsed;
				}
			}
			return matrix;
		}

		if (rows.size() == 16) {
			for (size_t index = 0; index < rows.size(); ++index) {
				const auto parsed = parseJsonFloat(rows[index]);
				if (!parsed) {
					return std::nullopt;
				}
				matrix(static_cast<int>(index / 4), static_cast<int>(index % 4)) = *parsed;
			}
			return matrix;
		}

		return std::nullopt;
	}

	std::optional<sibr::Matrix3f> parseJsonMatrix3Field(
		const picojson::object& object,
		std::initializer_list<const char*> keys)
	{
		for (const char* key : keys) {
			const auto it = object.find(key);
			if (it == object.end()) {
				continue;
			}

			const auto parsed = parseJsonMatrix3(it->second);
			if (parsed) {
				return parsed;
			}
		}
		return std::nullopt;
	}

	std::optional<sibr::Matrix4f> parseJsonMatrix4Field(
		const picojson::object& object,
		std::initializer_list<const char*> keys)
	{
		for (const char* key : keys) {
			const auto it = object.find(key);
			if (it == object.end()) {
				continue;
			}

			const auto parsed = parseJsonMatrix4(it->second);
			if (parsed) {
				return parsed;
			}
		}
		return std::nullopt;
	}

	void applyCameraToWorldOrientation(const sibr::Matrix3f& rotation, ParsedCameraPose& pose)
	{
		// SIBR cameras look along local -Z. Graphdeco-style camera matrices are treated as camera-to-world.
		const sibr::Vector3f forward(-rotation(0, 2), -rotation(1, 2), -rotation(2, 2));
		const sibr::Vector3f up(rotation(0, 1), rotation(1, 1), rotation(2, 1));
		const auto normalizedForward = normalizedVector(forward);
		const auto normalizedUp = normalizedVector(up);
		if (!normalizedForward || !normalizedUp) {
			return;
		}

		pose.forward = *normalizedForward;
		pose.up = *normalizedUp;
		pose.has_orientation = true;
	}

	std::optional<float> parseFovy(const picojson::object& object)
	{
		for (const char* key : { "fovy", "fov_y", "camera_angle_y", "FoVy" }) {
			const auto value = parseJsonFloatField(object, key);
			if (!value) {
				continue;
			}

			if (*value > 0.0f && *value < kPi) {
				return *value;
			}
			if (*value > 0.0f && *value < 180.0f) {
				return *value * kPi / 180.0f;
			}
		}

		const auto fy = parseJsonFloatField(object, "fy");
		const auto height = parseJsonFloatField(object, "height");
		if (fy && height && *fy > 0.0f && *height > 0.0f) {
			return 2.0f * std::atan(0.5f * *height / *fy);
		}

		return std::nullopt;
	}

	bool parseCameraPoseObject(const picojson::object& object, ParsedCameraPose& pose)
	{
		bool hasPosition = false;

		if (const auto transform = parseJsonMatrix4Field(object, { "transform_matrix", "camera_to_world", "c2w" })) {
			pose.position = sibr::Vector3f((*transform)(0, 3), (*transform)(1, 3), (*transform)(2, 3));
			hasPosition = isFiniteVector(pose.position);
			const sibr::Matrix3f rotation = transform->block<3, 3>(0, 0);
			applyCameraToWorldOrientation(rotation, pose);
		}

		if (const auto position = parseJsonVector3Field(object, { "position", "camera_position", "camera_center", "eye" })) {
			pose.position = *position;
			hasPosition = true;
		}

		if (const auto rotation = parseJsonMatrix3Field(object, { "rotation", "orientation" })) {
			applyCameraToWorldOrientation(*rotation, pose);
		}

		const auto forward = parseJsonVector3Field(object, { "forward", "direction", "view_direction" });
		const auto up = parseJsonVector3Field(object, { "up" });
		if (forward && up) {
			const auto normalizedForward = normalizedVector(*forward);
			const auto normalizedUp = normalizedVector(*up);
			if (normalizedForward && normalizedUp) {
				pose.forward = *normalizedForward;
				pose.up = *normalizedUp;
				pose.has_orientation = true;
			}
		}

		if (hasPosition) {
			const auto target = parseJsonVector3Field(object, { "target", "look_at", "lookat" });
			if (target) {
				const auto targetForward = normalizedVector(*target - pose.position);
				if (targetForward) {
					pose.forward = *targetForward;
					pose.has_orientation = true;
				}
			}
		}

		if (const auto fovy = parseFovy(object)) {
			pose.has_fovy = true;
			pose.fovy = *fovy;
		}

		return hasPosition && isFiniteVector(pose.position);
	}

	void collectCameraPoseSamples(const picojson::array& cameras, std::vector<ParsedCameraPose>& samples)
	{
		for (const auto& cameraValue : cameras) {
			if (!cameraValue.is<picojson::object>()) {
				continue;
			}

			ParsedCameraPose sample;
			if (parseCameraPoseObject(cameraValue.get<picojson::object>(), sample)) {
				samples.emplace_back(sample);
			}
		}
	}

	bool loadCameraPoseSamples(const boost::filesystem::path& modelDir, std::vector<ParsedCameraPose>& samples)
	{
		const boost::filesystem::path camerasPath = modelDir / "cameras.json";
		if (!boost::filesystem::exists(camerasPath) || !boost::filesystem::is_regular_file(camerasPath)) {
			return false;
		}

		std::ifstream camerasFile(camerasPath.string());
		if (!camerasFile.good()) {
			SIBR_WRG << "Unable to open cameras.json at " << camerasPath.string() << std::endl;
			return false;
		}

		picojson::value rootValue;
		const std::string parseError = picojson::parse(rootValue, camerasFile);
		if (!parseError.empty()) {
			SIBR_WRG << "Invalid cameras.json at " << camerasPath.string() << ": " << parseError << std::endl;
			return false;
		}

		if (rootValue.is<picojson::array>()) {
			collectCameraPoseSamples(rootValue.get<picojson::array>(), samples);
		} else if (rootValue.is<picojson::object>()) {
			const auto& root = rootValue.get<picojson::object>();
			bool parsedArray = false;
			for (const char* key : { "cameras", "frames" }) {
				const auto it = root.find(key);
				if (it != root.end() && it->second.is<picojson::array>()) {
					collectCameraPoseSamples(it->second.get<picojson::array>(), samples);
					parsedArray = true;
				}
			}

			if (!parsedArray) {
				ParsedCameraPose sample;
				if (parseCameraPoseObject(root, sample)) {
					samples.emplace_back(sample);
				}
			}
		}

		if (samples.empty()) {
			SIBR_WRG << "cameras.json did not contain valid camera poses at " << camerasPath.string() << std::endl;
			return false;
		}
		return true;
	}

	float medianValue(std::vector<float> values)
	{
		if (values.empty()) {
			return 0.0f;
		}

		std::sort(values.begin(), values.end());
		const size_t mid = values.size() / 2;
		if ((values.size() % 2) != 0) {
			return values[mid];
		}
		return 0.5f * (values[mid - 1] + values[mid]);
	}

	sibr::Vector3f medianVector(const std::vector<sibr::Vector3f>& values)
	{
		std::vector<float> xs;
		std::vector<float> ys;
		std::vector<float> zs;
		xs.reserve(values.size());
		ys.reserve(values.size());
		zs.reserve(values.size());
		for (const auto& value : values) {
			xs.emplace_back(value.x());
			ys.emplace_back(value.y());
			zs.emplace_back(value.z());
		}

		return sibr::Vector3f(medianValue(xs), medianValue(ys), medianValue(zs));
	}

	std::optional<sibr::Vector3f> computeGaussianPositionCenter(const sibr::GaussianField& field)
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		size_t count = 0;
		for (const auto& position : field.pos) {
			if (!isFiniteVector(position)) {
				continue;
			}
			x += static_cast<double>(position.x());
			y += static_cast<double>(position.y());
			z += static_cast<double>(position.z());
			++count;
		}

		if (count == 0) {
			return std::nullopt;
		}

		return sibr::Vector3f(
			static_cast<float>(x / static_cast<double>(count)),
			static_cast<float>(y / static_cast<double>(count)),
			static_cast<float>(z / static_cast<double>(count)));
	}

	sibr::Vector3f boundsCenter(const sibr::Vector3f& minBounds, const sibr::Vector3f& maxBounds)
	{
		return 0.5f * (minBounds + maxBounds);
	}

	bool buildCamerasJsonFocusPose(const sibr::GaussianField& field, CameraFocusPose& focusPose)
	{
		std::vector<ParsedCameraPose> samples;
		if (!loadCameraPoseSamples(boost::filesystem::path(field.path), samples)) {
			return false;
		}

		std::vector<sibr::Vector3f> positions;
		std::vector<float> fovys;
		positions.reserve(samples.size());
		for (const auto& sample : samples) {
			positions.emplace_back(sample.position);
			if (sample.has_fovy && sample.fovy > 0.0f && sample.fovy < kPi) {
				fovys.emplace_back(sample.fovy);
			}
		}

		focusPose.eye = medianVector(positions);

		const sibr::Vector3f fallbackTarget = computeGaussianPositionCenter(field).value_or(boundsCenter(field.min_edges, field.max_edges));
		const sibr::Vector3f fallbackForward = normalizedVector(fallbackTarget - focusPose.eye)
			.value_or(sibr::Vector3f(0.0f, 0.0f, -1.0f));

		sibr::Vector3f forwardSum = sibr::Vector3f::Zero();
		sibr::Vector3f upSum = sibr::Vector3f::Zero();
		size_t orientationCount = 0;
		for (const auto& sample : samples) {
			if (!sample.has_orientation) {
				continue;
			}

			auto forward = sample.forward;
			auto up = sample.up;
			if (!isValidDirection(forward) || !isValidDirection(up)) {
				continue;
			}

			forward.normalize();
			up.normalize();
			if (forward.dot(fallbackForward) < 0.0f) {
				forward = -forward;
			}
			if (up.dot(sibr::Vector3f(0.0f, 1.0f, 0.0f)) < 0.0f) {
				up = -up;
			}

			forwardSum += forward;
			upSum += up;
			++orientationCount;
		}

		sibr::Vector3f forward = fallbackForward;
		sibr::Vector3f up(0.0f, 1.0f, 0.0f);
		if (orientationCount > 0 && isValidDirection(forwardSum)) {
			forward = forwardSum.normalized();
		}
		if (orientationCount > 0 && isValidDirection(upSum) && std::fabs(upSum.normalized().dot(forward)) < 0.98f) {
			up = upSum.normalized();
		}

		focusPose.target = focusPose.eye + forward;
		focusPose.up = up;
		if (!fovys.empty()) {
			focusPose.has_fovy = true;
			focusPose.fovy = medianValue(fovys);
		}

		return true;
	}
} // namespace

namespace sibr {
	MGStreamViewer::MGStreamViewer(Window& window, bool resize)
		: _window(window), _fpsCounter(false)
	{
		_enableGUI = window.isGUIEnabled();

		if (resize) {
			window.size(
				Window::desktopSize().x() - 200,
				Window::desktopSize().y() - 200);
			window.position(100, 100);
		}

		/// \todo TODO: support launch arg for stereo mode.
		renderingMode(IRenderingMode::Ptr(new MonoRdrMode()));

		//Default view resolution.
		int w = int(window.size().x() * 0.5f);
		int h = int(window.size().y() * 0.5f);
		setDefaultViewResolution(Vector2i(w, h));

		if (_enableGUI)
		{
			ImGui::GetStyle().WindowBorderSize = 0.0;
		}

		_scene = std::make_unique<GaussianScene>();
		_resourceManager = std::make_unique<ResourceManager>();
		_subsystem[RENDERING_SYSTEM] = std::make_unique<RenderingSystem>();
		_subsystem[RENDERING_SYSTEM]->onSystemAdded(*this);

		const std::string manifestPath = getCommandLineArgs().get<std::string>("manifest", "");
		if (!manifestPath.empty()) {
			std::string manifestError;
			loadManifestFile(manifestPath, manifestError);
		}
	}

	void MGStreamViewer::onUpdate(Input& input)
	{
		MultiViewBase::onUpdate(input);
		_appTimeSec += deltaTime();

		if (input.key().isActivated(Key::LeftControl) && input.key().isActivated(Key::LeftAlt) && input.key().isReleased(Key::G)) {
			toggleGUI();
		}
	}

	void MGStreamViewer::onRender(Window& win)
	{
		win.viewport().bind();
		glClearColor(37.f / 255.f, 37.f / 255.f, 38.f / 255.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(1.f, 1.f, 1.f, 1.f);

		if (_enableGUI) {
			onGui(win);
		}

		RenderingSystem* renderingSystem = getRenderingSystem();
		if (renderingSystem) {
			ViewerContext context;
			const auto viewIt = _ibrSubViews.find("Gaussian View");
			if (viewIt != _ibrSubViews.end()) {
				context.camera_pos = viewIt->second.cam.position();
				context.camera_forward = viewIt->second.cam.dir();
				context.camera_up = viewIt->second.cam.up();
			}
			context.current_phase = _currentPhase;
			context.app_time_sec = _appTimeSec;
			context.dt_sec = deltaTime();
			context.frame_index = _frameIndex;
			renderingSystem->tickStreaming(context);
		}

		MultiViewBase::onRender(win);
		++_frameIndex;

		_fpsCounter.update(_enableGUI && _showGUI);
	}

	void MGStreamViewer::onGui(Window& win)
	{
		if (!_enableGUI) {
			return;
		}

		MultiViewBase::onGui(win);

		// Menu
		if (_showGUI && ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Menu"))
			{
				ImGui::MenuItem("Pause", "", &_onPause);
				if (ImGui::BeginMenu("Display")) {
					const bool currentScreenState = win.isFullscreen();
					if (ImGui::MenuItem("Fullscreen", "", currentScreenState)) {
						win.setFullscreen(!currentScreenState);
					}

					const bool currentSyncState = win.isVsynced();
					if (ImGui::MenuItem("V-sync", "", currentSyncState)) {
						win.setVsynced(!currentSyncState);
					}

					const bool isHiDPI = ImGui::GetIO().FontGlobalScale > 1.0f;
					if (ImGui::MenuItem("HiDPI", "", isHiDPI)) {
						if (isHiDPI) {
							ImGui::GetStyle().ScaleAllSizes(1.0f / win.scaling());
							ImGui::GetIO().FontGlobalScale = 1.0f;
						}
						else {
							ImGui::GetStyle().ScaleAllSizes(win.scaling());
							ImGui::GetIO().FontGlobalScale = win.scaling();
						}
					}

					if (ImGui::MenuItem("Hide GUI (!)", "Ctrl+Alt+G")) {
						toggleGUI();
					}
					ImGui::EndMenu();
				}


				if (ImGui::MenuItem("Mosaic layout")) {
					mosaicLayout(win.viewport());
				}

				if (ImGui::MenuItem("Row layout")) {
					Vector2f itemSize = win.size().cast<float>();
					itemSize[0] = std::round(float(itemSize[0]) / float(_subViews.size() + _ibrSubViews.size()));
					const float verticalShift = ImGui::GetTitleBarHeight();
					float vid = 0.0f;
					for (auto& view : _ibrSubViews) {
						// Compute position on grid.
						view.second.viewport = Viewport(vid * itemSize[0], verticalShift, (vid + 1.0f) * itemSize[0] - 1.0f, verticalShift + itemSize[1] - 1.0f);
						view.second.shouldUpdateLayout = true;
						++vid;
					}
					for (auto& view : _subViews) {
						// Compute position on grid.
						view.second.viewport = Viewport(vid * itemSize[0], verticalShift, (vid + 1.0f) * itemSize[0] - 1.0f, verticalShift + itemSize[1] - 1.0f);
						view.second.shouldUpdateLayout = true;
						++vid;
					}
				}


				if (ImGui::MenuItem("Quit", "Escape")) { win.close(); }
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Views"))
			{
				for (auto& subview : _subViews) {
					if (ImGui::MenuItem(subview.first.c_str(), "", subview.second.view->active())) {
						subview.second.view->active(!subview.second.view->active());
					}
				}
				for (auto& subview : _ibrSubViews) {
					if (ImGui::MenuItem(subview.first.c_str(), "", subview.second.view->active())) {
						subview.second.view->active(!subview.second.view->active());
					}
				}
				if (ImGui::MenuItem("Metrics", "", _fpsCounter.active())) {
					_fpsCounter.toggleVisibility();
				}
				if (ImGui::BeginMenu("Front when focus"))
				{
					for (auto& subview : _subViews) {
						const bool isLockedInBackground = subview.second.flags & ImGuiWindowFlags_NoBringToFrontOnFocus;
						if (ImGui::MenuItem(subview.first.c_str(), "", !isLockedInBackground)) {
							if (isLockedInBackground) {
								subview.second.flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
							}
							else {
								subview.second.flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
							}
						}
					}
					for (auto& subview : _ibrSubViews) {
						const bool isLockedInBackground = subview.second.flags & ImGuiWindowFlags_NoBringToFrontOnFocus;
						if (ImGui::MenuItem(subview.first.c_str(), "", !isLockedInBackground)) {
							if (isLockedInBackground) {
								subview.second.flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
							}
							else {
								subview.second.flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
							}
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Reset Settings to Default", "")) {
					_window.resetSettingsToDefault();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Capture"))
			{

				if (ImGui::MenuItem("Set export directory...")) {
					std::string selectedDirectory;
					if (showFilePicker(selectedDirectory, FilePickerMode::Directory)) {
						if (!selectedDirectory.empty()) {
							_exportPath = selectedDirectory;
						}
					}
				}

				for (auto& subview : _subViews) {
					if (ImGui::MenuItem(subview.first.c_str())) {
						captureView(subview.second, _exportPath);
					}
				}
				for (auto& subview : _ibrSubViews) {
					if (ImGui::MenuItem(subview.first.c_str())) {
						captureView(subview.second, _exportPath);
					}
				}

				if (ImGui::MenuItem("Export Video")) {
					std::string saveFile;
					if (showFilePicker(saveFile, FilePickerMode::Save)) {
						const std::string outputVideo = saveFile + ".mp4";
						if (!_videoFrames.empty()) {
							SIBR_LOG << "Exporting video to : " << outputVideo << " ..." << std::flush;
							FFVideoEncoder vdoEncoder(outputVideo, 30, Vector2i(_videoFrames[0].cols, _videoFrames[0].rows));
							for (int i = 0; i < _videoFrames.size(); i++) {
								vdoEncoder << _videoFrames[i];
							}
							_videoFrames.clear();
							std::cout << " Done." << std::endl;

						}
						else {
							SIBR_WRG << "No frames to export!! Check save frames in camera options for the view you want to render and play the path and re-export!" << std::endl;
						}
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Panels"))
			{
				ImGui::MenuItem("Scene Outliner", nullptr, &_showScenePanel);
				ImGui::MenuItem("Resource Browser", nullptr, &_showResourceBrowser);
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
		if (_showScenePanel)
		{
			onShowScenePanel(win);
		}
		if (_showResourceBrowser)
		{
			onShowResourceBrowser(win);
		}
	}

	void MGStreamViewer::onSwapBuffer(sibr::Window& win)
	{
		win.swapBuffer();
	}

	Vector2i MGStreamViewer::getWindowSize() const
	{
		return _window.size();
	}

	const GaussianScene* MGStreamViewer::getScene() const {
		return _scene.get();
	}

	GaussianScene* MGStreamViewer::getScene() {
		return _scene.get();
	}

	ResourceManager* MGStreamViewer::getResourceManager() {
		return _resourceManager.get();
	}

	const ResourceManager* MGStreamViewer::getResourceManager() const {
		return _resourceManager.get();
	}

	RenderingSystem* MGStreamViewer::getRenderingSystem()
	{
		return static_cast<RenderingSystem*>(_subsystem[RENDERING_SYSTEM].get());
	}

	const RenderingSystem* MGStreamViewer::getRenderingSystem() const
	{
		return static_cast<const RenderingSystem*>(_subsystem[RENDERING_SYSTEM].get());
	}

	bool MGStreamViewer::loadModelDirectoryAsInstance(const std::string& modelPath)
	{
		if (modelPath.empty() || !_scene || !_resourceManager) {
			return false;
		}

		auto field = GaussianLoader::load(modelPath);
		if (!field) {
			_lastLoadError = "Failed to load model directory: " + modelPath;
			_loadState = contentLoadStateLabel(ContentLoadState::Error);
			return false;
		}

		const std::string assetId = field->name;
		if (assetId.empty()) {
			_lastLoadError = "Loaded model directory does not have a valid asset id: " + modelPath;
			_loadState = contentLoadStateLabel(ContentLoadState::Error);
			return false;
		}

		const std::string resolvedPath = field->path;
		Vector3f minBounds = field->min_edges;
		Vector3f maxBounds = field->max_edges;
		const bool addedField = _resourceManager->addField(std::move(field));
		GaussianField::Ptr focusField;
		if (!addedField) {
			const auto existingField = _resourceManager->getCpuFieldShared(assetId);
			if (!existingField) {
				_lastLoadError = "Failed to register loaded asset: " + assetId;
				_loadState = contentLoadStateLabel(ContentLoadState::Error);
				return false;
			}
			minBounds = existingField->min_edges;
			maxBounds = existingField->max_edges;
			focusField = existingField;
		} else {
			focusField = _resourceManager->getCpuFieldShared(assetId);
		}

		for (const auto& instancePair : _scene->getInstances()) {
			if (instancePair.second && instancePair.second->getAssetId() == assetId) {
				_selectedField = assetId;
				_selectedInstance = instancePair.second.get();
				_loadedContentKind = "model_dir";
				_loadedContentPath = resolvedPath;
				_loadState = contentLoadStateLabel(ContentLoadState::Loaded);
				_lastLoadError.clear();
				if (focusField) {
					focusCameraOnModel(*focusField);
				} else {
					focusCameraOnBounds(minBounds, maxBounds);
				}
				return true;
			}
		}

		std::string instanceName = assetId;
		int suffix = 1;
		while (_scene->getInstance(instanceName) != nullptr) {
			instanceName = assetId + "_" + std::to_string(suffix++);
		}

		GaussianInstance* instance = _scene->createInstance(instanceName, assetId);
		if (!instance) {
			_lastLoadError = "Failed to create scene instance for asset: " + assetId;
			_loadState = contentLoadStateLabel(ContentLoadState::Error);
			return false;
		}

		_selectedField = assetId;
		_selectedInstance = instance;
		if (auto* renderingSystem = getRenderingSystem()) {
			renderingSystem->onInstanceCreated(*instance);
		}

		_loadedContentKind = "model_dir";
		_loadedContentPath = resolvedPath;
		_loadState = contentLoadStateLabel(ContentLoadState::Loaded);
		_lastLoadError.clear();
		if (focusField) {
			focusCameraOnModel(*focusField);
		} else {
			focusCameraOnBounds(minBounds, maxBounds);
		}
		return true;
	}

	bool MGStreamViewer::replaceWithModelDirectory(const std::string& modelPath, std::string& error, uint64_t loadSequence)
	{
		beginContentLoad("model_dir", modelPath, loadSequence);
		if (!resetCurrentContent(error)) {
			finishContentLoad("model_dir", modelPath, loadSequence, false, error);
			return false;
		}
		if (!loadModelDirectoryAsInstance(modelPath)) {
			error = _lastLoadError.empty() ? ("Failed to load model directory: " + modelPath) : _lastLoadError;
			finishContentLoad("model_dir", _loadedContentPath.empty() ? modelPath : _loadedContentPath, loadSequence, false, error);
			return false;
		}
		finishContentLoad("model_dir", _loadedContentPath.empty() ? modelPath : _loadedContentPath, loadSequence, true, std::string());
		error.clear();
		return true;
	}

	bool MGStreamViewer::replaceWithManifestFile(const std::string& path, std::string& error, uint64_t loadSequence)
	{
		beginContentLoad("manifest", path, loadSequence);
		if (!resetCurrentContent(error)) {
			finishContentLoad("manifest", path, loadSequence, false, error);
			return false;
		}
		if (!loadManifestFile(path, error)) {
			finishContentLoad("manifest", _loadedContentPath.empty() ? path : _loadedContentPath, loadSequence, false, error);
			return false;
		}
		finishContentLoad("manifest", _loadedManifestPath.empty() ? path : _loadedManifestPath, loadSequence, true, std::string());
		error.clear();
		return true;
	}

	bool MGStreamViewer::unloadCurrentContent(std::string& error, uint64_t loadSequence)
	{
		if (!resetCurrentContent(error)) {
			_loadedContentKind.clear();
			_loadedContentPath.clear();
			_loadState = contentLoadStateLabel(ContentLoadState::Error);
			_lastLoadError = error;
			_lastLoadSequence = loadSequence;
			return false;
		}

		_loadedContentKind.clear();
		_loadedContentPath.clear();
		_loadState = contentLoadStateLabel(ContentLoadState::Idle);
		_lastLoadError.clear();
		_lastLoadSequence = loadSequence;
		error.clear();
		return true;
	}

	bool MGStreamViewer::captureGaussianViewSnapshot(const std::string& snapshotPath)
	{
		if (snapshotPath.empty()) {
			return false;
		}

		const auto viewIt = _ibrSubViews.find("Gaussian View");
		if (viewIt == _ibrSubViews.end()) {
			return false;
		}

		boost::filesystem::path outputPath(snapshotPath);
		boost::filesystem::path outputDirectory = outputPath.parent_path();
		std::string fileName = outputPath.filename().string();
		if (fileName.empty()) {
			fileName = "gaussian_view.png";
		}
		if (boost::filesystem::path(fileName).extension().empty()) {
			fileName += ".png";
		}

		const std::string directory = outputDirectory.empty() ? std::string(".") : outputDirectory.string();
		captureView("Gaussian View", directory, fileName);
		return boost::filesystem::exists(boost::filesystem::path(directory) / fileName);
	}

	bool MGStreamViewer::tryGetGaussianViewCamera(sibr::InputCamera& camera, std::string& error) const
	{
		const auto viewIt = _ibrSubViews.find("Gaussian View");
		if (viewIt == _ibrSubViews.end()) {
			error = "Gaussian View camera is not available.";
			return false;
		}

		if (const auto handler = std::dynamic_pointer_cast<InteractiveCameraHandler>(viewIt->second.handler)) {
			camera = handler->getCamera();
		} else {
			camera = viewIt->second.cam;
		}
		error.clear();
		return true;
	}

	bool MGStreamViewer::applyGaussianViewCamera(const sibr::InputCamera& camera, std::string& error)
	{
		auto viewIt = _ibrSubViews.find("Gaussian View");
		if (viewIt == _ibrSubViews.end()) {
			error = "Gaussian View camera is not available.";
			return false;
		}

		if (auto handler = std::dynamic_pointer_cast<InteractiveCameraHandler>(viewIt->second.handler)) {
			handler->fromCamera(camera, false, true);
			viewIt->second.cam = handler->getCamera();
		} else {
			viewIt->second.cam = camera;
		}
		error.clear();
		return true;
	}

	const RenderTargetRGB* MGStreamViewer::getGaussianViewRenderTarget() const
	{
		const auto viewIt = _ibrSubViews.find("Gaussian View");
		if (viewIt == _ibrSubViews.end() || !viewIt->second.rt) {
			return nullptr;
		}
		return viewIt->second.rt.get();
	}

	bool MGStreamViewer::isStreamingIdle() const
	{
		const RenderingSystem* renderingSystem = getRenderingSystem();
		if (!renderingSystem || !renderingSystem->hasManifest()) {
			return true;
		}

		const SwapManager::Stats* stats = renderingSystem->getSwapStats();
		if (!stats) {
			return true;
		}

		return stats->pending_disk_loads == 0
			&& stats->pending_gpu_uploads == 0
			&& stats->pending_gpu_evictions == 0
			&& stats->skipped_instances_last_frame == 0;
	}

	double MGStreamViewer::getAppTimeSeconds() const
	{
		return _appTimeSec;
	}

	uint64_t MGStreamViewer::getFrameIndex() const
	{
		return _frameIndex;
	}

	const std::string& MGStreamViewer::getCurrentPhase() const
	{
		return _currentPhase;
	}

	void MGStreamViewer::setCurrentPhase(const std::string& phase)
	{
		_currentPhase = phase;
	}

	std::vector<std::string> MGStreamViewer::getAvailablePhases() const
	{
		return _manifestStore.phases();
	}

	size_t MGStreamViewer::getManifestAssetCount() const
	{
		return _manifestStore.assets().size();
	}

	bool MGStreamViewer::hasLoadedContent() const
	{
		return _loadState == contentLoadStateLabel(ContentLoadState::Loaded) && !_loadedContentPath.empty();
	}

	const std::string& MGStreamViewer::getLoadedContentKind() const
	{
		return _loadedContentKind;
	}

	const std::string& MGStreamViewer::getLoadedContentPath() const
	{
		return _loadedContentPath;
	}

	const std::string& MGStreamViewer::getLoadState() const
	{
		return _loadState;
	}

	const std::string& MGStreamViewer::getLastLoadError() const
	{
		return _lastLoadError;
	}

	uint64_t MGStreamViewer::getLastLoadSequence() const
	{
		return _lastLoadSequence;
	}

	bool MGStreamViewer::resetCurrentContent(std::string& error)
	{
		error.clear();
		if (auto* renderingSystem = getRenderingSystem()) {
			renderingSystem->setManifest(nullptr);
		}

		std::vector<std::pair<std::string, GaussianInstance*>> instances;
		if (_scene) {
			instances.reserve(_scene->getInstances().size());
			for (const auto& instancePair : _scene->getInstances()) {
				if (instancePair.second) {
					instances.emplace_back(instancePair.first, instancePair.second.get());
				}
			}
		}

		if (auto* renderingSystem = getRenderingSystem()) {
			for (const auto& instancePair : instances) {
				renderingSystem->onInstanceRemoved(*instancePair.second);
			}
		}
		if (_scene) {
			for (const auto& instancePair : instances) {
				_scene->removeInstance(instancePair.first);
			}
		}

		GPUResourceManager::getInstance().clear();
		if (_resourceManager) {
			_resourceManager->clear();
		}
		_manifestStore.clear();
		_loadedManifestPath.clear();
		_currentPhase.clear();
		_selectedInstance = nullptr;
		_selectedField.clear();
		return true;
	}

	void MGStreamViewer::beginContentLoad(const std::string& kind, const std::string& path, uint64_t loadSequence)
	{
		_loadedContentKind = kind;
		_loadedContentPath = path;
		_loadState = contentLoadStateLabel(ContentLoadState::Loading);
		_lastLoadError.clear();
		_lastLoadSequence = loadSequence;
	}

	void MGStreamViewer::finishContentLoad(
		const std::string& kind,
		const std::string& path,
		uint64_t loadSequence,
		bool success,
		const std::string& error)
	{
		_loadedContentKind = kind;
		_loadedContentPath = path;
		_loadState = contentLoadStateLabel(success ? ContentLoadState::Loaded : ContentLoadState::Error);
		_lastLoadError = success ? std::string() : error;
		_lastLoadSequence = loadSequence;
	}

	const char* MGStreamViewer::contentLoadStateLabel(ContentLoadState state)
	{
		switch (state) {
		case ContentLoadState::Idle: return "idle";
		case ContentLoadState::Loading: return "loading";
		case ContentLoadState::Loaded: return "loaded";
		case ContentLoadState::Error: return "error";
		}
		return "unknown";
	}

	bool MGStreamViewer::loadManifestFile(const std::string& path, std::string& error)
	{
		if (!_manifestStore.load(path)) {
			error = "Failed to load manifest: " + path;
			_lastLoadError = error;
			_loadState = contentLoadStateLabel(ContentLoadState::Error);
			return false;
		}

		_loadedManifestPath = _manifestStore.path().string();
		_resourceManager->registerManifest(_manifestStore);
		auto phases = _manifestStore.phases();
		if (!phases.empty() && std::find(phases.begin(), phases.end(), _currentPhase) == phases.end()) {
			_currentPhase = phases.front();
		}

		if (auto* renderingSystem = getRenderingSystem()) {
			renderingSystem->setManifest(&_manifestStore);
		}

		if (_scene && _scene->getInstances().empty()) {
			const size_t createdCount = createManifestInstances(true);
			if (createdCount > 0) {
				SIBR_LOG << "Created " << createdCount << " manifest instance(s)." << std::endl;
			}
		}

		_loadedContentKind = "manifest";
		_loadedContentPath = _loadedManifestPath;
		_loadState = contentLoadStateLabel(ContentLoadState::Loaded);
		_lastLoadError.clear();
		focusCameraOnManifest();
		error.clear();
		return true;
	}

	size_t MGStreamViewer::createManifestInstances(bool onlyMissing)
	{
		if (_manifestStore.empty() || !_scene) {
			return 0;
		}

		std::vector<std::string> assetIds;
		assetIds.reserve(_manifestStore.assets().size());
		for (const auto& assetPair : _manifestStore.assets()) {
			assetIds.push_back(assetPair.first);
		}
		std::sort(assetIds.begin(), assetIds.end());

		size_t createdCount = 0;
		for (const auto& assetId : assetIds) {
			if (onlyMissing && _scene->countInstancesUsingAsset(assetId) > 0) {
				continue;
			}

			std::string instanceName = assetId;
			int suffix = 1;
			while (_scene->getInstance(instanceName) != nullptr) {
				const GaussianInstance* existingInstance = _scene->getInstance(instanceName);
				if (existingInstance && existingInstance->getAssetId() == assetId) {
					instanceName.clear();
					break;
				}
				instanceName = assetId + "_" + std::to_string(suffix++);
			}

			if (instanceName.empty()) {
				continue;
			}

			GaussianInstance* instance = _scene->createInstance(instanceName, assetId);
			if (!instance) {
				continue;
			}

			_selectedInstance = instance;
			_subsystem[RENDERING_SYSTEM]->onInstanceCreated(*instance);
			++createdCount;
		}

		return createdCount;
	}

	void MGStreamViewer::focusCameraOnManifest()
	{
		if (_manifestStore.empty()) {
			return;
		}

		bool hasBounds = false;
		Vector3f minBounds = Vector3f::Zero();
		Vector3f maxBounds = Vector3f::Zero();
		for (const auto& assetPair : _manifestStore.assets()) {
			const auto& descriptor = assetPair.second;
			if (!hasBounds) {
				minBounds = descriptor.bounds_min;
				maxBounds = descriptor.bounds_max;
				hasBounds = true;
				continue;
			}
			minBounds = minBounds.cwiseMin(descriptor.bounds_min);
			maxBounds = maxBounds.cwiseMax(descriptor.bounds_max);
		}

		if (!hasBounds) {
			return;
		}

		focusCameraOnBounds(minBounds, maxBounds);
	}

	void MGStreamViewer::focusCameraOnModel(const GaussianField& field)
	{
		CameraFocusPose camerasJsonPose;
		if (buildCamerasJsonFocusPose(field, camerasJsonPose)) {
			applyFocusCameraPose(
				camerasJsonPose.eye,
				camerasJsonPose.target,
				camerasJsonPose.up,
				field.min_edges,
				field.max_edges,
				camerasJsonPose.has_fovy,
				camerasJsonPose.fovy);
			return;
		}

		if (const auto gaussianCenter = computeGaussianPositionCenter(field)) {
			focusCameraOnPoint(*gaussianCenter, field.min_edges, field.max_edges);
			return;
		}

		focusCameraOnBounds(field.min_edges, field.max_edges);
	}

	void MGStreamViewer::focusCameraOnPoint(const Vector3f& focusPoint, const Vector3f& minBounds, const Vector3f& maxBounds)
	{
		if (!isFiniteVector(focusPoint)) {
			return;
		}

		const Vector3f diagonal = maxBounds - minBounds;
		const float maxExtent = std::max(1.0f, std::max(std::fabs(diagonal.x()), std::max(std::fabs(diagonal.y()), std::fabs(diagonal.z()))));
		const float zMargin = std::max(0.1f, 0.05f * std::fabs(diagonal.z()));
		const float preferredZOffset = std::max(1.0f, 0.25f * maxExtent);
		Vector3f eye = focusPoint;
		eye.z() = focusPoint.z() + preferredZOffset;

		if (isFiniteVector(minBounds) && isFiniteVector(maxBounds) && diagonal.z() > 2.0f * zMargin) {
			const float minZ = minBounds.z() + zMargin;
			const float maxZ = maxBounds.z() - zMargin;
			eye.z() = std::min(maxZ, std::max(minZ, eye.z()));
		}

		// Keep the camera inside the manifest AABB so camera_bounds rules immediately activate.
		const float xyInset = std::max(0.01f, 0.02f * maxExtent);
		if (isFiniteVector(minBounds) && isFiniteVector(maxBounds) && diagonal.x() > 2.0f * xyInset) {
			eye.x() = std::min(maxBounds.x() - xyInset, std::max(minBounds.x() + xyInset, eye.x()));
		}
		if (isFiniteVector(minBounds) && isFiniteVector(maxBounds) && diagonal.y() > 2.0f * xyInset) {
			eye.y() = std::min(maxBounds.y() - xyInset, std::max(minBounds.y() + xyInset, eye.y()));
		}

		applyFocusCameraPose(eye, focusPoint, Vector3f(0.0f, 1.0f, 0.0f), minBounds, maxBounds);
	}

	void MGStreamViewer::focusCameraOnBounds(const Vector3f& minBounds, const Vector3f& maxBounds)
	{
		const Vector3f center = 0.5f * (minBounds + maxBounds);
		const Vector3f diagonal = maxBounds - minBounds;
		const float maxExtent = std::max(1.0f, std::max(diagonal.x(), std::max(diagonal.y(), diagonal.z())));
		const float zMargin = std::max(0.1f, 0.05f * diagonal.z());
		const float preferredZOffset = std::max(1.0f, 0.25f * diagonal.z());
		const float clampedZ = std::min(maxBounds.z() - zMargin, center.z() + preferredZOffset);
		Vector3f eye = center;
		eye.z() = std::max(minBounds.z() + zMargin, clampedZ);

		// Keep the camera inside the manifest AABB so camera_bounds rules immediately activate.
		const float xyInset = std::max(0.01f, 0.02f * maxExtent);
		eye.x() = std::min(maxBounds.x() - xyInset, std::max(minBounds.x() + xyInset, eye.x()));
		eye.y() = std::min(maxBounds.y() - xyInset, std::max(minBounds.y() + xyInset, eye.y()));

		applyFocusCameraPose(eye, center, Vector3f(0.0f, 1.0f, 0.0f), minBounds, maxBounds);
	}

	void MGStreamViewer::applyFocusCameraPose(
		const Vector3f& eye,
		const Vector3f& target,
		const Vector3f& up,
		const Vector3f& minBounds,
		const Vector3f& maxBounds,
		bool hasFovy,
		float fovy)
	{
		const auto viewIt = _ibrSubViews.find("Gaussian View");
		if (viewIt == _ibrSubViews.end()) {
			return;
		}

		auto handler = std::dynamic_pointer_cast<InteractiveCameraHandler>(viewIt->second.handler);
		if (!handler) {
			return;
		}

		const auto forward = normalizedVector(target - eye);
		if (!forward) {
			return;
		}

		Vector3f cameraUp = up;
		if (!isValidDirection(cameraUp) || std::fabs(cameraUp.normalized().dot(*forward)) > 0.98f) {
			cameraUp = std::fabs(forward->dot(Vector3f(0.0f, 1.0f, 0.0f))) < 0.98f
				? Vector3f(0.0f, 1.0f, 0.0f)
				: Vector3f(1.0f, 0.0f, 0.0f);
		}

		const Vector3f diagonal = maxBounds - minBounds;
		const float maxExtent = std::max(1.0f, std::max(std::fabs(diagonal.x()), std::max(std::fabs(diagonal.y()), std::fabs(diagonal.z()))));
		const float focusDistance = std::max(1.0f, (target - eye).norm());
		const float farPlane = std::max(1000.0f, std::max(maxExtent * 20.0f, focusDistance * 4.0f));

		InputCamera focusCamera = viewIt->second.cam;
		focusCamera.setLookAt(eye, eye + *forward, cameraUp);
		if (hasFovy && fovy > 0.0f && fovy < kPi) {
			focusCamera.fovy(fovy);
		}
		focusCamera.znear(0.01f);
		focusCamera.zfar(farPlane);
		handler->fromCamera(focusCamera, false, true);
		viewIt->second.cam = focusCamera;
	}

	const char* MGStreamViewer::cpuStateLabel(CpuState state)
	{
		switch (state) {
		case CpuState::Loading: return "Loading";
		case CpuState::Resident: return "CPU";
		case CpuState::EvictQueued: return "Evicting";
		case CpuState::Failed: return "Failed";
		case CpuState::Unloaded:
		default: return "Unloaded";
		}
	}

	const char* MGStreamViewer::gpuStateLabel(GpuState state)
	{
		switch (state) {
		case GpuState::UploadQueued: return "Uploading";
		case GpuState::Resident: return "GPU";
		case GpuState::EvictQueued: return "Evicting";
		case GpuState::Failed: return "Failed";
		case GpuState::Unloaded:
		default: return "Unloaded";
		}
	}

	std::string MGStreamViewer::formatMegabytes(size_t bytes)
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision(1) << (static_cast<double>(bytes) / (1024.0 * 1024.0));
		return stream.str();
	}

	void MGStreamViewer::onShowScenePanel(Window& win) {
		float sideWidth = 350.0f;
		ImGui::SetNextWindowPos(ImVec2(win.size().x() - sideWidth, 20.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(sideWidth, win.size().y() - 20.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Scene Outliner", &_showScenePanel, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
			char phaseBuffer[128] = {};
			std::snprintf(phaseBuffer, sizeof(phaseBuffer), "%s", _currentPhase.c_str());
			if (ImGui::InputText("Current Phase", phaseBuffer, IM_ARRAYSIZE(phaseBuffer))) {
				_currentPhase = phaseBuffer;
			}

			size_t manifestInstanceCount = 0;
			for (const auto& assetPair : _manifestStore.assets()) {
				if (_scene->countInstancesUsingAsset(assetPair.first) > 0) {
					++manifestInstanceCount;
				}
			}

			if (!_manifestStore.empty()) {
				ImGui::Text("Manifest Instances: %u / %u",
					static_cast<unsigned>(manifestInstanceCount),
					static_cast<unsigned>(_manifestStore.assets().size()));
				if (ImGui::Button("Create Manifest Instances", ImVec2(-1, 25))) {
					const size_t createdCount = createManifestInstances(true);
					SIBR_LOG << "Created " << createdCount << " additional manifest instance(s)." << std::endl;
				}
				if (ImGui::Button("Focus Manifest Camera", ImVec2(-1, 25))) {
					focusCameraOnManifest();
				}
			}

			// Instance Creatttion Button
			if (ImGui::Button("Create New Instance", ImVec2(-1, 25))) {
				ImGui::OpenPopup("Create New Instance");
			}

			// Instance Creation Popup
			if (ImGui::BeginPopupModal("Create New Instance", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				static char nameBuf[128] = "NewInstance";
				static std::string previewAssetId;
				static Vector3f tempPos(0, 0, 0), tempRot(0, 0, 0);
				static float tempScale = 1.0f;

				ImGui::Text("Enter Instance Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf));

				std::string fieldName = previewAssetId.empty() ? "None" : previewAssetId;
				if (ImGui::BeginCombo("Gaussian Asset", fieldName.c_str())) {
					if (ImGui::Selectable("None", previewAssetId.empty())) {
						previewAssetId.clear();
					}

					for (const auto& assetId : _resourceManager->listAssetIds()) {
						if (ImGui::Selectable(assetId.c_str(), assetId == previewAssetId)) {
							previewAssetId = assetId;
						}
					}
					ImGui::EndCombo();
				}

				ImGui::Spacing();
				ImGui::Text("Initial Transform");
				ImGui::DragFloat3("Position", tempPos.data(), 0.1f);
				ImGui::DragFloat3("Rotation", tempRot.data(), 1.0f);
				ImGui::DragFloat("Scale", &tempScale, 0.01f, 0.001f, 100.0f);

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				if (ImGui::Button("Create", ImVec2(120, 0))) {
					_selectedInstance = _scene->createInstance(nameBuf, previewAssetId, tempPos, tempRot, tempScale);
					if (_selectedInstance) {
						_subsystem[RENDERING_SYSTEM]->onInstanceCreated(*_selectedInstance);

						SIBR_LOG << "Instance created: " << nameBuf << (previewAssetId.empty() ? " (No Asset)" : "") << std::endl;
						ImGui::CloseCurrentPopup();
						
						// Reset buffers
						strcpy(nameBuf, "NewInstance");
						previewAssetId.clear();
						tempPos = Vector3f(0, 0, 0);
						tempRot = Vector3f(0, 0, 0);
						tempScale = 1.0f;
					}
					else {
						SIBR_WRG << "Failed to create instance. Check for duplicate name: " << nameBuf << std::endl;
					}
				}

				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {
					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			const bool outlinerOpen = ImGui::BeginChild("OutlinerList", ImVec2(0, 250), true);
			if (outlinerOpen) {
				auto& allInstances = _scene->getInstances();
				for (auto& pair : allInstances) {
					GaussianInstance* inst = pair.second.get();
					bool isSelected = (_selectedInstance == inst);
					if (ImGui::Selectable(pair.first.c_str(), isSelected)) {
						_selectedInstance = inst;
					}
				}
			}
			ImGui::EndChild();

			ImGui::Spacing();
			ImGui::Separator();

			// DETAILS
			if (_selectedInstance) {
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "DETAILS: %s", _selectedInstance->getNameRef().c_str());

				if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
					// 1. Position
					if (ImGui::DragFloat3("Location", _selectedInstance->getPositionRef().data(), 0.1f)) {
					}

					// 2. Rotation
					if (ImGui::DragFloat3("Rotation", _selectedInstance->getEulerRef().data(), 0.5f)) {
					}

					// 3. Scale
					if (ImGui::DragFloat("Scale", &_selectedInstance->getScaleRef(), 0.01f, 0.001f, 100.0f)) {
					}
				}

				if (ImGui::CollapsingHeader("Gaussian Asset", ImGuiTreeNodeFlags_DefaultOpen)) {
					const std::string& currentAssetId = _selectedInstance->getAssetId();
					const auto currentField = _resourceManager->getCpuFieldShared(currentAssetId);
					std::string currentFieldName = currentAssetId.empty() ? "None" : currentAssetId;
					if (!currentField && !currentAssetId.empty()) {
						currentFieldName += " (missing)";
					}

					if (ImGui::BeginCombo("Source Field", currentFieldName.c_str())) {
						if (ImGui::Selectable("None", currentAssetId.empty())) {
							_selectedInstance->setAssetId("");
							_subsystem[RENDERING_SYSTEM]->onInstanceUpdated(*_selectedInstance);
						}
						for (const auto& assetId : _resourceManager->listAssetIds()) {
							bool isSourceSelected = (assetId == currentAssetId);
							if (ImGui::Selectable(assetId.c_str(), isSourceSelected)) {
								_selectedInstance->setAssetId(assetId);
								_subsystem[RENDERING_SYSTEM]->onInstanceUpdated(*_selectedInstance);
							}
						}
						ImGui::EndCombo();
					}

					if (currentField) {
						ImGui::BulletText("Name: %s", currentField->name.c_str());

						ImGui::Bullet();
						ImGui::SameLine();
						ImGui::Text("Path: ");
						ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
						ImGui::TextWrapped("%s", currentField->path.c_str());
						ImGui::PopTextWrapPos();

						ImGui::BulletText("Points: %u", currentField->count);
						ImGui::BulletText("SH Degree: %d", currentField->sh_degree);
					}
				}

				ImGui::Spacing();
				if (ImGui::Button("Delete Instance", ImVec2(-1, 25))) {
					_subsystem[RENDERING_SYSTEM]->onInstanceRemoved(*_selectedInstance);
					_scene->removeInstance(_selectedInstance->getName());
					_selectedInstance = nullptr;
				}
			}
			else {
				ImGui::TextDisabled("Select an instance to edit its properties.");
			}
		}
		ImGui::End();
	}

	void MGStreamViewer::onShowResourceBrowser(Window& win) {
		float browserHeight = 220.0f;
		float sideWidth = 350.0f;
		ImGui::SetNextWindowPos(ImVec2(0, win.size().y() - browserHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(win.size().x() - sideWidth, browserHeight), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Resource Browser", &_showResourceBrowser, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {
			if (ImGui::Button("Load Manifest", ImVec2(120, 30))) {
				std::string manifestPath;
				if (showFilePicker(manifestPath, FilePickerMode::Default, "", "json")) {
					std::string manifestError;
					loadManifestFile(manifestPath, manifestError);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Import PLY", ImVec2(100, 30))) {
				std::string path;
				if (showFilePicker(path, FilePickerMode::Directory)) {
					auto field = GaussianLoader::load(path);
					if (field) {
						_resourceManager->addField(std::move(field));
					}
				}
			}
			ImGui::SameLine();
			const bool canUnloadCurrent = hasLoadedContent();
			if (!canUnloadCurrent) {
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
			}
			if (ImGui::Button("Unload Current", ImVec2(130, 30)) && canUnloadCurrent) {
				std::string unloadError;
				if (!unloadCurrentContent(unloadError)) {
					SIBR_WRG << "Failed to unload current content: " << unloadError << std::endl;
				}
			}
			if (!canUnloadCurrent) {
				ImGui::PopStyleVar();
				ImGui::PopItemFlag();
			}
			ImGui::Separator();

			ImGui::TextWrapped("Manifest: %s", _loadedManifestPath.empty() ? "(none)" : _loadedManifestPath.c_str());
			ImGui::Text("CPU Resident: %s MB", formatMegabytes(_resourceManager->totalCpuBytes()).c_str());
			const RenderingSystem* renderingSystem = getRenderingSystem();
			ImGui::TextUnformatted("VRAM (partial accounting):");
			ImGui::Text("  GPU Assets:       %s MB", formatMegabytes(GPUResourceManager::getInstance().totalBytes()).c_str());
			if (renderingSystem) {
				if (const auto* view = renderingSystem->getView("Gaussian View")) {
					ImGui::Text("  World buffers:    %s MB", formatMegabytes(view->worldBufferBytes()).c_str());
					ImGui::Text("  Scratch (rast):   %s MB", formatMegabytes(view->scratchBufferBytes()).c_str());
					ImGui::Text("  Output+Interop:   %s MB", formatMegabytes(view->outputInteropBytes()).c_str());
				}
			}
			{
				size_t freeB = 0, totalB = 0;
				if (cudaMemGetInfo(&freeB, &totalB) == cudaSuccess) {
					ImGui::Text("  CUDA allocatable used (device): %s / %s MB",
						formatMegabytes(totalB - freeB).c_str(),
						formatMegabytes(totalB).c_str());
				}
			}
			ImGui::TextDisabled("* excludes CUDA runtime/context, cuBLAS/cuDNN, driver overhead");
			const SwapManager::Stats* stats = renderingSystem ? renderingSystem->getSwapStats() : nullptr;
			if (stats) {
				ImGui::Text("Phase: %s", stats->current_phase.empty() ? "(none)" : stats->current_phase.c_str());
				ImGui::Text("Required GPU: %u | Warm CPU: %u", static_cast<unsigned>(stats->required_gpu_count), static_cast<unsigned>(stats->warm_cpu_count));
				ImGui::Text("Disk Loads: %u | Uploads: %u | Evicts: %u",
					static_cast<unsigned>(stats->pending_disk_loads),
					static_cast<unsigned>(stats->pending_gpu_uploads),
					static_cast<unsigned>(stats->pending_gpu_evictions));
				ImGui::Text("Swap Hits: %u | Misses: %u | Skipped Instances: %u",
					static_cast<unsigned>(stats->swap_hits),
					static_cast<unsigned>(stats->swap_misses),
					static_cast<unsigned>(stats->skipped_instances_last_frame));
			}
			ImGui::Separator();

			float tileWidth = 120.0f;
			float tileHeight = 140.0f;
			float padding = 12.0f;
			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = std::max(1, (int)(panelWidth / (tileWidth + padding)));

			const bool assetGridOpen = ImGui::BeginChild("AssetGrid");
			if (assetGridOpen) {
				const auto allAssets = _resourceManager->snapshotAssets();
				int n = 0;
				std::string fieldPendingDelete;

				for (const auto& asset : allAssets) {
					bool isSelected = (_selectedField == asset.id);
					const GpuState gpuState = GPUResourceManager::getInstance().state(asset.id);

					ImGui::PushID(asset.id.c_str());
					ImGui::BeginGroup();

					if (ImGui::Selectable("##tile", isSelected, 0, ImVec2(tileWidth, tileHeight))) {
						_selectedField = asset.id;
					}

					if (ImGui::BeginPopupContextItem("AssetCtx")) {
						if (_manifestStore.assets().find(asset.id) == _manifestStore.assets().end()) {
							if (ImGui::MenuItem("Delete Asset")) {
								fieldPendingDelete = asset.id;
							}
						}
						ImGui::EndPopup();
					}

					ImVec2 cursorPos = ImGui::GetItemRectMin();
					ImVec2 center = ImVec2(cursorPos.x + tileWidth * 0.5f, cursorPos.y + tileHeight * 0.4f);

					ImGui::GetWindowDrawList()->AddCircleFilled(center, 30.0f, ImColor(100, 150, 255, 200));
					ImGui::GetWindowDrawList()->AddCircleFilled(center, 15.0f, ImColor(200, 220, 255, 255));

					ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 5, cursorPos.y + tileHeight - 30));
					ImGui::PushTextWrapPos(cursorPos.x + tileWidth - 5);
					ImGui::TextUnformatted(asset.id.c_str());
					ImGui::TextDisabled("%s / %s", cpuStateLabel(asset.cpu_state), gpuStateLabel(gpuState));
					ImGui::PopTextWrapPos();

					ImGui::EndGroup();
					ImGui::PopID();

					if ((n + 1) % columnCount != 0) {
						ImGui::SameLine(0, padding);
					}
					n++;
				}

				if (!fieldPendingDelete.empty()) {
					const size_t referenceCount = _scene->countInstancesUsingAsset(fieldPendingDelete);
					if (referenceCount > 0) {
						SIBR_WRG << "Cannot delete asset '" << fieldPendingDelete << "' because " << referenceCount << " instance(s) still reference it." << std::endl;
					}
					else {
						GPUResourceManager::getInstance().removeField(fieldPendingDelete);
						_resourceManager->removeField(fieldPendingDelete);
						if (_selectedField == fieldPendingDelete) {
							_selectedField.clear();
						}
					}
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
	}

	void MGStreamViewer::toggleGUI()
	{
		_showGUI = !_showGUI;
		if (!_showGUI) {
			SIBR_LOG << "[MultiViewManager] GUI is now hidden, use Ctrl+Alt+G to toggle it back on." << std::endl;
		}
		toggleSubViewsGUI();
	}
}
