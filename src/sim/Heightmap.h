// About: Minimal TIFF loader and bilinear sampler for NASA LOLA elevation data.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace luna::sim {

class Heightmap {
public:
    bool load(const std::string& path);

    // Sample elevation at lat/lon (radians). Returns meters above reference sphere.
    double sample(double lat, double lon) const;

    bool isLoaded() const { return !data_.empty(); }

private:
    std::vector<float> data_;
    uint32_t width_  = 0;
    uint32_t height_ = 0;
};

} // namespace luna::sim
