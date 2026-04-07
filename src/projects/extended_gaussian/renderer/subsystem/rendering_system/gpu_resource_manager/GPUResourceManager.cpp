#include "GPUResourceManager.hpp"
#include <projects/extended_gaussian/renderer/resource/GaussianField.hpp>

namespace sibr {
    GPUResourceManager& GPUResourceManager::getInstance()
    {
        static GPUResourceManager resource_manager;
        return resource_manager;
    }

    bool GPUResourceManager::addField(const std::string& assetId, const GaussianField* field)
    {
        if (!field || assetId.empty()) {
            return false;
        }
        if (gpu_fields.find(assetId) != gpu_fields.end()) {
            SIBR_WRG << "GPU GaussianField with asset id '" << assetId << "' already exists." << std::endl;
            return false;
        }

        gpu_fields.emplace(assetId, std::make_shared<GPUGaussianField>(assetId, field));
        return true;
    }

    std::shared_ptr<const GPUGaussianField> GPUResourceManager::getField(const std::string& assetId) const
    {
        if (assetId.empty()) {
            return nullptr;
        }
        auto itr = gpu_fields.find(assetId);
        if (itr == gpu_fields.end()) {
            return nullptr;
        }
        return itr->second;
    }

    GPUGaussianField::Ptr GPUResourceManager::getField(const std::string& assetId)
    {
        if (assetId.empty()) {
            return nullptr;
        }
        auto itr = gpu_fields.find(assetId);
        if (itr == gpu_fields.end()) {
            return nullptr;
        }
        return itr->second;
    }

    bool GPUResourceManager::removeField(const std::string& assetId)
    {
        if (assetId.empty()) {
            return false;
        }
        auto itr = gpu_fields.find(assetId);
        if (itr == gpu_fields.end()) {
            SIBR_WRG << "Cannot remove: GPU GaussianField '" << assetId << "' not found." << std::endl;
            return false;
        }
        gpu_fields.erase(itr);
        return true;
    }

    int GPUResourceManager::CleanUp()
    {
        return 0;
    }

    const std::unordered_map<std::string, GPUGaussianField::Ptr>& GPUResourceManager::getFields() const
    {
        return gpu_fields;
    }
}
