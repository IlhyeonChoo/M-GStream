#pragma once

# include <core/system/Config.hpp>
# include <core/view/MultiViewManager.hpp>
# include "Config.hpp"
#include <projects/M_GStream/renderer/scene/GaussianScene.hpp>
#include <projects/M_GStream/renderer/subsystem/Subsystem.hpp>
#include <projects/M_GStream/renderer/resource/ManifestStore.hpp>
#include <projects/M_GStream/renderer/resource/ResourceManager.hpp>
#include <projects/M_GStream/renderer/subsystem/rendering_system/gpu_resource_manager/GPUResourceManager.hpp>

#include <cstddef>
#include <vector>

namespace sibr {
	class RenderingSystem;

	class SIBR_MGSTREAM_EXPORT MGStreamViewer : public sibr::MultiViewBase
	{
	public:
		SIBR_CLASS_PTR(MGStreamViewer);

		/*
		 * \brief Creates a MultiViewManager in a given OS window.
		 * \param window The OS window to use.
		 * \param resize Should the window be resized by the manager to maximize usable space.
		 */
		MGStreamViewer(sibr::Window& window, bool resize = true);

		/**
		 * \brief Update subviews and the MultiViewManager.
		 * \param input The Input state to use.
		 */
		void	onUpdate(sibr::Input& input) override;

		/**
		 * \brief Render the content of the MultiViewManager and its interface
		 * \param win The OS window into which the rendering should be performed.
		 */
		void	onRender(sibr::Window& win) override;

		/**
		 * \brief Render menus and additional gui
		 * \param win The OS window into which the rendering should be performed.
		 */
		void	onGui(sibr::Window& win) override;

		void onSwapBuffer(sibr::Window& win);

		Vector2i getWindowSize() const;

		const GaussianScene* getScene() const;
		GaussianScene* getScene();
		ResourceManager* getResourceManager();
		const ResourceManager* getResourceManager() const;
		RenderingSystem* getRenderingSystem();
		const RenderingSystem* getRenderingSystem() const;
		bool loadModelDirectoryAsInstance(const std::string& modelPath);
		bool replaceWithModelDirectory(const std::string& modelPath, std::string& error, uint64_t loadSequence = 0);
		bool replaceWithManifestFile(const std::string& path, std::string& error, uint64_t loadSequence = 0);
		bool unloadCurrentContent(std::string& error, uint64_t loadSequence = 0);
		bool captureGaussianViewSnapshot(const std::string& snapshotPath);
		bool tryGetGaussianViewCamera(sibr::InputCamera& camera, std::string& error) const;
		bool applyGaussianViewCamera(const sibr::InputCamera& camera, std::string& error);
		const RenderTargetRGB* getGaussianViewRenderTarget() const;
		bool isStreamingIdle() const;
		double getAppTimeSeconds() const;
		uint64_t getFrameIndex() const;
		const std::string& getCurrentPhase() const;
		void setCurrentPhase(const std::string& phase);
		std::vector<std::string> getAvailablePhases() const;
		size_t getManifestAssetCount() const;
		bool hasLoadedContent() const;
		const std::string& getLoadedContentKind() const;
		const std::string& getLoadedContentPath() const;
		const std::string& getLoadState() const;
		const std::string& getLastLoadError() const;
		uint64_t getLastLoadSequence() const;

	private:
		enum class ContentLoadState {
			Idle,
			Loading,
			Loaded,
			Error
		};

		bool loadManifestFile(const std::string& path, std::string& error);
		bool resetCurrentContent(std::string& error);
		void beginContentLoad(const std::string& kind, const std::string& path, uint64_t loadSequence);
		void finishContentLoad(const std::string& kind, const std::string& path, uint64_t loadSequence, bool success, const std::string& error);
		static const char* contentLoadStateLabel(ContentLoadState state);
		size_t createManifestInstances(bool onlyMissing = true);
		void focusCameraOnManifest();
		void focusCameraOnModel(const GaussianField& field);
		void focusCameraOnPoint(const Vector3f& focusPoint, const Vector3f& minBounds, const Vector3f& maxBounds);
		void focusCameraOnBounds(const Vector3f& minBounds, const Vector3f& maxBounds);
		void applyFocusCameraPose(
			const Vector3f& eye,
			const Vector3f& target,
			const Vector3f& up,
			const Vector3f& minBounds,
			const Vector3f& maxBounds,
			bool hasFovy = false,
			float fovy = 0.0f);
		static const char* cpuStateLabel(CpuState state);
		static const char* gpuStateLabel(GpuState state);
		static std::string formatMegabytes(size_t bytes);

		void onShowScenePanel(sibr::Window& win);
		void onShowResourceBrowser(sibr::Window& win);

		/** Show/hide the GUI. */
		void toggleGUI();

		sibr::Window& _window; ///< The OS window.
		sibr::FPSCounter _fpsCounter; ///< A FPS counter.
		bool _showGUI = true; ///< Should the GUI be displayed.
		bool _showScenePanel = false;
		bool _showResourceBrowser = false;

		GaussianInstance* _selectedInstance = nullptr;
		std::string _selectedField;

		GaussianScene::UPtr _scene;
		Subsystem::UPtr _subsystem[SubsystemType::SUBSYSTEM_LAST];
		ResourceManager::UPtr _resourceManager;
		ManifestStore _manifestStore;
		std::string _loadedManifestPath;
		std::string _currentPhase;
		std::string _loadedContentKind;
		std::string _loadedContentPath;
		std::string _loadState = contentLoadStateLabel(ContentLoadState::Idle);
		std::string _lastLoadError;
		uint64_t _lastLoadSequence = 0;
		double _appTimeSec = 0.0;
		uint64_t _frameIndex = 0;
	};
}
