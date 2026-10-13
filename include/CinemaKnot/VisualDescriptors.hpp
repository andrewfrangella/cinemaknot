#pragma once

#include "Vector.hpp"
#include <vector>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

namespace CinemaKnot {

/**
 * @brief Visual descriptor extractor for experimental film scans, video frames,
 * and 32x25 Nipkow mechanical television rasters.
 */
class VisualDescriptorExtractor {
public:
    struct VisualFeatures {
        float meanLuminance{0.0f};   // Overall illumination [0.0 - 1.0]
        float centroidX{0.5f};       // Horizontal center of mass [0.0 - 1.0]
        float centroidY{0.5f};       // Vertical center of mass [0.0 - 1.0]
        float spatialVariance{0.0f}; // Contrast / high-frequency grain density [0.0 - 1.0]
        float temporalDelta{0.0f};   // Difference from previous frame [0.0 - 1.0]

        FeatureVector toVector() const {
            return FeatureVector{meanLuminance, centroidX, centroidY, spatialVariance, temporalDelta};
        }
    };

private:
    std::vector<uint8_t> m_prevFrame;

public:
    VisualDescriptorExtractor() = default;

    /**
     * @brief Process an 8-bit grayscale image buffer (e.g. 32x25 for Nipkow or 320x240 for video).
     * @param pixels Raw 8-bit grayscale bytes.
     * @param width Image width.
     * @param height Image height.
     */
    VisualFeatures process(const uint8_t* pixels, size_t width, size_t height) {
        VisualFeatures feat;
        if (!pixels || width == 0 || height == 0) return feat;

        size_t totalPixels = width * height;
        float sumLum = 0.0f;
        float sumWeightedX = 0.0f;
        float sumWeightedY = 0.0f;

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                float val = static_cast<float>(pixels[y * width + x]) / 255.0f;
                sumLum += val;
                sumWeightedX += val * static_cast<float>(x);
                sumWeightedY += val * static_cast<float>(y);
            }
        }

        feat.meanLuminance = sumLum / static_cast<float>(totalPixels);

        if (sumLum > 1e-5f) {
            feat.centroidX = (sumWeightedX / sumLum) / static_cast<float>(width);
            feat.centroidY = (sumWeightedY / sumLum) / static_cast<float>(height);
        } else {
            feat.centroidX = 0.5f;
            feat.centroidY = 0.5f;
        }

        // Spatial variance (film grain, contrast, edge complexity)
        float sumSqDiff = 0.0f;
        for (size_t i = 0; i < totalPixels; ++i) {
            float val = static_cast<float>(pixels[i]) / 255.0f;
            float diff = val - feat.meanLuminance;
            sumSqDiff += diff * diff;
        }
        feat.spatialVariance = std::min(1.0f, (sumSqDiff / static_cast<float>(totalPixels)) * 4.0f);

        // Temporal delta against previous frame
        if (!m_prevFrame.empty() && m_prevFrame.size() == totalPixels) {
            float sumDelta = 0.0f;
            for (size_t i = 0; i < totalPixels; ++i) {
                float d = std::abs(static_cast<float>(pixels[i]) - static_cast<float>(m_prevFrame[i])) / 255.0f;
                sumDelta += d;
            }
            feat.temporalDelta = sumDelta / static_cast<float>(totalPixels);
        } else {
            feat.temporalDelta = 0.0f;
            m_prevFrame.resize(totalPixels);
        }

        std::copy(pixels, pixels + totalPixels, m_prevFrame.begin());
        return feat;
    }
};

} // namespace CinemaKnot
