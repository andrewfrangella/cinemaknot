#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

namespace CinemaKnot {

#pragma pack(push, 1)

/**
 * @brief Compact, fixed-size binary packet for air-gapped IoT communication
 * between Raspberry Pis and ESP32 microcontrollers over UDP.
 */
struct KnotPacket {
    static constexpr uint16_t MAGIC = 0x4B4E; // 'K', 'N' (KNot)
    static constexpr uint8_t VERSION = 1;
    static constexpr size_t MAX_DIM = 8;

    enum Type : uint8_t {
        TYPE_QUERY     = 0x01, // Feature vector sent to request corpus match
        TYPE_RESULT    = 0x02, // Query match result (ID, distance, payload)
        TYPE_ONSET     = 0x03, // Transient trigger (needle drop, tape splice, opto pulse)
        TYPE_TELEMETRY = 0x04, // Continuous hardware state (motor RPM, pitch, fader)
        TYPE_SYNC      = 0x05  // Master phase sync pulse (e.g. 10 FPS Nipkow revolution)
    };

    uint16_t magic{MAGIC};
    uint8_t  version{VERSION};
    uint8_t  type{TYPE_QUERY};
    uint8_t  sourceNodeId{0};
    uint8_t  targetNodeId{0}; // 0 = broadcast to all
    uint16_t sequence{0};

    // Payload fields
    uint32_t matchedId{0};    // Corpus index (frame #, tape cue #)
    float    distance{0.0f};  // Match distance in feature space
    uint8_t  vectorDim{0};
    float    vectorData[MAX_DIM]{0.0f};

    // Helper functions
    bool isValid() const {
        return magic == MAGIC && version == VERSION;
    }

    void setVector(const std::vector<float>& vals) {
        vectorDim = static_cast<uint8_t>(std::min(vals.size(), MAX_DIM));
        for (size_t i = 0; i < vectorDim; ++i) {
            vectorData[i] = vals[i];
        }
    }

    std::vector<float> getVector() const {
        return std::vector<float>(vectorData, vectorData + vectorDim);
    }
};

#pragma pack(pop)

} // namespace CinemaKnot
