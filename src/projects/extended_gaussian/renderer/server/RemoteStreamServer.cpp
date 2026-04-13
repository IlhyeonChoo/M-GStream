#include "RemoteStreamServer.hpp"

#include <core/system/Utils.hpp>

#include "MjpegStreamer.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace fs = std::filesystem;

namespace {

constexpr const char* kServiceName = "extended_gaussian_remote_stream";
constexpr const char* kVersionName = "m5-mjpeg-stream";
constexpr const char* kMjpegBoundary = "ExGaussBoundary";
constexpr auto kAcceptSleep = std::chrono::milliseconds(25);
constexpr auto kStreamWait = std::chrono::milliseconds(250);

struct StreamClientSession {
    explicit StreamClientSession(tcp::socket&& accepted_socket)
        : socket(std::make_shared<tcp::socket>(std::move(accepted_socket)))
    {
    }

    std::shared_ptr<tcp::socket> socket;
    std::atomic<bool> stop_requested{ false };
    std::atomic<bool> finished{ false };
    std::thread thread;
};

std::string jsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string mimeTypeForPath(const fs::path& path)
{
    const std::string extension = toLower(path.extension().string());
    if (extension == ".html") {
        return "text/html; charset=utf-8";
    }
    if (extension == ".js") {
        return "application/javascript; charset=utf-8";
    }
    if (extension == ".css") {
        return "text/css; charset=utf-8";
    }
    if (extension == ".json") {
        return "application/json; charset=utf-8";
    }
    if (extension == ".png") {
        return "image/png";
    }
    if (extension == ".jpg" || extension == ".jpeg") {
        return "image/jpeg";
    }
    if (extension == ".svg") {
        return "image/svg+xml";
    }
    if (extension == ".wasm") {
        return "application/wasm";
    }
    return "application/octet-stream";
}

bool isHexDigit(char ch)
{
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

int fromHex(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    const char lowered = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (lowered >= 'a' && lowered <= 'f') {
        return 10 + (lowered - 'a');
    }
    return -1;
}

std::string percentDecode(const std::string& path, std::string& error)
{
    std::string decoded;
    decoded.reserve(path.size());
    for (size_t index = 0; index < path.size(); ++index) {
        const char ch = path[index];
        if (ch != '%') {
            decoded.push_back(ch);
            continue;
        }
        if (index + 2 >= path.size() || !isHexDigit(path[index + 1]) || !isHexDigit(path[index + 2])) {
            error = "Invalid percent-encoded path.";
            return {};
        }
        const int hi = fromHex(path[index + 1]);
        const int lo = fromHex(path[index + 2]);
        decoded.push_back(static_cast<char>((hi << 4) | lo));
        index += 2;
    }
    return decoded;
}

std::string stripQueryAndFragment(const std::string& target)
{
    const size_t marker = target.find_first_of("?#");
    if (marker == std::string::npos) {
        return target;
    }
    return target.substr(0, marker);
}

bool isSafeRelativePath(const std::string& relative_path)
{
    if (relative_path.empty()) {
        return false;
    }
    if (relative_path.front() == '/' || relative_path.front() == '\\') {
        return false;
    }

    std::string normalized = relative_path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::stringstream stream(normalized);
    std::string component;
    while (std::getline(stream, component, '/')) {
        if (component.empty() || component == ".") {
            continue;
        }
        if (component == ".." || component.find(':') != std::string::npos) {
            return false;
        }
    }
    return true;
}

std::string readFileBytes(const fs::path& path, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        error = "Failed to open file '" + path.string() + "'.";
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

template <typename Body>
void applyCommonHeaders(http::response<Body>& response)
{
    response.version(11);
    response.keep_alive(false);
    response.set(http::field::server, "extended_gaussian_server");
}

http::response<http::string_body> makeTextResponse(
    http::status status,
    const std::string& content_type,
    const std::string& body)
{
    http::response<http::string_body> response{ status, 11 };
    applyCommonHeaders(response);
    response.set(http::field::content_type, content_type);
    response.body() = body;
    response.prepare_payload();
    return response;
}

http::response<http::empty_body> makeHeadResponse(
    http::status status,
    const std::string& content_type,
    uint64_t content_length)
{
    http::response<http::empty_body> response{ status, 11 };
    applyCommonHeaders(response);
    response.set(http::field::content_type, content_type);
    response.content_length(content_length);
    return response;
}

std::string jpegBackendName()
{
#if defined(SIBR_EXTENDED_GAUSSIAN_TURBOJPEG_AVAILABLE)
    return "TurboJPEG";
#else
    return "OpenCV";
#endif
}

std::string healthJson(
    const sibr::ServerStats& stats,
    const sibr::RendererHealthSnapshot& renderer,
    double uptime_sec)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream
        << "{\n"
        << "  \"ok\": true,\n"
        << "  \"service\": \"" << kServiceName << "\",\n"
        << "  \"version\": \"" << kVersionName << "\",\n"
        << "  \"uptime_sec\": " << uptime_sec << ",\n"
        << "  \"server\": {\n"
        << "    \"listen_host\": \"" << jsonEscape(stats.listen_host) << "\",\n"
        << "    \"listen_port\": " << stats.listen_port << ",\n"
        << "    \"www_root\": \"" << jsonEscape(stats.www_root) << "\",\n"
        << "    \"total_http_requests\": " << stats.total_http_requests << "\n"
        << "  },\n"
        << "  \"stream\": {\n"
        << "    \"jpeg_backend\": \"" << jpegBackendName() << "\",\n"
        << "    \"active_clients\": " << stats.active_stream_clients << ",\n"
        << "    \"frames_captured\": " << stats.stream_frames_captured << ",\n"
        << "    \"frames_dropped\": " << stats.stream_frames_dropped << ",\n"
        << "    \"queue_dropped\": " << stats.stream_queue_dropped << ",\n"
        << "    \"frames_published\": " << stats.stream_frames_published << ",\n"
        << "    \"latest_sequence\": " << stats.stream_latest_sequence << ",\n"
        << "    \"width\": " << stats.stream_width << ",\n"
        << "    \"height\": " << stats.stream_height << ",\n"
        << "    \"fps\": " << stats.stream_fps << "\n"
        << "  },\n"
        << "  \"renderer\": {\n"
        << "    \"initialized\": " << (renderer.initialized ? "true" : "false") << ",\n"
        << "    \"has_manifest\": " << (renderer.has_manifest ? "true" : "false") << ",\n"
        << "    \"frame_index\": " << renderer.frame_index << ",\n"
        << "    \"app_time_sec\": " << renderer.app_time_sec << "\n"
        << "  }\n"
        << "}";
    return stream.str();
}

std::string mjpegStreamHeader()
{
    std::ostringstream stream;
    stream
        << "HTTP/1.1 200 OK\r\n"
        << "Server: extended_gaussian_server\r\n"
        << "Connection: close\r\n"
        << "Cache-Control: no-store, no-cache, must-revalidate, private\r\n"
        << "Pragma: no-cache\r\n"
        << "Expires: 0\r\n"
        << "Content-Type: multipart/x-mixed-replace; boundary=" << kMjpegBoundary << "\r\n\r\n";
    return stream.str();
}

std::string mjpegPartHeader(const sibr::MjpegStreamer::EncodedFrame& frame)
{
    const size_t content_length = frame.jpeg_bytes ? frame.jpeg_bytes->size() : 0;
    std::ostringstream stream;
    stream
        << "--" << kMjpegBoundary << "\r\n"
        << "Content-Type: image/jpeg\r\n"
        << "Content-Length: " << content_length << "\r\n"
        << "X-Sequence: " << frame.sequence << "\r\n"
        << "X-Source-Frame-Index: " << frame.source_frame_index << "\r\n"
        << "X-Width: " << frame.width << "\r\n"
        << "X-Height: " << frame.height << "\r\n\r\n";
    return stream.str();
}

void closeSocket(tcp::socket& socket)
{
    beast::error_code ec;
    socket.shutdown(tcp::socket::shutdown_both, ec);
    socket.close(ec);
}

} // namespace

namespace sibr {

class RemoteStreamServer::Impl {
public:
    std::unique_ptr<asio::io_context> io_context;
    std::unique_ptr<tcp::acceptor> acceptor;
    std::mutex stream_sessions_mutex;
    std::vector<std::unique_ptr<StreamClientSession>> stream_sessions;
};

RemoteStreamServer::RemoteStreamServer(ServerOptions options)
    : options_(std::move(options)), impl_(std::make_unique<Impl>())
{
}

RemoteStreamServer::~RemoteStreamServer()
{
    stop();
}

bool RemoteStreamServer::start(std::string& error)
{
    if (running()) {
        error.clear();
        return true;
    }

    www_root_ = resolveWwwRoot(error);
    if (www_root_.empty()) {
        return false;
    }

    auto io_context = std::make_unique<asio::io_context>(1);
    auto acceptor = std::make_unique<tcp::acceptor>(*io_context);
    const std::string listen_port = std::to_string(options_.listen_port);

    beast::error_code ec;
    tcp::resolver resolver(*io_context);
    const auto endpoints = resolver.resolve(
        options_.listen_host, listen_port, tcp::resolver::flags::passive, ec);
    if (ec || endpoints.begin() == endpoints.end()) {
        error = "Failed to resolve listen endpoint '" + options_.listen_host + ":" + listen_port + "': " + ec.message();
        return false;
    }

    const tcp::endpoint endpoint = endpoints.begin()->endpoint();
    acceptor->open(endpoint.protocol(), ec);
    if (ec) {
        error = "Failed to open acceptor: " + ec.message();
        return false;
    }
    acceptor->set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
        error = "Failed to set reuse_address: " + ec.message();
        return false;
    }
    acceptor->bind(endpoint, ec);
    if (ec) {
        error = "Failed to bind '" + options_.listen_host + ":" + listen_port + "': " + ec.message();
        return false;
    }
    acceptor->listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        error = "Failed to listen on '" + options_.listen_host + ":" + listen_port + "': " + ec.message();
        return false;
    }
    acceptor->non_blocking(true, ec);
    if (ec) {
        error = "Failed to enable non-blocking accept: " + ec.message();
        return false;
    }

