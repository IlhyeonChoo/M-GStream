#include "GaussianLoader.hpp"

#include <fstream>
#include <regex>

namespace fs = boost::filesystem;

namespace {

std::string findLargestNumberedSubdirectory(const fs::path& directory_path)
{
	if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
		return {};
	}

	std::regex regex_pattern(R"_(iteration_(\d+))_");
	std::string largest_subdirectory;
	int largest_number = -1;

	for (const auto& entry : fs::directory_iterator(directory_path)) {
		if (!fs::is_directory(entry)) {
			continue;
		}

		const std::string subdirectory = entry.path().filename().string();
		std::smatch match;
		if (!std::regex_match(subdirectory, match, regex_pattern)) {
			continue;
		}

		const int number = std::stoi(match[1]);
		if (number > largest_number) {
			largest_number = number;
			largest_subdirectory = subdirectory;
		}
	}

	return largest_subdirectory;
}

fs::path findPointCloudRoot(const fs::path& model_path)
{
	const fs::path standard_root = model_path / "point_cloud";
	if (fs::exists(standard_root) && fs::is_directory(standard_root)) {
		return standard_root;
	}

	const fs::path block_root = model_path / "point_cloud_blocks";
	if (fs::exists(block_root) && fs::is_directory(block_root)) {
		const fs::path preferred_scale = block_root / "scale_1.0";
		if (fs::exists(preferred_scale) && fs::is_directory(preferred_scale)) {
			return preferred_scale;
		}

		for (const auto& entry : fs::directory_iterator(block_root)) {
			if (fs::is_directory(entry)) {
				return entry.path();
			}
		}
	}

	return {};
}

std::pair<int, int> findArg(const std::string& line, const std::string& name)
{
	int start = line.find(name, 0);
	start = line.find("=", start);
	start += 1;
	int end = line.find_first_of(",)", start);
	return std::make_pair(start, end);
}

} // namespace

namespace sibr {
	GaussianModelDirectoryProbe GaussianLoader::probeModelDirectory(const fs::path& model_path)
	{
		GaussianModelDirectoryProbe probe;
		try {
			if (model_path.empty()) {
				probe.error = "Model directory path is empty.";
				return probe;
			}

			fs::path resolved_model_path = model_path;
			if (!fs::exists(resolved_model_path)) {
				probe.error = "Model directory does not exist: " + model_path.string();
				return probe;
			}
			if (!fs::is_directory(resolved_model_path)) {
				probe.error = "Model path is not a directory: " + model_path.string();
				return probe;
			}

			resolved_model_path = fs::canonical(resolved_model_path);

			const fs::path cfg_args_path = resolved_model_path / "cfg_args";
			if (!fs::exists(cfg_args_path) || !fs::is_regular_file(cfg_args_path)) {
				probe.error = "Could not find config file 'cfg_args' at " + resolved_model_path.string();
				return probe;
			}

			const fs::path point_cloud_root = findPointCloudRoot(resolved_model_path);
			if (point_cloud_root.empty()) {
				probe.error = "Could not find point cloud directory at " + resolved_model_path.string()
					+ " (expected 'point_cloud' or 'point_cloud_blocks/scale_*').";
				return probe;
			}

			const std::string latest_folder = findLargestNumberedSubdirectory(point_cloud_root);
			if (latest_folder.empty()) {
				probe.error = "Could not find iteration folder in " + point_cloud_root.string();
				return probe;
			}

			const fs::path latest_iteration_dir = point_cloud_root / latest_folder;
			const fs::path point_cloud_ply = latest_iteration_dir / "point_cloud.ply";
			if (!fs::exists(point_cloud_ply) || !fs::is_regular_file(point_cloud_ply)) {
				probe.error = "Could not find model's PLY file at " + point_cloud_ply.string();
				return probe;
			}

			probe.ok = true;
			probe.canonical_model_dir = resolved_model_path;
			probe.point_cloud_root = point_cloud_root;
			probe.latest_iteration_dir = latest_iteration_dir;
			probe.point_cloud_ply = point_cloud_ply;
			return probe;
		} catch (const fs::filesystem_error& error) {
			probe.error = "Failed to inspect model directory '" + model_path.string() + "': " + error.what();
			return probe;
		}
	}

	GaussianField::UPtr GaussianLoader::load(const std::string& modelPath) {
		auto field = std::make_unique<GaussianField>();
		field->path = modelPath;

		const GaussianModelDirectoryProbe probe = probeModelDirectory(fs::path(modelPath));
		if (!probe.ok) {
			SIBR_ERR << probe.error;
			return nullptr;
		}

		field->path = probe.canonical_model_dir.string();
		std::string folderName = probe.canonical_model_dir.filename().string();
		if (folderName.empty()) {
			folderName = probe.canonical_model_dir.parent_path().filename().string();
		}
		field->name = folderName;

		std::ifstream cfgFile((probe.canonical_model_dir / "cfg_args").string());
		if (!cfgFile.good()) {
			SIBR_ERR << "Could not find config file 'cfg_args' at " << probe.canonical_model_dir.string();
			return nullptr;
		}

		std::string cfgLine;
		std::getline(cfgFile, cfgLine);
		auto shRng = findArg(cfgLine, "sh_degree");
		int runtime_sh_degree = std::stoi(cfgLine.substr(shRng.first, shRng.second - shRng.first));
		field->sh_degree = runtime_sh_degree;

		const std::string finalPlyPath = probe.point_cloud_ply.string();

		bool success = false;
		switch (runtime_sh_degree) {
		case 0: success = loadPly<0>(finalPlyPath.c_str(), *field); break;
		case 1: success = loadPly<1>(finalPlyPath.c_str(), *field); break;
		case 2: success = loadPly<2>(finalPlyPath.c_str(), *field); break;
		case 3: success = loadPly<3>(finalPlyPath.c_str(), *field); break;
		default:
			SIBR_ERR << "Unsupported SH degree: " << runtime_sh_degree;
			return nullptr;
		}

		if (!success) return nullptr;

		return field;
	}	
}
