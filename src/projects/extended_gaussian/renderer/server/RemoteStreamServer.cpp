#include "RemoteStreamServer.hpp"

#include <core/system/Utils.hpp>

#include "MjpegStreamer.hpp"
#include <projects/extended_gaussian/renderer/resource/GaussianLoader.hpp>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;
namespace fs = std::filesystem;

namespace {

constexpr const char* kServiceName = "extended_gaussian_remote_stream";
constexpr const char* kVersionName = "m7-integration-verification";
constexpr const char* kMjpegBoundary = "ExGaussBoundary";
constexpr auto kAcceptSleep = std::chrono::milliseconds(25);
constexpr auto kStreamWait = std::chrono::milliseconds(250);

const char* controlQueueModeName()
{
    return "camera_latest_only+management_fifo";
}

const char* loadSourceKindName(sibr::LoadContentSourceKind kind)
{
    switch (kind) {
    case sibr::LoadContentSourceKind::ModelDirectory:
        return "model_dir";
    case sibr::LoadContentSourceKind::Manifest:
        return "manifest";
    }
    return "unknown";
}

struct BrowseEntry {
    std::string name;
    std::string path;
    std::string kind;
    std::string loadable_as;
};

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

struct ControlClientSession {
    std::atomic<bool> stop_requested{ false };
    std::atomic<bool> finished{ false };
    std::mutex socket_mutex;
    tcp::socket* socket = nullptr;
    std::thread thread;
};

uint64_t unixTimeMillisecondsNow()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string timingSummaryJson(const sibr::TimingStatsSummary& summary)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream
        << "{"
        << "\"samples\":" << summary.samples << ","
        << "\"average_ms\":" << summary.average_ms << ","
        << "\"p50_ms\":" << summary.p50_ms << ","
        << "\"p95_ms\":" << summary.p95_ms << ","
        << "\"max_ms\":" << summary.max_ms
        << "}";
    return stream.str();
}

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

constexpr const char* kBrowseRootSentinel = "__ROOT__";

fs::path filesystemRootPath()
{
    const fs::path current_path = fs::current_path();
    const fs::path root_path = current_path.root_path();
    return root_path.empty() ? current_path : root_path;
}

fs::path processHomeDirectoryPath()
{
#ifdef _WIN32
    const char* user_profile = std::getenv("USERPROFILE");
    if (user_profile && *user_profile) {
        return fs::path(user_profile);
    }
    const char* home_drive = std::getenv("HOMEDRIVE");
    const char* home_path = std::getenv("HOMEPATH");
    if (home_drive && *home_drive && home_path && *home_path) {
        return fs::path(std::string(home_drive) + std::string(home_path));
    }
#else
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return fs::path(home);
    }
#endif
    return {};
}

fs::path defaultBrowseStartPath()
{
    const fs::path home_path = processHomeDirectoryPath();
    if (!home_path.empty()) {
        std::error_code error;
        if (fs::exists(home_path, error) && !error && fs::is_directory(home_path, error) && !error) {
            return home_path;
        }
    }
    return filesystemRootPath();
}

std::string queryParameterValue(const std::string& target, const std::string& key, std::string& error)
{
    const size_t query_pos = target.find('?');
    if (query_pos == std::string::npos || query_pos + 1 >= target.size()) {
        return {};
    }

    std::string query = target.substr(query_pos + 1);
    const size_t fragment_pos = query.find('#');
    if (fragment_pos != std::string::npos) {
        query = query.substr(0, fragment_pos);
    }

    std::stringstream stream(query);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        if (pair.empty()) {
            continue;
        }

        const size_t equals_pos = pair.find('=');
        const std::string encoded_key = equals_pos == std::string::npos ? pair : pair.substr(0, equals_pos);
        std::string decoded_key = percentDecode(encoded_key, error);
        if (!error.empty()) {
            return {};
        }
        if (decoded_key != key) {
            continue;
        }

        const std::string encoded_value = equals_pos == std::string::npos ? std::string() : pair.substr(equals_pos + 1);
        std::string decoded_value = percentDecode(encoded_value, error);
        if (!error.empty()) {
            return {};
        }
        return decoded_value;
    }

    return {};
}

bool resolveBrowsePath(const std::string& raw_path, fs::path& canonical_path, std::string& error)
{
    fs::path requested;
    if (raw_path.empty()) {
        requested = defaultBrowseStartPath();
    } else if (raw_path == kBrowseRootSentinel) {
        requested = filesystemRootPath();
    } else {
        requested = fs::path(raw_path);
    }
    if (!requested.is_absolute()) {
        error = "Browse path must be absolute.";
        return false;
    }

    std::error_code fs_error;
    if (!fs::exists(requested, fs_error)) {
        error = "Browse path does not exist: " + requested.string();
        return false;
    }
    if (!fs::is_directory(requested, fs_error)) {
        error = "Browse path is not a directory: " + requested.string();
        return false;
    }

    canonical_path = fs::canonical(requested, fs_error);
    if (fs_error) {
        error = "Failed to resolve browse path '" + requested.string() + "': " + fs_error.message();
        return false;
    }

    return true;
}

