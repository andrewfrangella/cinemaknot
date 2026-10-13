# CinemaKnot: Human & Practitioner Guide

> **A Guide for Artists, Instructors, and Students of Experimental Cinema & Media Art**

---

## 1. Pedagogical Philosophy & Core Concepts

### Demystifying Machine Learning
Most contemporary artificial intelligence operates as an extractive, opaque "black box" hosted in corporate data centers. In the lineage of Crawford & Paglen’s *Excavating AI*, **CinemaKnot** strips away the myth of the "generative oracle." 

Instead of generating synthetic images out of nowhere, CinemaKnot exposes the foundational mathematical mechanism of machine learning: **spatial classification and metric distance in a multidimensional feature space**. 

* **The System Does Not Think for You:** It does not produce poetic text or hallucinated imagery. It is a mathematical instrument that measures physical phenomena (the rumble of a turntable, the flutter of an analog tape head, the contact mic on a chassis) and calculates geometric distances to an archival cinema database.
* **Resisting the ELIZA Effect:** We reject the tendency to interpret mechanical coincidence as intentional machine intelligence. The system outputs flat measurements, metric distances, and spatial coordinates.
* **Completely Air-Gapped and Sovereign:** The system runs entirely on bare-metal microcontrollers (ESP32) and small computers (Raspberry Pi 4). It has no internet connection, incurs zero token costs, and stores all data locally.

### Expanding Structural Film & Expanded Cinema
Rooted in the Binghamton experimental cinema tradition (Ken Jacobs' *Nervous System*, Larry Gottheim, Nicholas Ray's *We Can't Go Home Again*), CinemaKnot unites tactile media with computational structures:
* **Duration as Material:** Time is not compressed into a thumbnail or quick summary; students interact with the actual speed, friction, and duration of spinning discs, rotating tape reels, and vinyl grooves.
* **The "Knot" vs. "Follow the Leader":** Standard interactive media often relies on simple 1:1 mappings (e.g. *louder audio = brighter light*). In CinemaKnot, multidimensional vectors form **knots**—interconnected neighborhoods in feature space where multiple physical dimensions (spectral centroid, noise flatness, spatial light mass, edge complexity) negotiate the retrieval of film frames.

---

## 2. Hardware Topology & Wiring

```
                   ┌─────────────────────────────────────────┐
                   │        AIR-GAPPED LOCAL NETWORK         │
                   │        (Isolated Wi-Fi / UDP 8888)      │
                   └───────▲─────────────────────────▲───────┘
                           │                         │
            ┌──────────────┴──────────┐   ┌──────────┴──────────────┐
            │   ESP32 Tactile Node    │   │  Raspberry Pi 4 TV Node │
            │ (Turntable / Cassette)  │   │  (32x25 Mechanical TV)  │
            └─────────────────────────┘   └─────────────────────────┘
```

### Node A: The ESP32 Tactile Node (Turntable & Cassette)
* **Microcontroller:** ESP32-WROOM-32 or ESP32 NodeMCU board.
* **Analog Input (ADC Pin 34 / 35):**
  * **Turntable:** Connect the output of a phono pre-amplifier or high-Z piezo contact mic mounted under the turntable platter.
  * **Cassette Player:** Connect the amplified output of the cassette read head or the audio line out.
  * *Voltage Note:* Ensure signals stay within the ESP32 ADC range (0.0 V to 3.3 V). Use a voltage divider or capacitor-coupled DC bias (1.65 V center).
* **Motor Control Output (PWM Pin 18):**
  * Logic-level N-channel MOSFET (e.g. IRLZ44N) or TB6612FNG motor driver driving the cassette player's DC motor. Allows the cinema database to manipulate tape playback speed dynamically.
* **Optical Strobe LED (Pin 2):**
  * Diagnostic LED flashing on transient onsets (needle drops, tape splices).

### Node B: The Raspberry Pi 4 Media Node (Mechanical TV)
* **Single Board Computer:** Raspberry Pi 4 Model B (4GB or 8GB).
* **Motor Driver:** Adafruit DC & Stepper Motor HAT (I2C address `0x60`).
* **Motor:** STEPPERONLINE NEMA 17 (1.8° step angle, 200 steps/rev) driving the 32-line Nipkow disc.
* **Index Sensor:** Slotted photoelectric interrupter on BCM GPIO 4 (Pin 7) detecting the once-per-revolution framing notch.
* **Optical Modulator:** 3W High-power LED heatsink mounted, driven via logic MOSFET on BCM GPIO 21.

---

## 3. Setting Up the Offline Local IoT Network

CinemaKnot **must not be connected to the internet**. You have two options for the local studio network:

### Option 1: Standalone Hardware Router (Recommended for Classrooms)
1. Procure any basic Wi-Fi router.
2. Unplug the WAN / internet cable entirely.
3. Configure the Wi-Fi network:
   * **SSID:** `CinemaKnotNet`
   * **Password:** `cinemaknotoffline`
   * **Subnet:** `192.168.1.x` (or `192.168.4.x`)
4. Connect the Raspberry Pi and all ESP32s to this network. All nodes will broadcast directly to `255.255.255.255:8888`.

### Option 2: Raspberry Pi as Local Access Point
Configure the Pi 4 to broadcast its own offline hotspot:
```bash
sudo nmcli con add type wifi ifname wlan0 mode ap con-name CinemaKnotHotspot ssid CinemaKnotNet
sudo nmcli con modify CinemaKnotHotspot 802-11-wireless.band bg 802-11-wireless.channel 6
sudo nmcli con modify CinemaKnotHotspot wifi-sec.key-mgmt wpa-psk wifi-sec.psk "cinemaknotoffline"
sudo nmcli con modify CinemaKnotHotspot ipv4.method shared ipv4.addresses "192.168.4.1/24"
sudo nmcli con up CinemaKnotHotspot
```

---

## 4. Classroom Exercises & Score-Based Performance

### Exercise 1: The Materiality Score (Needle Drops & Optical Phase)
* **Setup:** Place an ESP32 node on a turntable playing a locked groove or unrecorded vinyl lead-out.
* **The Task:** Students design an instruction score. Every tactile drop of the needle generates a sudden transient onset (`TYPE_ONSET`), physically rotating the Nipkow disc's optical framing phase by $45^\circ$.
* **Critical Inquiry:** How does the physical gesture of touching the needle puncture the continuity of the moving image? What happens when mechanical error interrupts cinematic illusion?

### Exercise 2: Tape Flutter vs. Spatial Light Mass
* **Setup:** Route a modified cassette player with a variable motor pot into the ESP32.
* **The Task:** Fast-forwarding or dragging a finger on the cassette reel alters the spectral centroid and RMS energy. The KD-tree queries an archive of structural 16mm films (e.g. flicker patterns, high-contrast leader, dense landscape shots).
* **Critical Inquiry:** The student is not watching a movie; they are navigating an archive spatially through friction and tape drag.

---

## 5. Compiling and Running

### On Raspberry Pi 4 / macOS:
```bash
cd Developer/cinemaknot
mkdir -p build && cd build
cmake ..
make -j4
./knot_test        # Verifies math, KD-tree, and networking
./pi_cinema_node   # Runs the cinema engine on UDP port 8888
```

### On ESP32:
1. Open [`examples/esp32_tactile_node.ino`](file:///Users/andrewfrangella/Developer/cinemaknot/examples/esp32_tactile_node.ino) in Arduino IDE or PlatformIO.
2. Ensure the `include/CinemaKnot` folder is in your include search path.
3. Select your ESP32 board and flash over USB.
4. Open the Serial Monitor at 115200 baud to view real-time network telemetry.
