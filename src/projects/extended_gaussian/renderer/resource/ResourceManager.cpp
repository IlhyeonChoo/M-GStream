#include "ResourceManager.hpp"

namespace sibr {
    bool ResourceManager::addField(GaussianField::UPtr field)
    {
        if (!field) return false;

        if (fields.find(field->name) != fields.end()) {
            SIBR_WRG << "GaussianField with name '" << field->name << "' already exists." << std::endl;
            return false;
        }

        fields.emplace(field->name, std::move(field));
        return true;
    }

    const GaussianField* ResourceManager::getField(const std::string& name) const
    {
        auto itr = fields.find(name);
        if (itr == fields.end()) {
            return nullptr;
        }
        return itr->second.get();
    }

    GaussianField* ResourceManager::getField(const std::string& name)
    {
        return const_cast<GaussianField*>(static_cast<const ResourceManager*>(this)->getField(name));
    }

    bool ResourceManager::removeField(const std::string& name)
    {
        auto itr = fields.find(name);
        if (itr == fields.end()) {
            SIBR_WRG << "Cannot remove: GaussianField '" << name << "' not found." << std::endl;
            return false;
        }
        fields.erase(itr);
        return true;
    }

    const std::unordered_map<std::string, GaussianField::UPtr>& ResourceManager::getFields() const
    {
        return fields;
    }
}