std::string browseEntryJson(const BrowseEntry& entry)
{
    std::ostringstream stream;
    stream
        << "{"
        << "\"name\":\"" << jsonEscape(entry.name) << "\","
        << "\"path\":\"" << jsonEscape(entry.path) << "\","
        << "\"kind\":\"" << jsonEscape(entry.kind) << "\","
        << "\"loadable_as\":\"" << jsonEscape(entry.loadable_as) << "\""
        << "}";
    return stream.str();
}

bool isPotentialModelDirectory(const fs::path& directory_path)
{
    std::error_code error;
    if (!fs::is_directory(directory_path, error) || error) {
        return false;
    }

    const bool has_cfg_args = fs::exists(directory_path / "cfg_args", error);
    if (error || !has_cfg_args) {
        return false;
    }

    error.clear();
    if (fs::exists(directory_path / "point_cloud", error) && !error) {
        return true;
    }

    error.clear();
    return fs::exists(directory_path / "point_cloud_blocks", error) && !error;
}

std::string filesystemListJson(const fs::path& current_path, const fs::path& parent_path, const std::vector<BrowseEntry>& entries)
{
    std::ostringstream stream;
    stream
        << "{"
        << "\"ok\":true,"
        << "\"current_path\":\"" << jsonEscape(current_path.string()) << "\","
        << "\"parent_path\":\"" << jsonEscape(parent_path.string()) << "\","
        << "\"entries\":[";
    for (size_t index = 0; index < entries.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << browseEntryJson(entries[index]);
    }
    stream << "]}";
    return stream.str();
}

std::vector<BrowseEntry> listBrowseEntries(const fs::path& current_path)
{
    std::vector<BrowseEntry> entries;
    std::error_code iterator_error;
    for (const auto& entry : fs::directory_iterator(current_path, iterator_error)) {
        if (iterator_error) {
            break;
        }

        const fs::path entry_path = entry.path();
        const std::string name = entry_path.filename().string();
        if (name.empty()) {
            continue;
        }

        std::error_code status_error;
        if (fs::is_directory(entry_path, status_error)) {
            std::string loadable_as = "none";
            if (isPotentialModelDirectory(entry_path)) {
                try {
                    const sibr::GaussianModelDirectoryProbe probe = sibr::GaussianLoader::probeModelDirectory(boost::filesystem::path(entry_path.string()));
                    if (probe.ok) {
                        loadable_as = "model_dir";
                    }
                } catch (const std::exception&) {
                    loadable_as = "none";
                }
            }
            entries.push_back(BrowseEntry{
                name,
                entry_path.string(),
                "directory",
                loadable_as
            });
            continue;
        }

        if (!fs::is_regular_file(entry_path, status_error)) {
            continue;
        }
        if (toLower(entry_path.extension().string()) != ".json") {
            continue;
        }

        entries.push_back(BrowseEntry{
            name,
            entry_path.string(),
            "manifest_file",
            "manifest"
        });
    }

    std::sort(entries.begin(), entries.end(), [](const BrowseEntry& lhs, const BrowseEntry& rhs) {
        if (lhs.kind != rhs.kind) {
            return lhs.kind == "directory";
        }
        return lhs.name < rhs.name;
    });
    return entries;
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

std::string vector3Json(const sibr::Vector3f& value)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream << "[" << value.x() << "," << value.y() << "," << value.z() << "]";
    return stream.str();
}

