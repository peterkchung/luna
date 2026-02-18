// About: Minimal TIFF parser for uncompressed float data + bilinear elevation sampling.

#include "sim/Heightmap.h"
#include "util/Log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace luna::sim {

namespace {

// Reads uint16/uint32 from raw bytes with byte-order awareness
struct TiffReader {
    const uint8_t* buf;
    size_t size;
    bool littleEndian;

    uint16_t u16(size_t off) const {
        uint16_t v;
        std::memcpy(&v, buf + off, 2);
        if (!littleEndian) v = static_cast<uint16_t>((v >> 8) | (v << 8));
        return v;
    }

    uint32_t u32(size_t off) const {
        uint32_t v;
        std::memcpy(&v, buf + off, 4);
        if (!littleEndian) {
            v = ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
                ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
        }
        return v;
    }

    // Read IFD entry value — inline for count=1, else treat as offset
    uint32_t ifdValue(size_t entry, uint16_t type) const {
        if (type == 3 && u32(entry + 4) == 1) return u16(entry + 8); // SHORT
        return u32(entry + 8);
    }
};

void swapFloatBytes(std::vector<float>& data) {
    for (float& v : data) {
        uint32_t bits;
        std::memcpy(&bits, &v, 4);
        bits = ((bits >> 24) & 0xFF) | ((bits >> 8) & 0xFF00) |
               ((bits << 8) & 0xFF0000) | ((bits << 24) & 0xFF000000);
        std::memcpy(&v, &bits, 4);
    }
}

} // anonymous namespace

bool Heightmap::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_WARN("Heightmap not found: %s", path.c_str());
        return false;
    }

    auto fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> raw(fileSize);
    file.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(fileSize));

    if (fileSize < 8) {
        LOG_ERROR("TIFF too small: %s", path.c_str());
        return false;
    }

    TiffReader r{raw.data(), fileSize, false};

    // Byte order marker
    if (raw[0] == 'I' && raw[1] == 'I')
        r.littleEndian = true;
    else if (raw[0] == 'M' && raw[1] == 'M')
        r.littleEndian = false;
    else {
        LOG_ERROR("Invalid TIFF byte order: %s", path.c_str());
        return false;
    }

    if (r.u16(2) != 42) {
        LOG_ERROR("Invalid TIFF magic: %s", path.c_str());
        return false;
    }

    uint32_t ifdOff = r.u32(4);
    uint16_t numEntries = r.u16(ifdOff);

    uint32_t width = 0, height = 0;
    uint32_t stripOffsetCount = 0;
    uint32_t stripOffsetsValue = 0;
    uint16_t stripOffsetType = 0;
    uint32_t rowsPerStrip = 0;

    for (uint16_t i = 0; i < numEntries; i++) {
        size_t entry = ifdOff + 2 + i * 12;
        uint16_t tag  = r.u16(entry);
        uint16_t type = r.u16(entry + 2);
        uint32_t count = r.u32(entry + 4);

        switch (tag) {
            case 256: width  = r.ifdValue(entry, type); break;
            case 257: height = r.ifdValue(entry, type); break;
            case 273: // StripOffsets
                stripOffsetCount = count;
                stripOffsetType  = type;
                stripOffsetsValue = r.u32(entry + 8);
                break;
            case 278: rowsPerStrip = r.ifdValue(entry, type); break;
            default: break;
        }
    }

    if (width == 0 || height == 0) {
        LOG_ERROR("TIFF missing dimensions: %s", path.c_str());
        return false;
    }

    width_  = width;
    height_ = height;
    data_.resize(static_cast<size_t>(width) * height);

    if (stripOffsetCount <= 1) {
        // Single strip — all data at one offset
        uint32_t off = stripOffsetsValue;
        std::memcpy(data_.data(), raw.data() + off,
                    static_cast<size_t>(width) * height * sizeof(float));
    } else {
        // Multiple strips — offsets stored at stripOffsetsValue
        uint32_t rowsRead = 0;
        for (uint32_t s = 0; s < stripOffsetCount && rowsRead < height; s++) {
            uint32_t off;
            if (stripOffsetType == 3) // SHORT
                off = r.u16(stripOffsetsValue + s * 2);
            else
                off = r.u32(stripOffsetsValue + s * 4);

            uint32_t rows = std::min(rowsPerStrip, height - rowsRead);
            std::memcpy(data_.data() + static_cast<size_t>(rowsRead) * width,
                        raw.data() + off,
                        static_cast<size_t>(rows) * width * sizeof(float));
            rowsRead += rows;
        }
    }

    if (!r.littleEndian) swapFloatBytes(data_);

    LOG_INFO("Heightmap loaded: %ux%u from %s", width_, height_, path.c_str());
    return true;
}

double Heightmap::sample(double lat, double lon) const {
    if (data_.empty()) return 0.0;

    // Equirectangular: row 0 = north pole, col 0 = -180°
    double py = (0.5 - lat / M_PI) * (height_ - 1);
    double px = (lon / (2.0 * M_PI) + 0.5) * (width_ - 1);

    // Wrap longitude at seam
    if (px < 0.0) px += width_;
    else if (px >= width_) px -= width_;

    // Clamp latitude at poles
    py = std::clamp(py, 0.0, static_cast<double>(height_ - 1));

    // Bilinear interpolation
    int x0 = static_cast<int>(px);
    int y0 = static_cast<int>(py);
    int x1 = (x0 + 1) % static_cast<int>(width_);
    int y1 = std::min(y0 + 1, static_cast<int>(height_ - 1));

    double fx = px - x0;
    double fy = py - y0;

    double v00 = data_[y0 * width_ + x0];
    double v10 = data_[y0 * width_ + x1];
    double v01 = data_[y1 * width_ + x0];
    double v11 = data_[y1 * width_ + x1];

    double top = v00 * (1.0 - fx) + v10 * fx;
    double bot = v01 * (1.0 - fx) + v11 * fx;
    double value = top * (1.0 - fy) + bot * fy;

    // LOLA data is in kilometers — convert to meters
    return value * 1000.0;
}

} // namespace luna::sim
