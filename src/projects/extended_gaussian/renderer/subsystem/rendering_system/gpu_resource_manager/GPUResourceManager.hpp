#pragma once

#include <core/system/Config.hpp>
#include "Config.hpp"
#include "GPUGaussianField.hpp"

#include <unordered_map>

namespace sibr {
	class GaussianField;

	class SIBR_EXTENDED_GAUSSIAN_EXPORT GPUResourceManager {
	public:
		SIBR_CLASS_PTR(GPUResourceManager);

		static GPUResourceManager& getInstance();

		GPUResourceManager(const GPUResourceManager&) = delete;
		GPUResourceManager& operator=(const GPUResourceManager&) = delete;

		bool addField(const std::string& assetId, const GaussianField* field);

		std::shared_ptr<const GPUGaussianField> getField(const std::string& assetId) const;
		GPUGaussianField::Ptr getField(const std::string& assetId);

		bool removeField(const std::string& assetId);

		const std::unordered_map<std::string, GPUGaussianField::Ptr>& getFields() const;

		int CleanUp();

	private:
		GPUResourceManager() = default;
		std::unordered_map<std::string, GPUGaussianField::Ptr> gpu_fields;
	};
}