std::string cameraPoseJson(const sibr::RemoteCameraPose& pose)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream
        << "{"
        << "\"position\":" << vector3Json(pose.position) << ","
        << "\"forward\":" << vector3Json(pose.forward) << ","
        << "\"up\":" << vector3Json(pose.up) << ","
        << "\"fovy\":" << pose.fovy
        << "}";
    return stream.str();
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
        << "    \"encoded_bytes_total\": " << stats.stream_encoded_bytes_total << ",\n"
        << "    \"encoded_bytes_last\": " << stats.stream_encoded_bytes_last << ",\n"
        << "    \"encoded_bytes_average\": " << stats.stream_encoded_bytes_average << ",\n"
        << "    \"width\": " << stats.stream_width << ",\n"
        << "    \"height\": " << stats.stream_height << ",\n"
        << "    \"fps\": " << stats.stream_fps << ",\n"
        << "    \"timing_ms\": {\n"
        << "      \"capture_to_raw_ready\": " << timingSummaryJson(stats.stream_capture_to_raw_ready_ms) << ",\n"
        << "      \"encode\": " << timingSummaryJson(stats.stream_encode_ms) << ",\n"
        << "      \"capture_to_encoded\": " << timingSummaryJson(stats.stream_capture_to_encoded_ms) << "\n"
        << "    }\n"
        << "  },\n"
        << "  \"control\": {\n"
        << "    \"websocket_path\": \"/control\",\n"
        << "    \"queue_mode\": \"" << controlQueueModeName() << "\",\n"
        << "    \"active_clients\": " << stats.active_control_clients << ",\n"
        << "    \"messages_received\": " << stats.control_messages_received << ",\n"
        << "    \"messages_queued\": " << stats.control_messages_queued << ",\n"
        << "    \"messages_rejected\": " << stats.control_messages_rejected << ",\n"
        << "    \"messages_superseded\": " << stats.control_messages_superseded << ",\n"
        << "    \"messages_applied\": " << stats.control_messages_applied << ",\n"
        << "    \"apply_failures\": " << stats.control_apply_failures << ",\n"
        << "    \"latest_received_sequence\": " << stats.control_latest_received_sequence << ",\n"
        << "    \"last_applied_sequence\": " << stats.control_last_applied_sequence << ",\n"
        << "    \"message_pending\": " << (stats.control_message_pending ? "true" : "false") << ",\n"
        << "    \"timing_ms\": {\n"
        << "      \"receive_to_apply\": " << timingSummaryJson(stats.control_receive_to_apply_ms) << "\n"
        << "    }\n"
        << "  },\n"
        << "  \"renderer\": {\n"
        << "    \"initialized\": " << (renderer.initialized ? "true" : "false") << ",\n"
        << "    \"has_manifest\": " << (renderer.has_manifest ? "true" : "false") << ",\n"
        << "    \"frame_index\": " << renderer.frame_index << ",\n"
        << "    \"app_time_sec\": " << renderer.app_time_sec << ",\n"
        << "    \"camera_pose_available\": " << (renderer.has_camera_pose ? "true" : "false");
    if (renderer.has_camera_pose) {
        stream << ",\n    \"camera_pose\": " << cameraPoseJson(renderer.camera_pose);
    }
    stream << ",\n    \"current_phase\": \"" << jsonEscape(renderer.current_phase) << "\"";
    stream << ",\n    \"available_phases\": [";
    for (size_t index = 0; index < renderer.available_phases.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << "\"" << jsonEscape(renderer.available_phases[index]) << "\"";
    }
    stream << "]";
    stream << ",\n    \"total_assets\": " << renderer.total_asset_count;
    stream << ",\n    \"content_loaded\": " << (renderer.content_loaded ? "true" : "false");
    stream << ",\n    \"loaded_source_kind\": \"" << jsonEscape(renderer.loaded_source_kind) << "\"";
    stream << ",\n    \"loaded_source_path\": \"" << jsonEscape(renderer.loaded_source_path) << "\"";
    stream << ",\n    \"load_state\": \"" << jsonEscape(renderer.load_state) << "\"";
    stream << ",\n    \"last_load_error\": \"" << jsonEscape(renderer.last_load_error) << "\"";
    stream << ",\n    \"last_load_sequence\": " << renderer.last_load_sequence;
    stream << ",\n    \"streaming\": {"
           << "\"required_gpu\":" << renderer.required_gpu_count
           << ",\"warm_cpu\":" << renderer.warm_cpu_count
           << ",\"pending_disk_loads\":" << renderer.pending_disk_loads
           << ",\"pending_gpu_uploads\":" << renderer.pending_gpu_uploads
           << ",\"pending_gpu_evictions\":" << renderer.pending_gpu_evictions
           << ",\"cpu_resident_bytes\":" << renderer.cpu_resident_bytes
           << ",\"gpu_resident_bytes\":" << renderer.gpu_resident_bytes
           << ",\"skipped_instances\":" << renderer.skipped_instances_last_frame
           << ",\"swap_hits\":" << renderer.swap_hits
           << ",\"swap_misses\":" << renderer.swap_misses
           << "}";
    stream
        << "\n  }\n"
        << "}";
    return stream.str();
}