    mjpeg_streamer_ = std::make_unique<MjpegStreamer>(options_);
    mjpeg_streamer_->start();

    impl_->io_context = std::move(io_context);
    impl_->acceptor = std::move(acceptor);
    stop_requested_.store(false);
    running_.store(true);
    start_time_ = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.running = true;
        stats_.listen_host = options_.listen_host;
        stats_.listen_port = options_.listen_port;
        stats_.www_root = www_root_;
        stats_.total_http_requests = 0;
        stats_.active_stream_clients = 0;
        stats_.stream_frames_captured = 0;
        stats_.stream_frames_dropped = 0;
        stats_.stream_queue_dropped = 0;
        stats_.stream_frames_published = 0;
        stats_.stream_latest_sequence = 0;
        stats_.stream_width = options_.stream_width;
        stats_.stream_height = options_.stream_height;
        stats_.stream_fps = options_.stream_fps;
    }

    try {
        server_thread_ = std::thread(&RemoteStreamServer::serverThreadMain, this);
    } catch (const std::exception& exception) {
        if (mjpeg_streamer_) {
            mjpeg_streamer_->stop();
            mjpeg_streamer_.reset();
        }
        if (impl_) {
            impl_->acceptor.reset();
            impl_->io_context.reset();
        }
        running_.store(false);
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.running = false;
        }
        error = std::string("Failed to start server thread: ") + exception.what();
        return false;
    }

    SIBR_LOG << "RemoteStreamServer listening on "
        << options_.listen_host << ":" << options_.listen_port
        << " (www root: " << www_root_ << ", JPEG backend: " << jpegBackendName() << ")" << std::endl;
    SIBR_WRG << "RemoteStreamServer does not provide auth or TLS in M5. Avoid binding it to the public internet." << std::endl;
    error.clear();
    return true;
}

