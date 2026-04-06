#pragma once

#include <projects/extended_gaussian/renderer/subsystem/Subsystem.hpp>
#include <core/system/Config.hpp>
#include "Config.hpp"
#include "RenderGaussianScene.hpp"
#include "GaussianView.hpp"
#include "gpu_resource_manager/GPUResourceManager.hpp"

#include <unordered_map>

namespace sibr
{
	class ExtendedGaussianViewer;
	class ResourceManager;

	class SIBR_EXTENDED_GAUSSIAN_EXPORT RenderingSystem : public Subsystem
	{
	public:
		SIBR_CLASS_PTR(RenderingSystem);

		RenderingSystem();

		void onSystemAdded(ExtendedGaussianViewer& owner) override;

		void onSystemRemoved(ExtendedGaussianViewer& owner) override;

		void onInstaceCreated(GaussianInstance& instance) override;

		void onInstaceUpdated(GaussianInstance& instance) override;

		void onInstanceRemoved(GaussianInstance& instance) override;

		void onRender() override;

		bool addView(GaussianView::Ptr field);

		const GaussianView* getView(const std::string& name) const;
		GaussianView* getView(const std::string& name);

		bool removeView(const std::string& name);

		const RenderGaussianScene* getScene() const;

		~RenderingSystem();

	private:
		void syncRenderInstanceAsset(RenderGaussianInstance& renderInstance, const GaussianInstance& instance);

		int device;
		const ResourceManager* resources = nullptr;
		std::unordered_map<std::string, GaussianView::Ptr> views;
		RenderGaussianScene::UPtr scene;
	};
}