std::string controlReadyJson(const sibr::RendererHealthSnapshot& renderer)
{
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(6);
    stream
        << "{"
        << "\"ok\":true,"
        << "\"type\":\"ready\","
        << "\"service\":\"" << kServiceName << "\","
        << "\"version\":\"" << kVersionName << "\","
        << "\"queue_mode\":\"" << controlQueueModeName() << "\","
        << "\"control_path\":\"/control\","
        << "\"camera_pose_available\":" << (renderer.has_camera_pose ? "true" : "false");
    if (renderer.has_camera_pose) {
        stream << ",\"camera_pose\":" << cameraPoseJson(renderer.camera_pose);
    }
    stream
        << ",\"frame_index\":" << renderer.frame_index
        << ",\"app_time_sec\":" << renderer.app_time_sec
        << ",\"has_manifest\":" << (renderer.has_manifest ? "true" : "false")
        << ",\"current_phase\":\"" << jsonEscape(renderer.current_phase) << "\""
        << ",\"available_phases\":[";
    for (size_t index = 0; index < renderer.available_phases.size(); ++index) {
        if (index > 0) {
            stream << ",";
        }
        stream << "\"" << jsonEscape(renderer.available_phases[index]) << "\"";
    }
    stream
        << "]"
        << ",\"total_assets\":" << renderer.total_asset_count
        << ",\"content_loaded\":" << (renderer.content_loaded ? "true" : "false")
        << ",\"loaded_source_kind\":\"" << jsonEscape(renderer.loaded_source_kind) << "\""
        << ",\"loaded_source_path\":\"" << jsonEscape(renderer.loaded_source_path) << "\""
        << ",\"load_state\":\"" << jsonEscape(renderer.load_state) << "\""
        << ",\"last_load_error\":\"" << jsonEscape(renderer.last_load_error) << "\""
        << ",\"last_load_sequence\":" << renderer.last_load_sequence
        << "}";
    return stream.str();
}

std::string controlAckJson(uint64_t sequence, bool superseded_previous, const char* request_type)
{
    std::ostringstream stream;
    const uint64_t server_ack_unix_ms = unixTimeMillisecondsNow();
    stream
        << "{"
        << "\"ok\":true,"
        << "\"type\":\"ack\","
        << "\"request_type\":\"" << request_type << "\","
        << "\"status\":\"queued\","
        << "\"queue_mode\":\"" << controlQueueModeName() << "\","
        << "\"sequence\":" << sequence << ","
        << "\"superseded_previous\":" << (superseded_previous ? "true" : "false") << ","
        << "\"server_ack_unix_ms\":" << server_ack_unix_ms
        << "}";
    return stream.str();
}

std::string controlErrorJson(const std::string& error)
{
    std::ostringstream stream;
    stream
        << "{"
        << "\"ok\":false,"
        << "\"type\":\"error\","
        << "\"error\":\"" << jsonEscape(error) << "\""
        << "}";
    return stream.str();
}

std::string apiErrorJson(const std::string& error)
{
    std::ostringstream stream;
    stream
        << "{"
        << "\"ok\":false,"
        << "\"error\":\"" << jsonEscape(error) << "\""
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
    stream.setf(std::ios::fixed);
    stream.precision(3);
    stream
        << "--" << kMjpegBoundary << "\r\n"
        << "Content-Type: image/jpeg\r\n"
        << "Content-Length: " << content_length << "\r\n"
        << "X-Sequence: " << frame.sequence << "\r\n"
        << "X-Source-Frame-Index: " << frame.source_frame_index << "\r\n"
        << "X-Control-Sequence: " << frame.control_sequence << "\r\n"
        << "X-Width: " << frame.width << "\r\n"
        << "X-Height: " << frame.height << "\r\n"
        << "X-Capture-To-Raw-Ready-Ms: " << frame.capture_to_raw_ready_ms << "\r\n"
        << "X-Encode-Ms: " << frame.encode_ms << "\r\n"
        << "X-Capture-To-Encoded-Ms: " << frame.capture_to_encoded_ms << "\r\n"
        << "X-Encoded-Unix-Ms: " << frame.encoded_unix_time_ms << "\r\n\r\n";
    return stream.str();
}

