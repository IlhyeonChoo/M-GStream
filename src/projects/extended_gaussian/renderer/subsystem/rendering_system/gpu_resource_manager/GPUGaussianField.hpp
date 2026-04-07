#pragma once

#include <core/system/Config.hpp>
#include "Config.hpp"

#include <string>

namespace sibr {
	class GaussianField;

	class SIBR_EXTENDED_GAUSSIAN_EXPORT GPUGaussianField {
	public:
		SIBR_CLASS_PTR(GPUGaussianField);

		GPUGaussianField(const std::string& p_assetId, const GaussianField* p_origin);

		GPUGaussianField(const GPUGaussianField&) = delete;
		GPUGaussianField& operator=(const GPUGaussianField&) = delete;

		~GPUGaussianField();

		std::string asset_id;
		int count;
		float* pos_cuda;
		float* rot_cuda;
		float* scale_cuda;
		float* opacity_cuda;
		float* shs_cuda;
	};
}
