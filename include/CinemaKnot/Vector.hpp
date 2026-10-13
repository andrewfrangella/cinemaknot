#pragma once

#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <numeric>

namespace CinemaKnot {

/**
 * @brief Multidimensional feature vector representation for associative indexing.
 * Pure C++ with zero external dependencies, compatible with Linux (Raspberry Pi)
 * and FreeRTOS / ESP-IDF (ESP32).
 */
class FeatureVector {
public:
    std::vector<float> values;

    FeatureVector() = default;
    explicit FeatureVector(size_t dim, float initVal = 0.0f) : values(dim, initVal) {}
    FeatureVector(std::initializer_list<float> list) : values(list) {}
    explicit FeatureVector(const std::vector<float>& vals) : values(vals) {}

    size_t dim() const { return values.size(); }
    float& operator[](size_t i) { return values[i]; }
    const float& operator[](size_t i) const { return values[i]; }

    /**
     * @brief Weighted Euclidean Distance: d = sqrt(sum(w_i * (u_i - v_i)^2))
     */
    float distanceSquared(const FeatureVector& other, const std::vector<float>& weights = {}) const {
        size_t n = std::min(values.size(), other.values.size());
        float sum = 0.0f;
        bool hasWeights = (weights.size() >= n);

        for (size_t i = 0; i < n; ++i) {
            float diff = values[i] - other.values[i];
            float w = hasWeights ? weights[i] : 1.0f;
            sum += w * (diff * diff);
        }
        return sum;
    }

    float distance(const FeatureVector& other, const std::vector<float>& weights = {}) const {
        return std::sqrt(distanceSquared(other, weights));
    }

    /**
     * @brief Manhattan (L1) Distance: d = sum(w_i * |u_i - v_i|)
     */
    float manhattanDistance(const FeatureVector& other, const std::vector<float>& weights = {}) const {
        size_t n = std::min(values.size(), other.values.size());
        float sum = 0.0f;
        bool hasWeights = (weights.size() >= n);

        for (size_t i = 0; i < n; ++i) {
            float diff = std::fabs(values[i] - other.values[i]);
            float w = hasWeights ? weights[i] : 1.0f;
            sum += w * diff;
        }
        return sum;
    }

    /**
     * @brief Normalize vector in-place using min-max bounds
     */
    void normalizeMinMax(const std::vector<float>& mins, const std::vector<float>& maxs) {
        size_t n = std::min(values.size(), std::min(mins.size(), maxs.size()));
        for (size_t i = 0; i < n; ++i) {
            float range = maxs[i] - mins[i];
            if (range > 1e-6f) {
                values[i] = (values[i] - mins[i]) / range;
            } else {
                values[i] = 0.0f;
            }
        }
    }
};

} // namespace CinemaKnot
