
#include "ExtendedGaussianViewer.hpp"
#include <projects/extended_gaussian/renderer/resource/GaussianLoader.hpp>
#include <projects/extended_gaussian/renderer/subsystem/rendering_system/RenderingSystem.hpp>

namespace sibr {
	ExtendedGaussianViewer::ExtendedGaussianViewer(Window& window, bool resize)
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
	}

	void ExtendedGaussianViewer::onUpdate(Input& input)
	{
		MultiViewBase::onUpdate(input);

		if (input.key().isActivated(Key::LeftControl) && input.key().isActivated(Key::LeftAlt) && input.key().isReleased(Key::G)) {
			toggleGUI();
		}
	}

	void ExtendedGaussianViewer::onRender(Window& win)
	{
		win.viewport().bind();
		glClearColor(37.f / 255.f, 37.f / 255.f, 38.f / 255.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(1.f, 1.f, 1.f, 1.f);

		onGui(win);

		MultiViewBase::onRender(win);

		_fpsCounter.update(_enableGUI && _showGUI);
	}

	void ExtendedGaussianViewer::onGui(Window& win)
	{
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

	void ExtendedGaussianViewer::onSwapBuffer(sibr::Window& win)
	{
		win.swapBuffer();
	}

	Vector2i ExtendedGaussianViewer::getWindowSize() const
	{
		return _window.size();
	}

	const GaussianScene* ExtendedGaussianViewer::getScene() const {
		return _scene.get();
	}

	const ResourceManager* ExtendedGaussianViewer::getResourceManager() const {
		return _resourceManager.get();
	}

	void ExtendedGaussianViewer::onShowScenePanel(Window& win) {
		float sideWidth = 350.0f;
		ImGui::SetNextWindowPos(ImVec2(win.size().x() - sideWidth, 20.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(sideWidth, win.size().y() - 20.0f), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Scene Outliner", &_showScenePanel, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {

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

					for (auto& pair : _resourceManager->getFields()) {
						if (ImGui::Selectable(pair.first.c_str(), pair.first == previewAssetId)) {
							previewAssetId = pair.first;
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
						_subsystem[RENDERING_SYSTEM]->onInstaceCreated(*_selectedInstance);

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

			if (ImGui::BeginChild("OutlinerList", ImVec2(0, 250), true)) {
				auto& allInstances = _scene->getInstances();
				for (auto& pair : allInstances) {
					GaussianInstance* inst = pair.second.get();
					bool isSelected = (_selectedInstance == inst);
					if (ImGui::Selectable(pair.first.c_str(), isSelected)) {
						_selectedInstance = inst;
					}
				}
				ImGui::EndChild();
			}

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
					if (ImGui::DragFloat3("Rotation", _selectedInstance->getEularRef().data(), 0.5f)) {
						
					}

					// 3. Scale
					if (ImGui::DragFloat("Scale", &_selectedInstance->getScaleRef(), 0.01f, 0.001f, 100.0f)) {
						
					}
				}

				if (ImGui::CollapsingHeader("Gaussian Asset", ImGuiTreeNodeFlags_DefaultOpen)) {
					const std::string& currentAssetId = _selectedInstance->getAssetId();
					const GaussianField* currentField = _resourceManager->getField(currentAssetId);
					std::string currentFieldName = currentAssetId.empty() ? "None" : currentAssetId;
					if (!currentField && !currentAssetId.empty()) {
						currentFieldName += " (missing)";
					}

					if (ImGui::BeginCombo("Source Field", currentFieldName.c_str())) {
						if (ImGui::Selectable("None", currentAssetId.empty())) {
							_selectedInstance->setAssetId("");
							_subsystem[RENDERING_SYSTEM]->onInstaceUpdated(*_selectedInstance);
						}
						for (auto& pair : _resourceManager->getFields()) {
							bool isSourceSelected = (pair.first == currentAssetId);
							if (ImGui::Selectable(pair.first.c_str(), isSourceSelected)) {
								_selectedInstance->setAssetId(pair.first);
								_subsystem[RENDERING_SYSTEM]->onInstaceUpdated(*_selectedInstance);
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

	void ExtendedGaussianViewer::onShowResourceBrowser(Window& win) {
		float browserHeight = 220.0f;
		float sideWidth = 350.0f;
		ImGui::SetNextWindowPos(ImVec2(0, win.size().y() - browserHeight), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(win.size().x() - sideWidth, browserHeight), ImGuiCond_FirstUseEver);

		if (ImGui::Begin("Resource Browser", &_showResourceBrowser, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize)) {

			if (ImGui::Button("Import PLY", ImVec2(100, 30))) {
				std::string path;
				if (showFilePicker(path, FilePickerMode::Directory)) {
					auto field = GaussianLoader::load(path);
					if (field) _resourceManager->addField(std::move(field));
				}
			}
			ImGui::Separator();

			float tileWidth = 120.0f;
			float tileHeight = 140.0f;
			float padding = 12.0f;
			float panelWidth = ImGui::GetContentRegionAvail().x;
			int columnCount = std::max(1, (int)(panelWidth / (tileWidth + padding)));

			if (ImGui::BeginChild("AssetGrid")) {
				auto& allFields = _resourceManager->getFields();
				int n = 0;
				std::string fieldPendingDelete;

				for (auto& pair : allFields) {
					bool isSelected = (_selectedField == pair.first);

					ImGui::PushID(pair.first.c_str()); 
					ImGui::BeginGroup();

					if (ImGui::Selectable("##tile", isSelected, 0, ImVec2(tileWidth, tileHeight))) {
						_selectedField = pair.first;
					}

					if (ImGui::BeginPopupContextItem("AssetCtx")) {
						if (ImGui::MenuItem("Delete Asset")) {
							fieldPendingDelete = pair.first;
						}
						ImGui::EndPopup();
					}

					ImVec2 cursorPos = ImGui::GetItemRectMin();
					ImVec2 center = ImVec2(cursorPos.x + tileWidth * 0.5f, cursorPos.y + tileHeight * 0.4f);

					ImGui::GetWindowDrawList()->AddCircleFilled(center, 30.0f, ImColor(100, 150, 255, 200));
					ImGui::GetWindowDrawList()->AddCircleFilled(center, 15.0f, ImColor(200, 220, 255, 255));

					ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 5, cursorPos.y + tileHeight - 30));
					ImGui::PushTextWrapPos(cursorPos.x + tileWidth - 5);
					ImGui::TextUnformatted(pair.first.c_str());
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

				ImGui::EndChild();
			}
		}
		ImGui::End();
	}

	void ExtendedGaussianViewer::toggleGUI()
	{
		_showGUI = !_showGUI;
		if (!_showGUI) {
			SIBR_LOG << "[MultiViewManager] GUI is now hidden, use Ctrl+Alt+G to toggle it back on." << std::endl;
		}
		toggleSubViewsGUI();
	}
}
