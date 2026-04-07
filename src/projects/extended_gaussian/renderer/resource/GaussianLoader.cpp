#include "GaussianLoader.hpp"

#include <fstream>
#include <regex>

namespace fs = boost::filesystem;

std::string findLargestNumberedSubdirectory(const std::string& directoryPath) {
	fs::path dirPath(directoryPath);
	if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
		std::cerr << "Invalid directory: " << directoryPath << std::endl;
		return "";
	}

	std::regex regexPattern(R"_(iteration_(\d+))_");
	std::string largestSubdirectory;
	int largestNumber = -1;

	for (const auto& entry : fs::directory_iterator(dirPath)) {
		if (fs::is_directory(entry)) {
			std::string subdirectory = entry.path().filename().string();
			std::smatch match;

			if (std::regex_match(subdirectory, match, regexPattern)) {
				int number = std::stoi(match[1]);

				if (number > largestNumber) {
					largestNumber = number;
					largestSubdirectory = subdirectory;
				}
			}
		}
	}

	return largestSubdirectory;
}

std::pair<int, int> findArg(const std::string& line, const std::string& name)
{
	int start = line.find(name, 0);
	start = line.find("=", start);
	start += 1;
	int end = line.find_first_of(",)", start);
	return std::make_pair(start, end);
}

namespace sibr {
	GaussianField::UPtr GaussianLoader::load(const std::string& modelPath) {
		auto field = std::make_unique<GaussianField>();
		field->path = modelPath;

		fs::path p(modelPath);

		std::string folderName = p.filename().string();
		if (folderName.empty()) {
			folderName = p.parent_path().filename().string();
		}

		field->name = folderName;

		// 1. 경로 처리
		std::string pathWithSlash = modelPath;
		if (pathWithSlash.back() != '/' && pathWithSlash.back() != '\\') {
			pathWithSlash += "/";
		}

		// 2. cfg_args 파싱
		std::ifstream cfgFile(pathWithSlash + "cfg_args");
		if (!cfgFile.good()) {
			SIBR_ERR << "Could not find config file 'cfg_args' at " << modelPath;
			return nullptr;
		}

		std::string cfgLine;
		std::getline(cfgFile, cfgLine);
		auto shRng = findArg(cfgLine, "sh_degree");
		int runtime_sh_degree = std::stoi(cfgLine.substr(shRng.first, shRng.second - shRng.first));
		field->sh_degree = runtime_sh_degree; // field에 저장

		// 3. 최신 iteration 폴더 찾기
		std::string plyRoot = pathWithSlash + "point_cloud";
		std::string latestFolder = findLargestNumberedSubdirectory(plyRoot);
		if (latestFolder.empty()) {
			SIBR_ERR << "Could not find iteration folder in " << plyRoot;
			return nullptr;
		}

		// 4. 최종 PLY 경로 완성
		std::string finalPlyPath = plyRoot + "/" + latestFolder + "/point_cloud.ply";

		// 5. [핵심 수정] 런타임 변수를 컴파일 타임 상수로 매핑
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
