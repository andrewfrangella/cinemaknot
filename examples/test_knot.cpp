#include "CinemaKnot/CinemaKnot.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cmath>

int main() {
    std::cout << "=== " << CinemaKnot::getVersion() << " ===\n";

    // 1. Test FeatureVector & Distance
    CinemaKnot::FeatureVector v1{0.1f, 0.5f, 0.8f};
    CinemaKnot::FeatureVector v2{0.1f, 0.6f, 0.9f};
    float dist = v1.distance(v2);
    std::cout << "[Test 1] Vector distance: " << dist << "\n";
    assert(dist > 0.1f && dist < 0.2f);

    // 2. Test KDTree and Corpus Construction
    CinemaKnot::Corpus<uint32_t> corpus;
    
    // Simulate a database of 100 archival 32x25 frames
    // Dim 0: Mean Luminance, Dim 1: Centroid X, Dim 2: Centroid Y, Dim 3: Variance
    for (uint32_t i = 0; i < 100; ++i) {
        float lum = static_cast<float>(i) / 100.0f;
        float cx = 0.5f + 0.3f * std::sin(static_cast<float>(i) * 0.2f);
        float cy = 0.5f + 0.3f * std::cos(static_cast<float>(i) * 0.2f);
        float var = 0.2f + 0.1f * std::sin(static_cast<float>(i) * 0.5f);
        corpus.addEntry({lum, cx, cy, var}, i);
    }
    corpus.compileIndex();
    std::cout << "[Test 2] Compiled corpus with " << corpus.size() << " entries.\n";

    // 3. Test Query Nearest (1-NN)
    CinemaKnot::FeatureVector queryPoint{0.45f, 0.6f, 0.4f, 0.25f};
    uint32_t matchedId = 0;
    float matchDist = 0.0f;
    bool found = corpus.queryNearest(queryPoint, matchedId, matchDist);
    assert(found);
    std::cout << "[Test 3] 1-NN Match: Frame ID=" << matchedId << ", Distance=" << matchDist << "\n";

    // 4. Test Aleatoric Knot Query (Top-K sampling)
    uint32_t knotId = 0;
    float knotDist = 0.0f;
    bool knotFound = corpus.queryKnot(queryPoint, 5, 0.5f, knotId, knotDist);
    assert(knotFound);
    std::cout << "[Test 4] Aleatoric Knot Match: Frame ID=" << knotId << ", Distance=" << knotDist << "\n";

    // 5. Test Signal Descriptors (Audio / Tactile input)
    CinemaKnot::SignalDescriptorExtractor audioExtractor(256, 44100.0f);
    std::vector<float> sineWave(256);
    for (size_t i = 0; i < 256; ++i) {
        sineWave[i] = std::sin(2.0f * 3.14159265f * 440.0f * i / 44100.0f);
    }
    auto audioFeat = audioExtractor.process(sineWave.data(), sineWave.size());
    std::cout << "[Test 5] Audio Extractor -> RMS: " << audioFeat.rms
              << ", Centroid: " << audioFeat.spectralCentroid
              << ", Flatness: " << audioFeat.spectralFlatness << "\n";
    assert(audioFeat.rms > 0.6f);

    // 6. Test Visual Descriptors (Mechanical TV 32x25 raster)
    CinemaKnot::VisualDescriptorExtractor visualExtractor;
    std::vector<uint8_t> nipkowFrame(32 * 25, 0);
    // Draw a bright spot in upper right
    for (int y = 0; y < 10; ++y) {
        for (int x = 20; x < 32; ++x) {
            nipkowFrame[y * 32 + x] = 255;
        }
    }
    auto visFeat = visualExtractor.process(nipkowFrame.data(), 32, 25);
    std::cout << "[Test 6] Visual Extractor -> Mean Lum: " << visFeat.meanLuminance
              << ", Cx: " << visFeat.centroidX << ", Cy: " << visFeat.centroidY
              << ", Variance: " << visFeat.spatialVariance << "\n";
    assert(visFeat.centroidX > 0.5f); // Should lean right

    // 7. Test Packet Serialization
    CinemaKnot::KnotPacket pkt;
    pkt.type = CinemaKnot::KnotPacket::TYPE_QUERY;
    pkt.sourceNodeId = 1; // Turntable ESP32
    pkt.setVector({audioFeat.rms, audioFeat.spectralCentroid, audioFeat.spectralFlatness});
    assert(pkt.isValid());
    std::cout << "[Test 7] Packet serialized: Type=" << (int)pkt.type
              << ", Dim=" << (int)pkt.vectorDim << ", Bytes=" << sizeof(pkt) << "\n";

    std::cout << "All CinemaKnot core tests passed successfully!\n";
    return 0;
}
