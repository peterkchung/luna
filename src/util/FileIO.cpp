// About: File I/O implementation — binary file reading for SPIR-V and assets.

#include "util/FileIO.h"
#include "util/Log.h"
#include <fstream>
#include <stdexcept>

namespace luna::util {

std::vector<char> readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file: %s", path.c_str());
        throw std::runtime_error("Failed to open file: " + path);
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

} // namespace luna::util
