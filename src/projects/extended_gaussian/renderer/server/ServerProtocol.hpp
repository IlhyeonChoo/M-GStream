#pragma once

#include "Config.hpp"

#include <core/system/CommandLineArgs.hpp>
#include <core/system/Vector.hpp>

#include <string>

namespace sibr {

	struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ServerOptions {
		bool enabled = false;
		std::string listen_host = "127.0.0.1";
		int listen_port = 8080;
		int stream_width = 1280;
		int stream_height = 720;
		int stream_fps = 15;
		std::string www_root;
	};

	struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT RemoteCameraPose {
		Vector3f position = Vector3f::Zero();
		Vector3f forward = Vector3f(0.0f, 0.0f, -1.0f);
		Vector3f up = Vector3f(0.0f, 1.0f, 0.0f);
		float fovy = 0.78539816339f;
	};

	enum class ControlMessageType {
		SetCameraPose
	};

	struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ControlMessage {
		ControlMessageType type = ControlMessageType::SetCameraPose;
		RemoteCameraPose camera_pose;
	};

	struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ParseControlMessageResult {
		bool ok = false;
		ControlMessage message;
		std::string error;

		explicit operator bool() const
		{
			return ok;
		}
	};

	SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ServerOptions ParseServerOptions(const CommandLineArgs& args);
	SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ParseControlMessageResult ParseControlMessageJson(const std::string& payload);

} // namespace sibr
