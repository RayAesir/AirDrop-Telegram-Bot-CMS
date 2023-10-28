#include "files.h"

// deps
#include <spdlog/spdlog.h>
// global
#include <fstream>

namespace files {

void CreateParentFolders(const fs::path& path) {
  const fs::path dir_path = path.parent_path();
  if (!fs::exists(dir_path)) {
    fs::create_directories(dir_path);
    spdlog::info("{}: '{}' create directory for '{}'", __FUNCTION__,
                 dir_path.string(), path.filename().string());
  }
}

void WriteFile(const fs::path& path, const std::string& data) {
  // file already exist
  if (fs::exists(path)) return;

  // create parent folders if not exist
  CreateParentFolders(path);

  std::ofstream file(path, std::ios_base::binary | std::ios_base::out);
  if (!file.is_open()) {
    spdlog::error("{}: Failed to open '{}'", __FUNCTION__, path.string());
    return;
  }
  file.write(data.c_str(), data.size());
  file.close();
}

void AppendFile(const fs::path& path, const std::string& data) {
  // create parent folders if not exist
  CreateParentFolders(path);

  std::ofstream file(
      path, std::ios_base::binary | std::ios_base::out | std::ios_base::app);
  if (!file.is_open()) {
    spdlog::error("{}: Failed to open '{}'", __FUNCTION__, path.string());
    return;
  }
  file.write(data.c_str(), data.size());
  file.close();
}

}  // namespace files