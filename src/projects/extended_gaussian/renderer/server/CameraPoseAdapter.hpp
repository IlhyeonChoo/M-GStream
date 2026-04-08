#pragma once

#include "ServerProtocol.hpp"

#include <core/assets/InputCamera.hpp>

#include <string>

namespace sibr {

	SIBR_EXTENDED_GAUSSIAN_EXPORT bool ValidateRemoteCameraPose(const RemoteCameraPose& pose, std::string& error);
	SIBR_EXTENDED_GAUSSIAN_EXPORT bool TryBuildInputCamera(const RemoteCameraPose& pose, sibr::InputCamera& camera, std::string& error);
	SIBR_EXTENDED_GAUSSIAN_EXPORT RemoteCameraPose ExportRemoteCameraPose(const sibr::InputCamera& camera);

} // namespace sibr
