#!/usr/bin/env python3
"""
CinemaKnot Node Simulator
Emulates ESP32 and Pi nodes broadcasting over the air-gapped UDP network.
Use this to test the Central Nervous System Hub on macOS or Pi.
"""

import socket
import struct
import time
import math
import random

IOT_PORT = 8888
PACKET_FORMAT = "<HBBBBHIfB8f"
MAGIC = 0x4B4E

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)

print("=== CinemaKnot Node Simulator ===")
print(f"Emulating Turntable ESP32 (Node 1), Cassette ESP32 (Node 2), and Mechanical TV (Node 10)...")
print(f"Broadcasting to 127.0.0.1:{IOT_PORT} and 255.255.255.255:{IOT_PORT}")
print("Press Ctrl+C to stop.\n")

seq = 0
start_time = time.time()

try:
    while True:
        now = time.time()
        elapsed = now - start_time
        seq += 1

        # 1. Turntable ESP32 (Node 1)
        # Periodic heartbeat
        if seq % 15 == 0:
            hb1 = struct.pack(
                PACKET_FORMAT,
                MAGIC, 1, 0x06, 1, 0x01, seq, int(elapsed), 0.0, 0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
            )
            # Inject label into the packet's tail
            hb1_list = bytearray(hb1)
            label1 = b"Turntable (ESP32)\x00"
            hb1_list[17:17+len(label1)] = label1
            sock.sendto(hb1_list, ("127.0.0.1", IOT_PORT))

        # Turntable audio stream (simulating vinyl surface noise & scratches)
        rms1 = 0.15 + 0.1 * math.sin(elapsed * 2.0) + random.uniform(0.0, 0.05)
        centroid1 = 0.4 + 0.3 * math.sin(elapsed * 0.8)
        flatness1 = 0.6 + 0.2 * math.cos(elapsed * 1.5)
        pkt1 = struct.pack(
            PACKET_FORMAT,
            MAGIC, 1, 0x01, 1, 0, seq, 0, 0.0, 4,
            rms1, 0.2, centroid1, flatness1, 0.0, 0.0, 0.0, 0.0
        )
        sock.sendto(pkt1, ("127.0.0.1", IOT_PORT))

        # Occasional needle drop onset
        if random.random() < 0.08:
            onset = struct.pack(
                PACKET_FORMAT,
                MAGIC, 1, 0x03, 1, 0, seq, 0, 1.0, 0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
            )
            sock.sendto(onset, ("127.0.0.1", IOT_PORT))

        # 2. Cassette Deck ESP32 (Node 2)
        if seq % 15 == 0:
            hb2 = struct.pack(
                PACKET_FORMAT,
                MAGIC, 1, 0x06, 2, 0x02, seq, int(elapsed), 0.0, 0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
            )
            hb2_list = bytearray(hb2)
            label2 = b"Cassette Deck #1\x00"
            hb2_list[17:17+len(label2)] = label2
            sock.sendto(hb2_list, ("127.0.0.1", IOT_PORT))

        rms2 = 0.3 + 0.15 * math.sin(elapsed * 1.2)
        centroid2 = 0.25 + 0.1 * math.cos(elapsed * 2.5)
        flatness2 = 0.4 + 0.1 * math.sin(elapsed * 0.5)
        pkt2 = struct.pack(
            PACKET_FORMAT,
            MAGIC, 1, 0x01, 2, 0, seq, 0, 0.0, 4,
            rms2, 0.35, centroid2, flatness2, 0.0, 0.0, 0.0, 0.0
        )
        sock.sendto(pkt2, ("127.0.0.1", IOT_PORT))

        # 3. Mechanical TV Raspberry Pi (Node 10)
        if seq % 15 == 0:
            hb10 = struct.pack(
                PACKET_FORMAT,
                MAGIC, 1, 0x06, 10, 0x03, seq, int(elapsed), 0.0, 0,
                0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
            )
            hb10_list = bytearray(hb10)
            label10 = b"Nipkow TV (Pi 4)\x00"
            hb10_list[17:17+len(label10)] = label10
            sock.sendto(hb10_list, ("127.0.0.1", IOT_PORT))

        # Result frame match
        match_frame = int((math.sin(elapsed) + 1.0) * 150)
        res_dist = 0.12 + 0.08 * math.cos(elapsed)
        res_pkt = struct.pack(
            PACKET_FORMAT,
            MAGIC, 1, 0x02, 10, 0, seq, match_frame, res_dist, 0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        )
        sock.sendto(res_pkt, ("127.0.0.1", IOT_PORT))

        time.sleep(0.1)

except KeyboardInterrupt:
    print("\nSimulator stopped.")
