#pragma once

#include <projects/M_GStream/renderer/subsystem/Subsystem.hpp>
#include <core/system/Config.hpp>
#include "Config.hpp"
#include "RenderGaussianScene.hpp"
#include "GaussianView.hpp"
#include "SwapManager.hpp"
#include "gpu_resource_manager/GPUResourceManager.hpp"

#include <unordered_map>

namespace sibr
{
	class MGStreamViewer;
	class ResourceManager;
	class ManifestStore;

	class SIBR_MGSTREAM_EXPORT RenderingSystem : public Subsystem
	{
	public:
		SIBR_CLASS_PTR(RenderingSystem);

		RenderingSystem();

		void onSystemAdded(MGStreamViewer& owner) override;

		void onSystemRemoved(MGStreamViewer& owner) override;

		void onInstanceCreated(GaussianInstance& instance) override;

		void onInstanceUpdated(GaussianInstance& instance) override;

		void onInstanceRemoved(GaussianInstance& instance) override;

		void onRender() override;

		bool addView(GaussianView::Ptr field);

		const GaussianView* getView(const std::string& name) const;
		GaussianView* getView(const std::string& name);

		bool removeView(const std::string& name);

		const RenderGaussianScene* getScene() const;
		void setManifest(const ManifestStore* manifest);
		void tickStreaming(const ViewerContext& context);
		bool hasManifest() const;
		const SwapManager::Stats* getSwapStats() const;
		void clearSceneInstances();

		~RenderingSystem();

	private:
		void syncRenderInstanceAsset(RenderGaussianInstance& renderInstance, const GaussianInstance& instance);
		void ensureManualGpuResidency();

		int device;
		ResourceManager* resources = nullptr;
		std::unordered_map<std::string, GaussianView::Ptr> views;
		RenderGaussianScene::UPtr scene;
		SwapManager::UPtr swapManager;
	};
}
