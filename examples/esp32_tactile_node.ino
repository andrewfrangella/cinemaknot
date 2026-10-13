/**
 * @file esp32_tactile_node.ino
 * @brief Standalone ESP32 Node for Physical Performative Instruments
 * (Turntables, Modified Cassette Players, Contact Microphones).
 *
 * Fully air-gapped, offline IoT node communicating over local UDP broadcast.
 * Reads analog signals, calculates descriptors, and queries the cinema database.
 */

#include <WiFi.h>
#include <WiFiUdp.h>

// Direct inclusion of CinemaKnot protocol headers
#include "CinemaKnot/NetworkProtocol.hpp"
#include "CinemaKnot/SignalDescriptors.hpp"

// Network Configuration (Air-gapped private hotspot hosted by Raspberry Pi or local router)
const char* WIFI_SSID     = "CinemaKnotNet";
const char* WIFI_PASSWORD = "cinemaknotoffline";
const uint16_t IOT_PORT   = 8888;
const uint8_t  NODE_ID    = 1; // 1 = Turntable, 2 = Cassette Deck, etc.

// Hardware Pins
const int ANALOG_INPUT_PIN = 34; // Contact mic, phono cartridge, or tape head signal
const int MOTOR_PWM_PIN    = 18; // Motor speed control output (0-255)
const int STROBE_LED_PIN   = 2;  // Built-in status / optical strobe LED

WiFiUDP udp;
CinemaKnot::SignalDescriptorExtractor extractor(128, 8000.0f); // 128-point FFT @ 8kHz sample rate
uint16_t packetSequence = 0;

const size_t BUFFER_SIZE = 128;
float sampleBuffer[BUFFER_SIZE];
size_t sampleIndex = 0;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== CinemaKnot ESP32 Tactile Node ===");

    pinMode(ANALOG_INPUT_PIN, INPUT);
    pinMode(STROBE_LED_PIN, OUTPUT);
    pinMode(MOTOR_PWM_PIN, OUTPUT);
    analogWrite(MOTOR_PWM_PIN, 128); // Nominal motor speed

    // Connect to private offline Wi-Fi
    Serial.printf("Connecting to offline network: %s\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(300);
        digitalWrite(STROBE_LED_PIN, !digitalRead(STROBE_LED_PIN));
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected! Local IP: %s\n", WiFi.localIP().toString().c_str());
        udp.begin(IOT_PORT);
        digitalWrite(STROBE_LED_PIN, HIGH);
    } else {
        Serial.println("\n[Warning] Wi-Fi connection timed out. Check Pi hotspot.");
    }
}

void loop() {
    // 1. High-rate ADC sampling (approx 8 kHz)
    int raw = analogRead(ANALOG_INPUT_PIN);
    // Center 12-bit ADC (0 - 4095) around 0.0 (-1.0 to +1.0)
    float sample = (static_cast<float>(raw) - 2048.0f) / 2048.0f;
    sampleBuffer[sampleIndex++] = sample;

    // 2. When window buffer is full, extract descriptors
    if (sampleIndex >= BUFFER_SIZE) {
        sampleIndex = 0;

        auto feat = extractor.process(sampleBuffer, BUFFER_SIZE);

        // 3. Transmit Onset Trigger immediately if transient occurs
        if (feat.isOnset) {
            CinemaKnot::KnotPacket onsetPkt;
            onsetPkt.type = CinemaKnot::KnotPacket::TYPE_ONSET;
            onsetPkt.sourceNodeId = NODE_ID;
            onsetPkt.sequence = packetSequence++;
            
            udp.beginPacket(IPAddress(255, 255, 255, 255), IOT_PORT);
            udp.write(reinterpret_cast<const uint8_t*>(&onsetPkt), sizeof(onsetPkt));
            udp.endPacket();

            digitalWrite(STROBE_LED_PIN, HIGH);
            delayMicroseconds(500); // Visual strobe flash
            digitalWrite(STROBE_LED_PIN, LOW);
        }

        // 4. Send continuous feature query if signal is active
        if (feat.rms > 0.02f) {
            CinemaKnot::KnotPacket queryPkt;
            queryPkt.type = CinemaKnot::KnotPacket::TYPE_QUERY;
            queryPkt.sourceNodeId = NODE_ID;
            queryPkt.sequence = packetSequence++;
            queryPkt.setVector({feat.rms, feat.zcr, feat.spectralCentroid, feat.spectralFlatness});

            udp.beginPacket(IPAddress(255, 255, 255, 255), IOT_PORT);
            udp.write(reinterpret_cast<const uint8_t*>(&queryPkt), sizeof(queryPkt));
            udp.endPacket();
        }
    }

    // 5. Check for incoming responses from the Raspberry Pi / Cinema Engine
    int packetSize = udp.parsePacket();
    if (packetSize >= sizeof(CinemaKnot::KnotPacket)) {
        CinemaKnot::KnotPacket inPkt;
        udp.read(reinterpret_cast<char*>(&inPkt), sizeof(inPkt));

        if (inPkt.isValid() && inPkt.type == CinemaKnot::KnotPacket::TYPE_RESULT) {
            // E.g. modulate the cassette motor speed based on feature distance
            int targetPwm = static_cast<int>(120.0f + inPkt.distance * 100.0f);
            analogWrite(MOTOR_PWM_PIN, constrain(targetPwm, 50, 255));
        }
    }

    // Micro-delay for sample timing pacing
    delayMicroseconds(120);
}