bool writeWebSocketText(websocket::stream<tcp::socket>& stream, const std::string& payload, beast::error_code& ec)
{
    stream.text(true);
    stream.write(asio::buffer(payload), ec);
    return !ec;
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
    std::mutex control_sessions_mutex;
    std::vector<std::unique_ptr<ControlClientSession>> control_sessions;
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
        std::lock_guard<std::mutex> lock(control_message_mutex_);
        pending_camera_control_message_.reset();
        pending_management_control_messages_.clear();
        next_control_sequence_ = 1;
    }

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
        stats_.stream_encoded_bytes_total = 0;
        stats_.stream_encoded_bytes_last = 0;
        stats_.stream_encoded_bytes_average = 0.0;
        stats_.stream_capture_to_raw_ready_ms = {};
        stats_.stream_encode_ms = {};
        stats_.stream_capture_to_encoded_ms = {};
        stats_.active_control_clients = 0;
        stats_.control_messages_received = 0;
        stats_.control_messages_queued = 0;
        stats_.control_messages_rejected = 0;
        stats_.control_messages_superseded = 0;
        stats_.control_messages_applied = 0;
        stats_.control_apply_failures = 0;
        stats_.control_latest_received_sequence = 0;
        stats_.control_last_applied_sequence = 0;
        stats_.control_message_pending = false;
        stats_.control_receive_to_apply_ms = {};
    }

    try {
        server_thread_ = std::thread(&RemoteStreamServer::serverThreadMain, this);
    } catch (const std::exception& exception) {
        if (mjpeg_streamer_) {
            mjpeg_streamer_->stop();
            mjpeg_streamer_.reset();
        }
        {
            std::lock_guard<std::mutex> lock(control_message_mutex_);
            pending_camera_control_message_.reset();
            pending_management_control_messages_.clear();
        }
        if (impl_) {
            impl_->acceptor.reset();
            impl_->io_context.reset();
        }
        running_.store(false);
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.running = false;
            stats_.active_stream_clients = 0;
            stats_.active_control_clients = 0;
            stats_.control_message_pending = false;
        }
        error = std::string("Failed to start server thread: ") + exception.what();
        return false;
    }

    SIBR_LOG << "RemoteStreamServer listening on "
             << options_.listen_host << ":" << options_.listen_port
             << " (www root: " << www_root_ << ", JPEG backend: " << jpegBackendName() << ")" << std::endl;
    SIBR_WRG << "RemoteStreamServer does not provide auth or TLS in M6. Avoid binding it to the public internet."
             << std::endl;
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

    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->control_sessions_mutex);
        for (const auto& session : impl_->control_sessions) {
            if (!session) {
                continue;
            }
            session->stop_requested.store(true);
            std::lock_guard<std::mutex> socket_lock(session->socket_mutex);
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
        std::vector<std::unique_ptr<StreamClientSession>> stream_sessions_to_join;
        {
            std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
            stream_sessions_to_join.swap(impl_->stream_sessions);
        }
        for (auto& session : stream_sessions_to_join) {
            if (session && session->thread.joinable()) {
                session->thread.join();
            }
        }

        std::vector<std::unique_ptr<ControlClientSession>> control_sessions_to_join;
        {
            std::lock_guard<std::mutex> lock(impl_->control_sessions_mutex);
            control_sessions_to_join.swap(impl_->control_sessions);
        }
        for (auto& session : control_sessions_to_join) {
            if (!session) {
                continue;
            }
            session->stop_requested.store(true);
            {
                std::lock_guard<std::mutex> socket_lock(session->socket_mutex);
                if (session->socket && session->socket->is_open()) {
                    closeSocket(*session->socket);
                }
            }
            if (session->thread.joinable()) {
                session->thread.join();
            }
        }

        impl_->acceptor.reset();
        impl_->io_context.reset();
    }

    {
        std::lock_guard<std::mutex> lock(control_message_mutex_);
        pending_camera_control_message_.reset();
        pending_management_control_messages_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(control_latency_mutex_);
        control_receive_to_apply_samples_ms_.clear();
    }

    if (mjpeg_streamer_) {
        mjpeg_streamer_.reset();
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.running = false;
        stats_.active_stream_clients = 0;
        stats_.active_control_clients = 0;
        stats_.control_message_pending = false;
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

void RemoteStreamServer::submitRenderedFrame(const IRenderTarget& render_target, uint64_t source_frame_index, uint64_t control_sequence)
{
    if (!running() || !mjpeg_streamer_) {
        return;
    }
    mjpeg_streamer_->captureFrame(render_target, source_frame_index, control_sequence);
}

void RemoteStreamServer::releaseRenderThreadResources()
{
    if (!mjpeg_streamer_) {
        return;
    }
    mjpeg_streamer_->releaseRenderThreadResources();
}

bool RemoteStreamServer::consumePendingControlMessage(
    ControlMessage& message,
    uint64_t& sequence,
    std::chrono::steady_clock::time_point& received_at)
{
    bool has_message = false;
    bool has_more_messages = false;
    {
        std::lock_guard<std::mutex> lock(control_message_mutex_);
        if (!pending_management_control_messages_.empty()) {
            const PendingControlMessage pending = pending_management_control_messages_.front();
            pending_management_control_messages_.pop_front();
            sequence = pending.sequence;
            received_at = pending.received_time;
            message = pending.message;
            has_message = true;
        } else if (pending_camera_control_message_) {
            sequence = pending_camera_control_message_->sequence;
            received_at = pending_camera_control_message_->received_time;
            message = pending_camera_control_message_->message;
            pending_camera_control_message_.reset();
            has_message = true;
        }
        has_more_messages = pending_camera_control_message_.has_value() || !pending_management_control_messages_.empty();
    }
    if (has_message) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.control_message_pending = has_more_messages;
    }
    return has_message;
}

