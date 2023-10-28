#pragma once

// global
#include <filesystem>

// create fs::path in place always faster than concatenation
namespace fs = std::filesystem;

namespace files {

void CreateParentFolders(const fs::path& path);
// any file extension, std::string as raw byte data
void WriteFile(const fs::path& path, const std::string& data);
// text files
void AppendFile(const fs::path& path, const std::string& data);

}  // namespace files