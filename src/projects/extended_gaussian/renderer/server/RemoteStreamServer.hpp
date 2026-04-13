#pragma once

#include "Config.hpp"
#include "ServerProtocol.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace sibr {

struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT RendererHealthSnapshot {
	bool initialized = false;
	bool has_manifest = false;
	uint64_t frame_index = 0;
	double app_time_sec = 0.0;
};

struct SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT ServerStats {
	bool running = false;
	std::string listen_host;
	int listen_port = 0;
	std::string www_root;
	uint64_t total_http_requests = 0;
};

class SIBR_EXTENDED_GAUSSIAN_SERVER_EXPORT RemoteStreamServer {
public:
	explicit RemoteStreamServer(ServerOptions options);
	~RemoteStreamServer();

	RemoteStreamServer(const RemoteStreamServer&) = delete;
	RemoteStreamServer& operator=(const RemoteStreamServer&) = delete;

	bool start(std::string& error);
	void stop();
	bool running() const;

	void setRendererHealthSnapshot(const RendererHealthSnapshot& snapshot);
	RendererHealthSnapshot rendererHealthSnapshot() const;
	ServerStats stats() const;

private:
	class Impl;

	void serverThreadMain();
	std::string resolveWwwRoot(std::string& error) const;

	ServerOptions options_;
	std::string www_root_;
	std::atomic<bool> stop_requested_{ false };
	std::atomic<bool> running_{ false };
	std::thread server_thread_;
	std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::time_point::min();

	mutable std::mutex renderer_health_mutex_;
	RendererHealthSnapshot renderer_health_;

	mutable std::mutex stats_mutex_;
	ServerStats stats_;

	std::unique_ptr<Impl> impl_;
};

} // namespace sibr
