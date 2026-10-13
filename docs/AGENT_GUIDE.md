# CinemaKnot: Agent & System Architecture Guide

> **Technical Specification and Operational Invariants for AI Coding Assistants and Autonomous Agents**

---

## 1. System Invariants & Non-Negotiable Rules

Any agent modifying, extending, or refactoring this codebase **must strictly adhere** to the following constraints:

1. **Air-Gapped & Offline by Design:**
   * Under no circumstances may any code make outbound HTTP/HTTPS requests, phone home, download external assets, or query cloud inference APIs.
   * All algorithms must execute deterministically on local silicon (ESP32-WROOM-32 microcontrollers and Raspberry Pi 4 SBCs).
2. **Zero Heap Allocation in Real-Time Loops:**
   * The query methods (`findNearest`, `queryKnot`), audio analysis (`SignalDescriptorExtractor::process`), and network parsing must not invoke `malloc`, `free`, `new`, or heap re-allocations during active streaming.
   * Pre-allocate all buffers in constructor initializers.
3. **Dual-Target Cross-Compilation:**
   * Every header in `include/CinemaKnot/` must compile identically with:
     * **Host/Pi:** `clang++` / `g++` (C++17, 64-bit ARM / x86_64 POSIX).
     * **Microcontroller:** `xtensa-esp32-elf-g++` (C++11/C++17, FreeRTOS / Arduino core).
   * Do not introduce dependencies on Boost, Eigen, OpenCV, or OS-specific threads into the core headers.
4. **Physical Raster Conventions:**
   * In bitluni's Nipkow disc geometry: $W = 32$ columns, $H = 25$ rows (800 pixels per revolution / frame).
   * Scanning proceeds left-to-right, top-to-bottom ($x = 31 - c$, $y = 24 - r$). Do not invert the scan direction.

---

## 2. Memory Layout & Packet Specification

All IoT nodes exchange binary UDP packets packed to 1-byte alignment (`#pragma pack(push, 1)`). Total packet size is **49 bytes**.

```
Offset  Size  Type       Field Name     Description
────────────────────────────────────────────────────────────────────────
0x00    2     uint16_t   magic          0x4B4E ('K', 'N' in ASCII)
0x02    1     uint8_t    version        Protocol version (currently 0x01)
0x03    1     uint8_t    type           Packet type enum (see below)
0x04    1     uint8_t    sourceNodeId   Node ID (1=Turntable, 2=Tape, 10=Pi)
0x05    1     uint8_t    targetNodeId   Target node ID (0 = Broadcast to all)
0x06    2     uint16_t   sequence       Monotonically increasing sequence #
0x08    4     uint32_t   matchedId      Corpus payload (frame #, tape cue)
0x0C    4     float      distance       Calculated feature space distance
0x10    1     uint8_t    vectorDim      Active dimensions in vectorData (<= 8)
0x11    32    float[8]   vectorData     Raw 32-bit float descriptor values
────────────────────────────────────────────────────────────────────────
Total: 49 bytes
```

### Packet Type Enumeration (`KnotPacket::Type`)
* `0x01` (`TYPE_QUERY`): Sent by sensor/tactile nodes (ESP32) containing a real-time descriptor vector to search the corpus.
* `0x02` (`TYPE_RESULT`): Broadcast by the database engine (Pi) containing the matched ID, distance, and confidence.
* `0x03` (`TYPE_ONSET`): Sent immediately upon transient edge detection (needle drop, tape splice, contact hit).
* `0x04` (`TYPE_TELEMETRY`): Continuous hardware state (motor RPM, fader position, opto-sensor jitter).
* `0x05` (`TYPE_SYNC`): Master phase clock synchronization pulse.

---

## 3. Core Class Contracts

### `CinemaKnot::FeatureVector`
* Lightweight wrapper over `std::vector<float>` or stack values.
* Implements weighted Euclidean distance: $d^2 = \sum_{i=0}^{D-1} w_i (u_i - v_i)^2$.
* Implements Manhattan ($L_1$) distance: $d = \sum_{i=0}^{D-1} w_i |u_i - v_i|$.

### `CinemaKnot::KDTree<PayloadType>`
* Balanced $k$-d tree built using $O(N \log N)$ recursive median partitioning via `std::nth_element`.
* 1-NN search (`findNearest`) runs in $O(\log N)$ expected time.
* $K$-NN search (`findKNearest`) uses a bounded priority queue (max-heap) with no dynamic re-allocations.

### `CinemaKnot::SignalDescriptorExtractor`
* Windowed analysis engine designed for real-time contact mic, phono cartridge, or tape head signals.
* Precomputes Hann window in constructor.
* In-place Cooley-Tukey Radix-2 Real FFT ($N = 128, 256, 512$).
* Onset detection uses positive energy flux with refractory lockout (default 50 ms) to eliminate false double-triggers.

### `CinemaKnot::VisualDescriptorExtractor`
* Analyzes raw 8-bit grayscale buffers.
* Calculates:
  1. Mean Luminance: $\bar{L} = \frac{1}{WH} \sum I(x, y)$
  2. Spatial Center of Mass: $C_x = \frac{\sum x \cdot I(x, y)}{W \sum I(x, y)}$, $C_y = \frac{\sum y \cdot I(x, y)}{H \sum I(x, y)}$
  3. Spatial Variance / Grain Texture: $\sigma^2 = \frac{1}{WH} \sum (I(x,y) - \bar{L})^2$
  4. Temporal Frame Delta: $\Delta_t = \frac{1}{WH} \sum |I_t(x, y) - I_{t-1}(x, y)|$

---

## 4. How to Extend the Codebase

### Adding a New Physical Node (e.g. Raspberry Pi Pico W or Teensy)
1. Include `CinemaKnot/NetworkProtocol.hpp` and `CinemaKnot/SignalDescriptors.hpp`.
2. Configure your network stack to bind UDP port 8888.
3. In your main ADC loop:
   * Populate a sample buffer.
   * Call `extractor.process(buffer, size)`.
   * Fill a `KnotPacket` with `TYPE_QUERY` or `TYPE_ONSET` and call your network socket's broadcast method.

### Bridging into `mechanicaltv`
To route `CinemaKnot` output directly into the existing `mechanicaltv` Python/C++ daemon:
1. `pi_cinema_node` broadcasts `TYPE_RESULT` on port 8888.
2. In `mechanical_tv/server.py`, add a background UDP listener on port 8888.
3. Upon receiving `matchedId`, call the optical preview pipeline:
   ```python
   # In mechanical_tv server loop:
   frame = corpus_frames[packet.matchedId]
   motor_controller.set_phase_offset(packet.distance * 360.0)
   led_modulator.render_frame(frame)
   ```
