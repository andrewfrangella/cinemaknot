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
* [`hub/server.py`](hub/server.py): **Central Nervous System Hub** (Async Python/WebSockets daemon bridging UDP IoT to web UI).
* [`hub/static/`](hub/static/): Interactive web dashboard (neural topology graph, live patch bay, VU meters).
* [`examples/pi_cinema_node.cpp`](examples/pi_cinema_node.cpp): Standalone Pi appliance driving real-time queries against a 32×25 raster database.
* [`examples/esp32_tactile_node.ino`](examples/esp32_tactile_node.ino): ESP32 Arduino/ESP-IDF sketch for contact mics, turntables, and cassette players.
* [`examples/simulate_nodes.py`](examples/simulate_nodes.py): Multi-node hardware emulator for testing without physical sensors connected.

---

## 5. Building & Running

### Central Nervous System Hub (macOS or Raspberry Pi):
```bash
cd Developer/cinemaknot
python3 hub/server.py
```
Open **`http://127.0.0.1:8880`** in your browser. All discovered ESP32 and Pi nodes appear automatically with live VU telemetry, interactive dragging, and pluggable patch cables.

To test the Hub without hardware attached, launch the multi-node simulator in a separate terminal:
```bash
python3 examples/simulate_nodes.py
```

### C++ Core Engine (macOS / Raspberry Pi):
```bash
cd Developer/cinemaknot
mkdir -p build && cd build
cmake ..
make -j4
./knot_test        # Runs verification unit tests
./pi_cinema_node   # Starts the appliance on port 8888
```

### ESP32 Microcontrollers:
1. Open [`examples/esp32_tactile_node.ino`](examples/esp32_tactile_node.ino) in Arduino IDE or PlatformIO.
2. Ensure the `include/CinemaKnot` headers are placed in your Arduino search path.
3. Select your ESP32 board, set your offline SSID/password, and flash over USB.
4. Open the Serial Monitor at 115200 baud. The node will automatically announce itself to the Hub via `TYPE_HEARTBEAT`.
