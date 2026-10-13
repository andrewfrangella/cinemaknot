# CinemaKnot (C++ Core)

An air-gapped, offline, realtime multidimensional associative engine designed for experimental cinema, mechanical television, and physical instruments (turntables, modified cassette players, contact microphones).

---

## 1. Philosophical & Pedagogical Grounding

CinemaKnot steps away from cloud-dependent "black-box" machine learning and corporate APIs. Grounded in the traditions of **expanded cinema, structural film (Ken Jacobs, Larry Gottheim), and Crawford & Paglen's critique of classification**, CinemaKnot treats machine learning not as a generative oracle, but as a **transparent, physical metric space**.

* **Closed & Air-Gapped:** Zero external dependencies, no cloud subscriptions, no token costs, and 100% offline.
* **Materiality:** Operates directly on the bare metal of Raspberry Pi 4 computers and ESP32 microcontrollers.
* **Associative Knots vs. Linear Automation:** Rather than simple "follow-the-leader" mapping (e.g. loudness $\to$ brightness), CinemaKnot maps continuous physical gestures into high-dimensional space and queries structured media databases (film frames, scanlines, motor states, tape cues).

---

## 2. Hardware Topology (Local Offline IoT)

All devices communicate over an isolated local network (e.g., an ad-hoc Wi-Fi access point hosted on the Raspberry Pi or an unmanaged local switch) using UDP broadcast:

```
┌──────────────────────────────────────────────────────────────┐
│                  OFFLINE LOCAL IoT SUBNET                    │
│                     (UDP Port 8888)                          │
└───────────────▲──────────────────────────────▲───────────────┘
                │                              │
     [ KnotPacket Broadcast ]      [ KnotPacket Broadcast ]
                │                              │
 ┌──────────────┴───────────────┐ ┌────────────┴────────────────┐
 │     ESP32 Tactile Node       │ │     ESP32 Tape Node         │
 │ • Turntable Phono Cartridge  │ │ • Modified Cassette Player  │
 │ • Optical Tachometer (RPM)   │ │ • Motor PWM Speed Control   │
 │ • Piezo Contact Transducer   │ │ • Tape Head Audio ADC       │
 └──────────────────────────────┘ └─────────────────────────────┘
                │                              │
                └──────────────┬───────────────┘
                               │
               ┌───────────────▼──────────────┐
               │  Raspberry Pi 4 Media Node   │
               │ • Mechanical Television      │
               │   (32x25 Nipkow Disc Raster) │
               │ • Cinema Frame Corpus        │
               │ • Sub-millisecond KD-Tree    │
               │ • Closed-Loop Optical Sync   │
               └──────────────────────────────┘
```

---

## 3. Documentation

* [**Human & Practitioner Guide**](docs/HUMAN_GUIDE.md): Pedagogical overview, Binghamton structural film lineage, wiring diagrams, offline Wi-Fi router setup, and classroom performance scores.
* [**Agent & Architecture Guide**](docs/AGENT_GUIDE.md): Technical specification for AI coding assistants and autonomous agents, invariants, packet structure, memory rules, and class contracts.

---

## 4. Directory Layout

* [`include/CinemaKnot/Vector.hpp`](include/CinemaKnot/Vector.hpp): Multidimensional vector and distance metrics (Weighted Euclidean, Manhattan).
* [`include/CinemaKnot/KDTree.hpp`](include/CinemaKnot/KDTree.hpp): Dependency-free, allocation-free $k$-d tree for real-time spatial retrieval.
* [`include/CinemaKnot/SignalDescriptors.hpp`](include/CinemaKnot/SignalDescriptors.hpp): Audio/sensor feature extractor (RMS energy, ZCR, Cooley-Tukey Radix-2 FFT, spectral centroid, spectral flatness, transient onset detection).
* [`include/CinemaKnot/VisualDescriptors.hpp`](include/CinemaKnot/VisualDescriptors.hpp): Visual extractor for experimental frames and 32×25 Nipkow rasters (mean luminance, spatial mass centroid $C_x, C_y$, variance/texture, frame delta).
* [`include/CinemaKnot/Corpus.hpp`](include/CinemaKnot/Corpus.hpp): Associative database engine with 1-NN lookup, aleatoric cluster sampling ("knotting"), and binary serialization.
* [`include/CinemaKnot/NetworkProtocol.hpp`](include/CinemaKnot/NetworkProtocol.hpp): Compact binary protocol (`KnotPacket`) for microcontrollers and Linux.
* [`include/CinemaKnot/UdpSocket.hpp`](include/CinemaKnot/UdpSocket.hpp): Zero-dependency POSIX UDP socket implementation.
* [`examples/pi_cinema_node.cpp`](examples/pi_cinema_node.cpp): Standalone Pi appliance driving real-time queries against a 32×25 raster database.
* [`examples/esp32_tactile_node.ino`](examples/esp32_tactile_node.ino): ESP32 Arduino/ESP-IDF sketch for contact mics, turntables, and cassette players.

---

## 5. Building & Running

### On macOS / Raspberry Pi OS:
```bash
cd Developer/cinemaknot
mkdir -p build && cd build
cmake ..
make
./knot_test        # Runs verification unit tests
./pi_cinema_node   # Starts the appliance on port 8888
```

### On ESP32:
1. Open [`examples/esp32_tactile_node.ino`](file:///Users/andrewfrangella/Developer/cinemaknot/examples/esp32_tactile_node.ino) in Arduino IDE or PlatformIO.
2. Ensure the `include/CinemaKnot` headers are placed in your Arduino libraries folder or local project directory.
3. Set your local offline SSID and password (no internet needed).
4. Flash to ESP32 (NodeMCU / ESP32-WROOM-32).