void RemoteStreamServer::pushControlLatencySample(double value_ms)
{
    if (value_ms < 0.0) {
        return;
    }
    std::lock_guard<std::mutex> lock(control_latency_mutex_);
    constexpr size_t kLatencyHistoryLimit = 4096;
    if (control_receive_to_apply_samples_ms_.size() >= kLatencyHistoryLimit) {
        control_receive_to_apply_samples_ms_.erase(
            control_receive_to_apply_samples_ms_.begin(),
            control_receive_to_apply_samples_ms_.begin() + static_cast<std::ptrdiff_t>(control_receive_to_apply_samples_ms_.size() - kLatencyHistoryLimit + 1));
    }
    control_receive_to_apply_samples_ms_.push_back(value_ms);
}

TimingStatsSummary RemoteStreamServer::summarizeControlLatencySamples() const
{
    TimingStatsSummary summary;
    std::lock_guard<std::mutex> lock(control_latency_mutex_);
    summary.samples = control_receive_to_apply_samples_ms_.size();
    if (control_receive_to_apply_samples_ms_.empty()) {
        return summary;
    }

    summary.average_ms = std::accumulate(control_receive_to_apply_samples_ms_.begin(), control_receive_to_apply_samples_ms_.end(), 0.0) / static_cast<double>(control_receive_to_apply_samples_ms_.size());
    summary.max_ms = *std::max_element(control_receive_to_apply_samples_ms_.begin(), control_receive_to_apply_samples_ms_.end());
    std::vector<double> sorted = control_receive_to_apply_samples_ms_;
    std::sort(sorted.begin(), sorted.end());
    const size_t p50_index = sorted.size() / 2;
    const size_t p95_index = std::min(sorted.size() - 1, static_cast<size_t>(std::ceil(static_cast<double>(sorted.size()) * 0.95)) - 1);
    summary.p50_ms = sorted[p50_index];
    summary.p95_ms = sorted[p95_index];
    return summary;
}

void RemoteStreamServer::recordControlMessageApplied(uint64_t sequence, bool applied, double receive_to_apply_ms)
{
    if (applied) {
        pushControlLatencySample(receive_to_apply_ms);
    }

    TimingStatsSummary latency_summary = summarizeControlLatencySamples();

    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (applied) {
        ++stats_.control_messages_applied;
        stats_.control_last_applied_sequence = sequence;
    } else {
        ++stats_.control_apply_failures;
    }
    stats_.control_receive_to_apply_ms = latency_summary;
}

bool RemoteStreamServer::enqueueControlMessage(
    const ControlMessage& message,
    uint64_t& sequence,
    bool& superseded_previous,
    std::string& error)
{
    if (!running()) {
        error = "Remote stream server is not running.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(control_message_mutex_);
        PendingControlMessage pending;
        pending.sequence = next_control_sequence_++;
        pending.received_time = std::chrono::steady_clock::now();
        pending.message = message;
        sequence = pending.sequence;

        if (message.type == ControlMessageType::SetCameraPose) {
            superseded_previous = pending_camera_control_message_.has_value();
            pending_camera_control_message_ = pending;
        } else {
            superseded_previous = false;
            pending_management_control_messages_.push_back(pending);
        }
    }

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        ++stats_.control_messages_queued;
        if (superseded_previous) {
            ++stats_.control_messages_superseded;
        }
        stats_.control_latest_received_sequence = sequence;
        stats_.control_message_pending = true;
    }

    error.clear();
    return true;
}

RendererHealthSnapshot RemoteStreamServer::rendererHealthSnapshot() const
{
    std::lock_guard<std::mutex> lock(renderer_health_mutex_);
    return renderer_health_;
}

