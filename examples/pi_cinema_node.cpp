#include "CinemaKnot/CinemaKnot.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>

/**
 * @brief Standalone CinemaKnot Node for Raspberry Pi 4.
 * Air-gapped, zero-cloud appliance that connects physical tactile sensors
 * (turntables, modified cassette decks, contact mics) to experimental moving image corpora.
 */
int main() {
    std::cout << "=========================================================\n";
    std::cout << " CinemaKnot Pi Node - Experimental Cinema Appliance      \n";
    std::cout << " Offline Local IoT Core (Raspberry Pi 4)                \n";
    std::cout << "=========================================================\n";

    const uint16_t IOT_PORT = 8888;
    CinemaKnot::UdpSocket socket;

    if (!socket.open(IOT_PORT, true)) {
        std::cerr << "Failed to open UDP socket on port " << IOT_PORT << "\n";
        return 1;
    }
    std::cout << "[IoT] Listening on UDP 0.0.0.0:" << IOT_PORT << " (Broadcast enabled)\n";

    // 1. Build an in-memory 32x25 Mechanical TV frame corpus
    // (In production, replace with actual 32x25 scans loaded from an SD card directory)
    CinemaKnot::Corpus<uint32_t> frameCorpus;
    CinemaKnot::VisualDescriptorExtractor visualExtractor;

    const size_t CORPUS_SIZE = 500;
    std::vector<std::vector<uint8_t>> frameStorage(CORPUS_SIZE, std::vector<uint8_t>(32 * 25));

    std::cout << "[Corpus] Synthesizing and indexing " << CORPUS_SIZE << " 32x25 experimental frames...\n";
    for (uint32_t i = 0; i < CORPUS_SIZE; ++i) {
        auto& pixels = frameStorage[i];
        float phase = static_cast<float>(i) * 0.1f;
        
        // Procedural structural film patterns (bars, spirals, optical flickers)
        for (int y = 0; y < 25; ++y) {
            for (int x = 0; x < 32; ++x) {
                float val = 127.0f + 127.0f * std::sin(phase + static_cast<float>(x) * 0.3f)
                                     * std::cos(static_cast<float>(y) * 0.4f);
                pixels[y * 32 + x] = static_cast<uint8_t>(std::max(0.0f, std::min(255.0f, val)));
            }
        }

        auto feat = visualExtractor.process(pixels.data(), 32, 25);
        // Feature space: [Mean Lum, Centroid X, Centroid Y, Spatial Variance]
        frameCorpus.addEntry({feat.meanLuminance, feat.centroidX, feat.centroidY, feat.spatialVariance}, i);
    }

    frameCorpus.compileIndex();
    std::cout << "[Corpus] Spatial KD-Tree index compiled. Ready for real-time performance.\n";

    uint32_t activeFrameId = 0;
    float currentPhaseOffsetDeg = 0.0f;
    uint16_t packetSeq = 0;

    std::cout << "[Ready] Awaiting signals from ESP32s (Turntables / Cassette / Contact Mics)...\n";
    std::cout << "Press Ctrl+C to exit.\n\n";

    while (true) {
        CinemaKnot::KnotPacket pkt;
        std::string senderIp;

        // Poll non-blocking UDP incoming stream
        while (socket.receivePacket(pkt, &senderIp)) {
            if (pkt.type == CinemaKnot::KnotPacket::TYPE_QUERY) {
                // Incoming vector from student's instrument (e.g. turntable needle or cassette tape head)
                auto vec = pkt.getVector();
                
                // Pad or truncate to 4 dimensions to match our visual corpus
                CinemaKnot::FeatureVector queryVec(4, 0.5f);
                for (size_t d = 0; d < std::min(vec.size(), size_t(4)); ++d) {
                    queryVec[d] = vec[d];
                }

                uint32_t matchedFrame = 0;
                float distance = 0.0f;

                // Aleatoric knot lookup with small divergence (0.25) to avoid sterile 1:1 repetition
                if (frameCorpus.queryKnot(queryVec, 6, 0.25f, matchedFrame, distance)) {
                    activeFrameId = matchedFrame;
                    // Map feature distance to physical framing phase rotation (0 - 359 degrees)
                    currentPhaseOffsetDeg = std::fmod(distance * 360.0f, 360.0f);

                    std::cout << "[KNOT] Node " << (int)pkt.sourceNodeId 
                              << " (" << senderIp << ") -> Matched Frame #" << activeFrameId
                              << " (d=" << distance << ", Phase=" << currentPhaseOffsetDeg << "°)\n";

                    // Broadcast response back across IoT so other devices can slave to this event
                    CinemaKnot::KnotPacket resp;
                    resp.type = CinemaKnot::KnotPacket::TYPE_RESULT;
                    resp.sourceNodeId = 10; // Pi Master
                    resp.targetNodeId = 0;  // Broadcast to all
                    resp.matchedId = activeFrameId;
                    resp.distance = distance;
                    resp.sequence = packetSeq++;
                    socket.broadcastPacket(resp, IOT_PORT);
                }
            } else if (pkt.type == CinemaKnot::KnotPacket::TYPE_ONSET) {
                // Hard percussive hit (needle drop, tape splice, or contact mic transient)
                std::cout << "[TRIGGER] Onset detected from Node " << (int)pkt.sourceNodeId << "!\n";
                // Advance frame or trigger mechanical strobe
            }
        }

        // Sleep 1ms to prevent CPU burn in the polling loop
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return 0;
}
