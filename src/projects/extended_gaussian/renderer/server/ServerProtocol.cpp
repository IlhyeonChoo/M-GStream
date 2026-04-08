#include "ServerProtocol.hpp"

#include "CameraPoseAdapter.hpp"

#include "picojson/picojson.hpp"

#include <cmath>
#include <sstream>

namespace {

	constexpr float kPi = 3.14159265358979323846f;

	bool isFiniteFloat(float value)
	{
		return std::isfinite(value) != 0;
	}

	bool parseStringField(
		const picojson::object& object,
		const std::string& key,
		std::string& value,
		std::string& error)
	{
		const auto it = object.find(key);
		if (it == object.end()) {
			error = "Missing required string field '" + key + "'.";
			return false;
		}
		if (!it->second.is<std::string>()) {
			error = "Field '" + key + "' must be a string.";
			return false;
		}
		value = it->second.get<std::string>();
		return true;
	}

	bool parseFloatField(
		const picojson::object& object,
		const std::string& key,
		float& value,
		std::string& error)
	{
		const auto it = object.find(key);
		if (it == object.end()) {
			error = "Missing required numeric field '" + key + "'.";
			return false;
		}
		if (!it->second.is<double>()) {
			error = "Field '" + key + "' must be a number.";
			return false;
		}
		value = static_cast<float>(it->second.get<double>());
		if (!isFiniteFloat(value)) {
			error = "Field '" + key + "' must be finite.";
			return false;
		}
		return true;
	}

	bool parseVector3Field(
		const picojson::object& object,
		const std::string& key,
		sibr::Vector3f& value,
		std::string& error)
	{
		const auto it = object.find(key);
		if (it == object.end()) {
			error = "Missing required vector field '" + key + "'.";
			return false;
		}
		if (!it->second.is<picojson::array>()) {
			error = "Field '" + key + "' must be an array.";
			return false;
		}

		const auto& array = it->second.get<picojson::array>();
		if (array.size() != 3) {
			error = "Field '" + key + "' must contain exactly three numbers.";
			return false;
		}

		for (size_t index = 0; index < array.size(); ++index) {
			if (!array[index].is<double>()) {
				error = "Field '" + key + "' must contain only numbers.";
				return false;
			}
			value[static_cast<int>(index)] = static_cast<float>(array[index].get<double>());
			if (!isFiniteFloat(value[static_cast<int>(index)])) {
				error = "Field '" + key + "' must contain only finite numbers.";
				return false;
			}
		}

		return true;
	}

	int sanitizedPositiveInt(int value, int fallback)
	{
		return value > 0 ? value : fallback;
	}

	bool hasTrailingContent(std::istream& stream)
	{
		stream >> std::ws;
		return stream.peek() != std::char_traits<char>::eof();
	}

} // namespace

namespace sibr {

	ServerOptions ParseServerOptions(const CommandLineArgs& args)
	{
		ServerOptions options;
		options.enabled = args.get<bool>("server", false);

		const std::string listenHost = args.get<std::string>("listen-host", options.listen_host);
		if (!listenHost.empty()) {
			options.listen_host = listenHost;
		}

		const int listenPort = args.get<int>("listen-port", options.listen_port);
		if (listenPort > 0 && listenPort <= 65535) {
			options.listen_port = listenPort;
		}

		options.stream_width = sanitizedPositiveInt(
			args.get<int>("stream-width", options.stream_width),
			options.stream_width);
		options.stream_height = sanitizedPositiveInt(
			args.get<int>("stream-height", options.stream_height),
			options.stream_height);
		options.stream_fps = sanitizedPositiveInt(
			args.get<int>("stream-fps", options.stream_fps),
			options.stream_fps);
		return options;
	}

	ParseControlMessageResult ParseControlMessageJson(const std::string& payload)
	{
		ParseControlMessageResult result;

		std::istringstream stream(payload);
		picojson::value rootValue;
		const std::string parseError = picojson::parse(rootValue, stream);
		if (!parseError.empty()) {
			result.error = "Invalid control JSON: " + parseError;
			return result;
		}
		if (hasTrailingContent(stream)) {
			result.error = "Control message must not contain trailing content.";
			return result;
		}

		if (!rootValue.is<picojson::object>()) {
			result.error = "Control message root must be a JSON object.";
			return result;
		}

		const auto& object = rootValue.get<picojson::object>();

		std::string type;
		if (!parseStringField(object, "type", type, result.error)) {
			return result;
		}

		if (type != "set_camera_pose") {
			result.error = "Unsupported control message type '" + type + "'.";
			return result;
		}

		ControlMessage message;
		message.type = ControlMessageType::SetCameraPose;

		if (!parseVector3Field(object, "position", message.camera_pose.position, result.error)) {
			return result;
		}
		if (!parseVector3Field(object, "forward", message.camera_pose.forward, result.error)) {
			return result;
		}
		if (!parseVector3Field(object, "up", message.camera_pose.up, result.error)) {
			return result;
		}
		if (!parseFloatField(object, "fovy", message.camera_pose.fovy, result.error)) {
			return result;
		}

		if (message.camera_pose.fovy <= 0.0f || message.camera_pose.fovy >= kPi) {
			result.error = "Field 'fovy' must be in the open interval (0, pi).";
			return result;
		}

		if (!ValidateRemoteCameraPose(message.camera_pose, result.error)) {
			return result;
		}

		result.ok = true;
		result.message = message;
		return result;
	}

} // namespace sibr