void RemoteStreamServer::stop()
{
    stop_requested_.store(true);

    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
        for (const auto& session : impl_->stream_sessions) {
            if (!session) {
                continue;
            }
            session->stop_requested.store(true);
            if (session->socket && session->socket->is_open()) {
                closeSocket(*session->socket);
            }
        }
    }

    if (impl_ && impl_->acceptor) {
        beast::error_code ec;
        impl_->acceptor->cancel(ec);
        impl_->acceptor->close(ec);
    }
    if (impl_ && impl_->io_context) {
        impl_->io_context->stop();
    }
    if (mjpeg_streamer_) {
        mjpeg_streamer_->stop();
    }
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    if (impl_) {
        std::vector<std::unique_ptr<StreamClientSession>> sessions_to_join;
        {
            std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
            sessions_to_join.swap(impl_->stream_sessions);
        }
        for (auto& session : sessions_to_join) {
            if (session && session->thread.joinable()) {
                session->thread.join();
            }
        }
        impl_->acceptor.reset();
        impl_->io_context.reset();
    }

    if (mjpeg_streamer_) {
        mjpeg_streamer_.reset();
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.running = false;
        stats_.active_stream_clients = 0;
    }
    running_.store(false);
}

bool RemoteStreamServer::running() const
{
    return running_.load();
}