ServerStats RemoteStreamServer::stats() const
{
    ServerStats snapshot;
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        snapshot = stats_;
    }
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
        snapshot.stream_encoded_bytes_total = stream_stats.encoded_bytes_total;
        snapshot.stream_encoded_bytes_last = stream_stats.encoded_bytes_last;
        snapshot.stream_encoded_bytes_average = stream_stats.encoded_bytes_average;
        snapshot.stream_capture_to_raw_ready_ms = {stream_stats.capture_to_raw_ready_ms.samples, stream_stats.capture_to_raw_ready_ms.average_ms, stream_stats.capture_to_raw_ready_ms.p50_ms, stream_stats.capture_to_raw_ready_ms.p95_ms, stream_stats.capture_to_raw_ready_ms.max_ms};
        snapshot.stream_encode_ms = {stream_stats.encode_ms.samples, stream_stats.encode_ms.average_ms, stream_stats.encode_ms.p50_ms, stream_stats.encode_ms.p95_ms, stream_stats.encode_ms.max_ms};
        snapshot.stream_capture_to_encoded_ms = {stream_stats.capture_to_encoded_ms.samples, stream_stats.capture_to_encoded_ms.average_ms, stream_stats.capture_to_encoded_ms.p50_ms, stream_stats.capture_to_encoded_ms.p95_ms, stream_stats.capture_to_encoded_ms.max_ms};
    }
    snapshot.control_receive_to_apply_ms = summarizeControlLatencySamples();
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
                std::vector<std::unique_ptr<StreamClientSession>> stream_sessions_to_join;
                {
                    std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
                    auto it = impl_->stream_sessions.begin();
                    while (it != impl_->stream_sessions.end()) {
                        const bool should_join = (*it == nullptr) || (*it)->finished.load();
                        if (!should_join) {
                            ++it;
                            continue;
                        }
                        stream_sessions_to_join.push_back(std::move(*it));
                        it = impl_->stream_sessions.erase(it);
                    }
                }
                for (auto& session : stream_sessions_to_join) {
                    if (session && session->thread.joinable()) {
                        session->thread.join();
                    }
                }

                std::vector<std::unique_ptr<ControlClientSession>> control_sessions_to_join;
                {
                    std::lock_guard<std::mutex> lock(impl_->control_sessions_mutex);
                    auto it = impl_->control_sessions.begin();
                    while (it != impl_->control_sessions.end()) {
                        const bool should_join = (*it == nullptr) || (*it)->finished.load();
                        if (!should_join) {
                            ++it;
                            continue;
                        }
                        control_sessions_to_join.push_back(std::move(*it));
                        it = impl_->control_sessions.erase(it);
                    }
                }
                for (auto& session : control_sessions_to_join) {
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
            auto write_string_response =
                [&](http::status status, const std::string& content_type, const std::string& body) {
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

            const std::string raw_target = std::string(request.target());
            const std::string target = stripQueryAndFragment(raw_target);
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

            if (target == "/api/fs/list") {
                std::string query_error;
                const std::string requested_path = queryParameterValue(raw_target, "path", query_error);
                if (!query_error.empty()) {
                    write_string_response(
                        http::status::bad_request,
                        "application/json; charset=utf-8",
                        apiErrorJson(query_error));
                    closeSocket(socket);
                    continue;
                }

                fs::path current_path;
                std::string resolve_error;
                if (!resolveBrowsePath(requested_path, current_path, resolve_error)) {
                    write_string_response(
                        http::status::bad_request,
                        "application/json; charset=utf-8",
                        apiErrorJson(resolve_error));
                    closeSocket(socket);
                    continue;
                }

                std::error_code parent_error;
                fs::path parent_path = current_path.parent_path();
                if (parent_path.empty() || !fs::exists(parent_path, parent_error)) {
                    parent_path = current_path;
                }

                const std::vector<BrowseEntry> entries = listBrowseEntries(current_path);
                write_string_response(
                    http::status::ok,
                    "application/json; charset=utf-8",
                    filesystemListJson(current_path, parent_path, entries));
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
                if (!websocket::is_upgrade(request)) {
                    beast::error_code write_error;
                    if (is_head) {
                        auto response = makeHeadResponse(http::status::upgrade_required, "application/json; charset=utf-8", 0);
                        response.set(http::field::upgrade, "websocket");
                        http::write(socket, response, write_error);
                    } else {
                        auto response = makeTextResponse(
                            http::status::upgrade_required,
                            "application/json; charset=utf-8",
                            controlErrorJson("WebSocket upgrade required for /control."));
                        response.set(http::field::upgrade, "websocket");
                        http::write(socket, response, write_error);
                    }
                    if (write_error) {
                        SIBR_WRG << "RemoteStreamServer HTTP write failed: " << write_error.message() << std::endl;
                    }
                    closeSocket(socket);
                    continue;
                }

                auto session = std::make_unique<ControlClientSession>();
                ControlClientSession* const session_ptr = session.get();
                session->thread = std::thread(
                    [this,
                     session_ptr,
                     accepted_socket = std::move(socket),
                     accepted_request = std::move(request)]() mutable {
                        websocket::stream<tcp::socket> ws(std::move(accepted_socket));
                        ws.set_option(websocket::stream_base::decorator([](websocket::response_type& response) {
                            response.set(http::field::server, "extended_gaussian_server");
                        }));
                        {
                            std::lock_guard<std::mutex> socket_lock(session_ptr->socket_mutex);
                            session_ptr->socket = &ws.next_layer();
                        }

                        bool active_registered = false;
                        auto finish = [&]() {
                            {
                                std::lock_guard<std::mutex> socket_lock(session_ptr->socket_mutex);
                                session_ptr->socket = nullptr;
                            }
                            if (active_registered) {
                                std::lock_guard<std::mutex> lock(stats_mutex_);
                                if (stats_.active_control_clients > 0) {
                                    --stats_.active_control_clients;
                                }
                            }
                            session_ptr->finished.store(true);
                        };

                        beast::error_code ws_error;
                        ws.accept(accepted_request, ws_error);
                        if (ws_error) {
                            SIBR_WRG << "RemoteStreamServer WebSocket accept failed: " << ws_error.message() << std::endl;
                            finish();
                            return;
                        }

                        {
                            std::lock_guard<std::mutex> lock(stats_mutex_);
                            ++stats_.active_control_clients;
                        }
                        active_registered = true;

                        if (!writeWebSocketText(ws, controlReadyJson(rendererHealthSnapshot()), ws_error)) {
                            if (ws_error != websocket::error::closed &&
                                ws_error != asio::error::operation_aborted &&
                                ws_error != asio::error::bad_descriptor &&
                                ws_error != asio::error::eof) {
                                SIBR_WRG << "RemoteStreamServer WebSocket ready write failed: " << ws_error.message() << std::endl;
                            }
                            beast::error_code close_error;
                            ws.close(websocket::close_code::normal, close_error);
                            finish();
                            return;
                        }

                        while (!stop_requested_.load() && !session_ptr->stop_requested.load()) {
                            beast::flat_buffer ws_buffer;
                            ws_error.clear();
                            ws.read(ws_buffer, ws_error);
                            if (ws_error == websocket::error::closed || ws_error == asio::error::operation_aborted ||
                                ws_error == asio::error::bad_descriptor || ws_error == asio::error::eof) {
                                break;
                            }
                            if (ws_error) {
                                SIBR_WRG << "RemoteStreamServer WebSocket read failed: " << ws_error.message() << std::endl;
                                break;
                            }

                            {
                                std::lock_guard<std::mutex> lock(stats_mutex_);
                                ++stats_.control_messages_received;
                            }

                            std::string response_payload;
                            if (ws.got_binary()) {
                                {
                                    std::lock_guard<std::mutex> lock(stats_mutex_);
                                    ++stats_.control_messages_rejected;
                                }
                                response_payload = controlErrorJson("Binary WebSocket messages are not supported.");
                                if (!writeWebSocketText(ws, response_payload, ws_error)) {
                                    break;
                                }
                                continue;
                            }

                            const std::string payload = beast::buffers_to_string(ws_buffer.data());
                            const ParseControlMessageResult parsed = ParseControlMessageJson(payload);
                            if (!parsed) {
                                {
                                    std::lock_guard<std::mutex> lock(stats_mutex_);
                                    ++stats_.control_messages_rejected;
                                }
                                response_payload = controlErrorJson(parsed.error);
                                if (!writeWebSocketText(ws, response_payload, ws_error)) {
                                    break;
                                }
                                continue;
                            }

                            uint64_t sequence = 0;
                            bool superseded_previous = false;
                            std::string enqueue_error;
                            if (!enqueueControlMessage(parsed.message, sequence, superseded_previous, enqueue_error)) {
                                {
                                    std::lock_guard<std::mutex> lock(stats_mutex_);
                                    ++stats_.control_messages_rejected;
                                }
                                response_payload = controlErrorJson(enqueue_error);
                                if (!writeWebSocketText(ws, response_payload, ws_error)) {
                                    break;
                                }
                                continue;
                            }

                            const char* request_type = "set_camera_pose";
                            if (parsed.message.type == ControlMessageType::SetPhase) {
                                request_type = "set_phase";
                            } else if (parsed.message.type == ControlMessageType::LoadContent) {
                                request_type = "load_content";
                            }
                            response_payload = controlAckJson(sequence, superseded_previous, request_type);
                            if (!writeWebSocketText(ws, response_payload, ws_error)) {
                                break;
                            }
                        }

                        beast::error_code close_error;
                        ws.close(websocket::close_code::normal, close_error);
                        finish();
                    });
                {
                    std::lock_guard<std::mutex> lock(impl_->control_sessions_mutex);
                    impl_->control_sessions.push_back(std::move(session));
                }
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
        std::vector<std::unique_ptr<StreamClientSession>> stream_sessions_to_join;
        {
            std::lock_guard<std::mutex> lock(impl_->stream_sessions_mutex);
            stream_sessions_to_join.swap(impl_->stream_sessions);
        }
        for (auto& session : stream_sessions_to_join) {
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

        std::vector<std::unique_ptr<ControlClientSession>> control_sessions_to_join;
        {
            std::lock_guard<std::mutex> lock(impl_->control_sessions_mutex);
            control_sessions_to_join.swap(impl_->control_sessions);
        }
        for (auto& session : control_sessions_to_join) {
            if (!session) {
                continue;
            }
            session->stop_requested.store(true);
            if (session->thread.joinable()) {
                session->thread.join();
            }
        }
    }

    running_.store(false);
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.running = false;
    stats_.active_stream_clients = 0;
    stats_.active_control_clients = 0;
    stats_.control_message_pending = false;
}

} // namespace sibr
