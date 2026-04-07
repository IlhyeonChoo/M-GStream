#pragma once

#include <unordered_map>
# include <core/system/Config.hpp>
#include "Config.hpp"
#include "GaussianField.hpp"

namespace sibr {
	class SIBR_EXTENDED_GAUSSIAN_EXPORT ResourceManager {
	public:
		SIBR_CLASS_PTR(ResourceManager);

		ResourceManager() = default;

		ResourceManager(const ResourceManager&) = delete;
		ResourceManager& operator=(const ResourceManager&) = delete;

		bool addField(GaussianField::UPtr field);

		const GaussianField* getField(const std::string& name) const;
		GaussianField* getField(const std::string& name);

		bool removeField(const std::string& name);

		const std::unordered_map<std::string, GaussianField::UPtr>& getFields() const;

	private:
		std::unordered_map<std::string, GaussianField::UPtr> fields;
	};
}