void RemoteStreamServer::setRendererHealthSnapshot(const RendererHealthSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(renderer_health_mutex_);
    renderer_health_ = snapshot;
}

void RemoteStreamServer::submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index)
{
    if (!running() || !mjpeg_streamer_) {
        return;
    }
    mjpeg_streamer_->captureFrame(render_target, source_frame_index);
}

void RemoteStreamServer::releaseRenderThreadResources()
{
    if (!mjpeg_streamer_) {
        return;
    }
    mjpeg_streamer_->releaseRenderThreadResources();
}

RendererHealthSnapshot RemoteStreamServer::rendererHealthSnapshot() const
{
    std::lock_guard<std::mutex> lock(renderer_health_mutex_);
    return renderer_health_;
}

ServerStats RemoteStreamServer::stats() const
{
    std::lock_guard<std::mutex> lock(stats_mutex_);
    ServerStats snapshot = stats_;
    if (mjpeg_streamer_) {
        const MjpegStreamer::Stats stream_stats = mjpeg_streamer_->stats();
        snapshot.active_stream_clients = stream_stats.active_clients;
        snapshot.stream_frames_captured = stream_stats.raw_frames_captured;
        snapshot.stream_frames_dropped = stream_stats.raw_frames_dropped;
        snapshot.stream_queue_dropped = stream_stats.raw_queue_dropped;
        snapshot.stream_frames_published = stream_stats.jpeg_frames_published;
        snapshot.stream_latest_sequence = stream_stats.latest_sequence;
        snapshot.stream_width = stream_stats.output_width;
        snapshot.stream_height = stream_stats.output_height;
        snapshot.stream_fps = stream_stats.target_fps;
    }
    return snapshot;
}

std::string RemoteStreamServer::resolveWwwRoot(std::string& error) const
{
    std::vector<fs::path> candidates;
    if (!options_.www_root.empty()) {
        candidates.emplace_back(options_.www_root);
    }
    if (const char* env_root = std::getenv("EXTENDED_GAUSSIAN_WWW_ROOT")) {
        if (env_root[0] != '\0') {
            candidates.emplace_back(env_root);
        }
    }

    const fs::path install_directory = fs::path(getInstallDirectory());
    candidates.emplace_back(install_directory / "resources" / "extended_gaussian" / "server" / "www");
    candidates.emplace_back(install_directory / "share" / "extended_gaussian" / "www");
#ifdef SIBR_EXTENDED_GAUSSIAN_REMOTE_WWW_SOURCE_DIR
    candidates.emplace_back(fs::path(SIBR_EXTENDED_GAUSSIAN_REMOTE_WWW_SOURCE_DIR));
#endif

    for (const fs::path& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        std::error_code ec;
        const fs::path canonical = fs::weakly_canonical(candidate, ec);
        const fs::path resolved = ec ? fs::absolute(candidate) : canonical;
        if (fs::is_directory(resolved, ec)) {
            error.clear();
            return resolved.string();
        }
    }

    error = "Failed to locate remote stream www root. Use --www-root <path> or EXTENDED_GAUSSIAN_WWW_ROOT.";
    return {};
}

