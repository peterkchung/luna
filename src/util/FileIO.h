// About: File I/O helpers for loading binary data (SPIR-V shaders, assets).

#pragma once

#include <string>
#include <vector>

namespace luna::util {

std::vector<char> readBinaryFile(const std::string& path);

} // namespace luna::util
