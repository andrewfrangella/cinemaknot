#pragma once

#include "Vector.hpp"
#include <vector>
#include <cmath>
#include <complex>
#include <cstdint>
#include <algorithm>

namespace CinemaKnot {

/**
 * @brief Self-contained audio and tactile signal descriptor extractor.
 * Operates without external DSP libraries; lightweight enough for ESP32 and Pi.
 */
class SignalDescriptorExtractor {
public:
    struct AnalysisResult {
        float rms{0.0f};             // Energy / Loudness [0.0 - 1.0]
        float zcr{0.0f};             // Zero Crossing Rate [0.0 - 1.0]
        float spectralCentroid{0.0f};// Center of spectral mass / Brightness [0.0 - 1.0]
        float spectralFlatness{0.0f};// Noisiness vs Tonalness (Wiener entropy) [0.0 - 1.0]
        bool isOnset{false};         // Percussive onset detected (transient hit)
        
        FeatureVector toVector() const {
            return FeatureVector{rms, zcr, spectralCentroid, spectralFlatness};
        }
    };

private:
    size_t m_fftSize;
    float m_sampleRate;
    std::vector<float> m_window;
    std::vector<std::complex<float>> m_fftBuffer;
    std::vector<float> m_magnitudes;
    
    // Onset detection state
    float m_prevRms{0.0f};
    float m_onsetThreshold{0.08f};
    int32_t m_refractorySamples{0};
    int32_t m_cooldownRemaining{0};

public:
    explicit SignalDescriptorExtractor(size_t fftSize = 256, float sampleRate = 44100.0f)
        : m_fftSize(fftSize), m_sampleRate(sampleRate) {
        // Enforce power of 2
        size_t p = 1;
        while (p < m_fftSize) p <<= 1;
        m_fftSize = p;

        m_window.resize(m_fftSize);
        m_fftBuffer.resize(m_fftSize);
        m_magnitudes.resize(m_fftSize / 2 + 1);

        // Precompute Hann window
        const float twoPi = 6.283185307179586f;
        for (size_t i = 0; i < m_fftSize; ++i) {
            m_window[i] = 0.5f * (1.0f - std::cos(twoPi * i / (m_fftSize - 1)));
        }

        m_refractorySamples = static_cast<int32_t>(0.050f * m_sampleRate); // 50ms cooldown
    }

    void setOnsetThreshold(float thresh) { m_onsetThreshold = thresh; }
    void setRefractoryMs(float ms) { m_refractorySamples = static_cast<int32_t>((ms / 1000.0f) * m_sampleRate); }

    /**
     * @brief Process a buffer of audio or sensor samples (e.g. 256 samples).
     */
    AnalysisResult process(const float* samples, size_t count) {
        AnalysisResult result;
        if (!samples || count == 0) return result;

        size_t n = std::min(count, m_fftSize);

        // 1. RMS Energy
        float sumSquares = 0.0f;
        int zeroCrossings = 0;
        float prevSign = (samples[0] >= 0.0f) ? 1.0f : -1.0f;

        for (size_t i = 0; i < n; ++i) {
            float s = samples[i];
            sumSquares += s * s;

            // 2. Zero Crossing Rate
            float curSign = (s >= 0.0f) ? 1.0f : -1.0f;
            if (curSign != prevSign) {
                zeroCrossings++;
                prevSign = curSign;
            }

            // Windowed signal for FFT
            m_fftBuffer[i] = std::complex<float>(s * m_window[i], 0.0f);
        }

        for (size_t i = n; i < m_fftSize; ++i) {
            m_fftBuffer[i] = std::complex<float>(0.0f, 0.0f);
        }

        result.rms = std::sqrt(sumSquares / static_cast<float>(n));
        result.zcr = static_cast<float>(zeroCrossings) / static_cast<float>(n);

        // 3. Onset Detection (Energy Flux + Refractory Window)
        float energyDelta = result.rms - m_prevRms;
        if (m_cooldownRemaining > 0) {
            m_cooldownRemaining -= static_cast<int32_t>(n);
            result.isOnset = false;
        } else if (energyDelta > m_onsetThreshold && result.rms > 0.05f) {
            result.isOnset = true;
            m_cooldownRemaining = m_refractorySamples;
        } else {
            result.isOnset = false;
        }
        m_prevRms = result.rms;

        // 4. In-place Cooley-Tukey Radix-2 FFT
        computeFFT(m_fftBuffer);

        // 5. Magnitudes, Spectral Centroid, and Spectral Flatness
        size_t half = m_fftSize / 2;
        float magSum = 0.0f;
        float weightedSum = 0.0f;
        float logSum = 0.0f;
        float powerSum = 0.0f;
        const float eps = 1e-9f;

        for (size_t k = 0; k <= half; ++k) {
            float mag = std::abs(m_fftBuffer[k]);
            m_magnitudes[k] = mag;
            magSum += mag;
            weightedSum += static_cast<float>(k) * mag;

            float power = mag * mag + eps;
            powerSum += power;
            logSum += std::log(power);
        }

        // Spectral Centroid normalized [0, 1]
        if (magSum > eps) {
            result.spectralCentroid = (weightedSum / magSum) / static_cast<float>(half);
        } else {
            result.spectralCentroid = 0.0f;
        }

        // Spectral Flatness (ratio of geometric mean to arithmetic mean)
        float geometricMean = std::exp(logSum / static_cast<float>(half + 1));
        float arithmeticMean = powerSum / static_cast<float>(half + 1);
        if (arithmeticMean > eps) {
            result.spectralFlatness = std::min(1.0f, geometricMean / arithmeticMean);
        } else {
            result.spectralFlatness = 0.0f;
        }

        return result;
    }

private:
    static void computeFFT(std::vector<std::complex<float>>& a) {
        size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(a[i], a[j]);
            }
        }

        const float pi = 3.14159265358979323846f;
        for (size_t len = 2; len <= n; len <<= 1) {
            float ang = -2.0f * pi / static_cast<float>(len);
            std::complex<float> wlen(std::cos(ang), std::sin(ang));
            for (size_t i = 0; i < n; i += len) {
                std::complex<float> w(1.0f, 0.0f);
                for (size_t j = 0; j < len / 2; ++j) {
                    std::complex<float> u = a[i + j];
                    std::complex<float> v = a[i + j + len / 2] * w;
                    a[i + j] = u + v;
                    a[i + j + len / 2] = u - v;
                    w *= wlen;
                }
            }
        }
    }
};

} // namespace CinemaKnot