void RemoteStreamServer::serverThreadMain()
{
    try {
        while (!stop_requested_.load()) {
            if (impl_) {
                std::vector<std::unique_ptr<StreamClientSession>> sessions_to_join;
                {
                    std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
                    auto it = impl_->stream_sessions.begin();
                    while (it != impl_->stream_sessions.end()) {
                        const bool should_join = (*it == nullptr) || (*it)->finished.load();
                        if (!should_join) {
                            ++it;
                            continue;
                        }
                        sessions_to_join.push_back(std::move(*it));
                        it = impl_->stream_sessions.erase(it);
                    }
                }
                for (auto& session : sessions_to_join) {
                    if (session && session->thread.joinable()) {
                        session->thread.join();
                    }
                }
            }

            tcp::socket socket(*impl_->io_context);
            beast::error_code ec;
            impl_->acceptor->accept(socket, ec);

            if (stop_requested_.load()) {
                break;
            }
            if (ec == asio::error::would_block || ec == asio::error::try_again) {
                std::this_thread::sleep_for(kAcceptSleep);
                continue;
            }
            if (ec == asio::error::operation_aborted || ec == asio::error::bad_descriptor) {
                break;
            }
            if (ec) {
                SIBR_WRG << "RemoteStreamServer accept failed: " << ec.message() << std::endl;
                std::this_thread::sleep_for(kAcceptSleep);
                continue;
            }

            beast::flat_buffer buffer;
            http::request<http::string_body> request;
            http::read(socket, buffer, request, ec);
            if (ec) {
                SIBR_WRG << "RemoteStreamServer HTTP read failed: " << ec.message() << std::endl;
                closeSocket(socket);
                continue;
            }

            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                ++stats_.total_http_requests;
            }

            const bool is_head = request.method() == http::verb::head;
            auto write_string_response = [&](http::status status, const std::string& content_type, const std::string& body) {
                beast::error_code write_error;
                if (is_head) {
                    auto response = makeHeadResponse(status, content_type, body.size());
                    http::write(socket, response, write_error);
                } else {
                    auto response = makeTextResponse(status, content_type, body);
                    http::write(socket, response, write_error);
                }
                if (write_error) {
                    SIBR_WRG << "RemoteStreamServer HTTP write failed: " << write_error.message() << std::endl;
                }
            };

            if (request.method() != http::verb::get && !is_head) {
                auto response = makeTextResponse(
                    http::status::method_not_allowed,
                    "text/plain; charset=utf-8",
                    "Method not allowed.\n");
                response.set(http::field::allow, "GET, HEAD");
                http::write(socket, response, ec);
                closeSocket(socket);
                continue;
            }

            const std::string target = stripQueryAndFragment(std::string(request.target()));
            if (target == "/healthz") {
                const auto now = std::chrono::steady_clock::now();
                const double uptime_sec = std::chrono::duration<double>(now - start_time_).count();
                write_string_response(
                    http::status::ok,
                    "application/json; charset=utf-8",
                    healthJson(stats(), rendererHealthSnapshot(), uptime_sec));
                closeSocket(socket);
                continue;
            }

            if (target == "/stream.mjpg") {
                if (is_head) {
                    beast::error_code write_error;
                    http::response<http::empty_body> response{ http::status::ok, 11 };
                    applyCommonHeaders(response);
                    response.set(http::field::content_type, std::string("multipart/x-mixed-replace; boundary=") + kMjpegBoundary);
                    response.content_length(0);
                    http::write(socket, response, write_error);
                    closeSocket(socket);
                    continue;
                }

                auto session = std::make_unique<StreamClientSession>(std::move(socket));
                StreamClientSession* const session_ptr = session.get();
                session->thread = std::thread([this, session_ptr]() {
                    if (!session_ptr || !session_ptr->socket) {
                        return;
                    }

                    if (mjpeg_streamer_) {
                        mjpeg_streamer_->addClient();
                    }

                    beast::error_code write_error;
                    asio::write(*session_ptr->socket, asio::buffer(mjpegStreamHeader()), write_error);
                    if (!write_error) {
                        uint64_t last_sequence = 0;
                        while (!stop_requested_.load() && !session_ptr->stop_requested.load()) {
                            std::shared_ptr<const MjpegStreamer::EncodedFrame> frame;
                            if (!mjpeg_streamer_ || !mjpeg_streamer_->waitForFrameAfter(last_sequence, frame, kStreamWait)) {
                                continue;
                            }
                            if (!frame || !frame->jpeg_bytes || frame->jpeg_bytes->empty()) {
                                continue;
                            }

                            const std::string part_header = mjpegPartHeader(*frame);
                            asio::write(*session_ptr->socket, asio::buffer(part_header), write_error);
                            if (write_error) {
                                break;
                            }
                            asio::write(*session_ptr->socket, asio::buffer(*frame->jpeg_bytes), write_error);
                            if (write_error) {
                                break;
                            }
                            asio::write(*session_ptr->socket, asio::buffer(std::string("\r\n")), write_error);
                            if (write_error) {
                                break;
                            }
                            last_sequence = frame->sequence;
                        }
                    }

                    if (session_ptr->socket && session_ptr->socket->is_open()) {
                        closeSocket(*session_ptr->socket);
                    }
                    if (mjpeg_streamer_) {
                        mjpeg_streamer_->removeClient();
                    }
                    session_ptr->finished.store(true);
                });
                {
                    std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
                    impl_->stream_sessions.push_back(std::move(session));
                }
                continue;
            }

            if (target == "/control") {
                beast::error_code write_error;
                if (is_head) {
                    auto response = makeHeadResponse(http::status::upgrade_required, "application/json; charset=utf-8", 0);
                    response.set(http::field::upgrade, "websocket");
                    http::write(socket, response, write_error);
                } else {
                    auto response = makeTextResponse(
                        http::status::upgrade_required,
                        "application/json; charset=utf-8",
                        "{\"ok\":false,\"error\":\"WebSocket control is not implemented in M5.\"}\n");
                    response.set(http::field::upgrade, "websocket");
                    http::write(socket, response, write_error);
                }
                if (write_error) {
                    SIBR_WRG << "RemoteStreamServer HTTP write failed: " << write_error.message() << std::endl;
                }
                closeSocket(socket);
                continue;
            }

            std::string relative_path;
            if (target == "/" || target == "/index.html") {
                relative_path = "index.html";
            } else if (target == "/app.js") {
                relative_path = "app.js";
            } else if (target == "/styles.css") {
                relative_path = "styles.css";
            } else if (target.rfind("/static/", 0) == 0) {
                relative_path = target.substr(std::string("/static/").size());
            }

            if (relative_path.empty()) {
                write_string_response(http::status::not_found, "text/plain; charset=utf-8", "Not found.\n");
                closeSocket(socket);
                continue;
            }

            std::string decode_error;
            const std::string decoded_path = percentDecode(relative_path, decode_error);
            if (!decode_error.empty() || !isSafeRelativePath(decoded_path)) {
                write_string_response(http::status::bad_request, "text/plain; charset=utf-8", "Invalid static asset path.\n");
                closeSocket(socket);
                continue;
            }

            const fs::path asset_path = fs::path(www_root_) / fs::path(decoded_path);
            std::error_code file_error;
            if (!fs::exists(asset_path, file_error) || !fs::is_regular_file(asset_path, file_error)) {
                write_string_response(http::status::not_found, "text/plain; charset=utf-8", "Static asset not found.\n");
                closeSocket(socket);
                continue;
            }

            std::string file_read_error;
            const std::string content = readFileBytes(asset_path, file_read_error);
            if (!file_read_error.empty()) {
                write_string_response(http::status::internal_server_error, "text/plain; charset=utf-8", file_read_error + "\n");
            } else {
                write_string_response(http::status::ok, mimeTypeForPath(asset_path), content);
            }
            closeSocket(socket);
        }
    } catch (const std::exception& exception) {
        SIBR_WRG << "RemoteStreamServer thread terminated with exception: " << exception.what() << std::endl;
    }

    if (impl_) {
        std::vector<std::unique_ptr<StreamClientSession>> sessions_to_join;
        {
            std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
            sessions_to_join.swap(impl_->stream_sessions);
        }
        for (auto& session : sessions_to_join) {
            if (!session) {
                continue;
            }
            session->stop_requested.store(true);
            if (session->socket && session->socket->is_open()) {
                closeSocket(*session->socket);
            }
            if (session->thread.joinable()) {
                session->thread.join();
            }
        }
    }

    running_.store(false);
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.running = false;
}

} // namespace sibr
