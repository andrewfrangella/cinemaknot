#!/usr/bin/env python3
"""
CinemaKnot Central Nervous System Hub
Air-gapped real-time control, node discovery, routing matrix, and telemetry monitor.
Runs on macOS and Raspberry Pi.
"""

import asyncio
import json
import logging
import os
import socket
import struct
import time
from pathlib import Path
from aiohttp import web

logging.basicConfig(level=logging.INFO, format="[%(asctime)s] %(levelname)s %(message)s")
logger = logging.getLogger("CinemaKnotHub")

IOT_UDP_PORT = 8888
HTTP_PORT = 8880
PACKET_FORMAT = "<HBBBBHIfB8f"
PACKET_SIZE = struct.calcsize(PACKET_FORMAT) # 49 bytes
MAGIC = 0x4B4E # 'K', 'N'

ROLE_NAMES = {
    0x00: "Generic Node",
    0x01: "Turntable",
    0x02: "Cassette Deck",
    0x03: "Mechanical TV",
    0x04: "Contact Mic",
    0x05: "Audio I/O",
    0x06: "Camera Scanner",
    0x10: "Central Hub"
}

TYPE_NAMES = {
    0x01: "QUERY",
    0x02: "RESULT",
    0x03: "ONSET",
    0x04: "TELEMETRY",
    0x05: "SYNC",
    0x06: "HEARTBEAT",
    0x07: "ROUTE_CONFIG",
    0x08: "CONTROL"
}

class CentralNervousSystem:
    def __init__(self):
        self.nodes = {}  # node_id -> dict
        self.routes = [
            # Default starter route: Node 1 (Turntable) Centroid -> Node 10 (Mechanical TV) Phase
            {
                "id": "route-default-1",
                "src_node": 1,
                "src_param": "centroid",
                "dst_node": 10,
                "dst_param": "phase",
                "weight": 1.0,
                "active": True
            }
        ]
        self.ws_clients = set()
        self.udp_sock = None
        self.packet_count = 0
        self.last_packet_time = 0

    def init_udp(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, "SO_REUSEPORT"):
            try:
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            except OSError:
                pass
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.setblocking(False)
        sock.bind(("0.0.0.0", IOT_UDP_PORT))
        self.udp_sock = sock
        logger.info(f"UDP listener bound to 0.0.0.0:{IOT_UDP_PORT} (Broadcast enabled)")

    def parse_packet(self, data: bytes, addr):
        if len(data) < PACKET_SIZE:
            return None

        magic, ver, p_type, src_id, dst_id, seq, matched_id, dist, v_dim, *v_data = struct.unpack(
            PACKET_FORMAT, data[:PACKET_SIZE]
        )

        if magic != MAGIC:
            return None

        ip, port = addr
        now = time.time()
        self.packet_count += 1
        self.last_packet_time = now

        # Extract label if heartbeat, or vector
        label = ""
        if p_type == 0x06:  # HEARTBEAT
            raw_label = data[17:49]
            label = raw_label.decode("utf-8", errors="ignore").split("\x00")[0].strip()

        node_info = self.nodes.get(src_id)
        if not node_info:
            role_code = dst_id if p_type == 0x06 else 0x00
            node_info = {
                "id": src_id,
                "role_code": role_code,
                "role_name": ROLE_NAMES.get(role_code, "Generic Node"),
                "label": label or f"Node-{src_id}",
                "ip": ip,
                "port": port,
                "first_seen": now,
                "last_seen": now,
                "status": "ONLINE",
                "uptime_sec": matched_id if p_type == 0x06 else 0,
                "packet_seq": seq,
                "last_type": TYPE_NAMES.get(p_type, "UNKNOWN"),
                "telemetry": {
                    "rms": 0.0,
                    "centroid": 0.0,
                    "flatness": 0.0,
                    "zcr": 0.0,
                    "custom_val": 0.0
                },
                "onset_count": 0,
                "matched_id": 0,
                "distance": 0.0,
                "activity_pulse": False
            }
            self.nodes[src_id] = node_info
            logger.info(f"Discovered new node: ID={src_id} ({node_info['label']}) from {ip}:{port}")

        node_info["last_seen"] = now
        node_info["status"] = "ONLINE"
        node_info["packet_seq"] = seq
        node_info["last_type"] = TYPE_NAMES.get(p_type, "UNKNOWN")
        node_info["activity_pulse"] = True

        if label:
            node_info["label"] = label
        if p_type == 0x06:
            node_info["uptime_sec"] = matched_id
            if dst_id in ROLE_NAMES:
                node_info["role_code"] = dst_id
                node_info["role_name"] = ROLE_NAMES[dst_id]

        if p_type in (0x01, 0x04):  # QUERY or TELEMETRY
            if v_dim >= 1:
                node_info["telemetry"]["rms"] = round(v_data[0], 4)
            if v_dim >= 2:
                node_info["telemetry"]["zcr"] = round(v_data[1], 4)
            if v_dim >= 3:
                node_info["telemetry"]["centroid"] = round(v_data[2], 4)
            if v_dim >= 4:
                node_info["telemetry"]["flatness"] = round(v_data[3], 4)

        if p_type == 0x03:  # ONSET
            node_info["onset_count"] += 1

        if p_type == 0x02:  # RESULT
            node_info["matched_id"] = matched_id
            node_info["distance"] = round(dist, 4)

        return node_info

    def send_control(self, target_id: int, param_id: int, value: float):
        """Send TYPE_CONTROL packet to adjust remote node parameters."""
        if not self.udp_sock:
            return
        pkt = struct.pack(
            PACKET_FORMAT,
            MAGIC, 1, 0x08, 0x10, target_id, 0, param_id, value, 0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        )
        self.udp_sock.sendto(pkt, ("255.255.255.255", IOT_UDP_PORT))

    def trigger_onset(self, target_id: int):
        """Inject manual test transient onset across the IoT."""
        if not self.udp_sock:
            return
        pkt = struct.pack(
            PACKET_FORMAT,
            MAGIC, 1, 0x03, 0x10, target_id, 0, 0, 1.0, 0,
            0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        )
        self.udp_sock.sendto(pkt, ("255.255.255.255", IOT_UDP_PORT))

    def decay_nodes(self):
        """Mark nodes offline if no packet received within 3.5 seconds."""
        now = time.time()
        for node in self.nodes.values():
            if now - node["last_seen"] > 3.5:
                node["status"] = "OFFLINE"
            node["activity_pulse"] = False

    def get_state(self):
        return {
            "timestamp": time.time(),
            "packet_count": self.packet_count,
            "nodes": list(self.nodes.values()),
            "routes": self.routes
        }


hub = CentralNervousSystem()


async def udp_reader_task():
    loop = asyncio.get_running_loop()
    while True:
        try:
            data, addr = await loop.sock_recvfrom(hub.udp_sock, 512)
            hub.parse_packet(data, addr)
        except (BlockingIOError, InterruptedError):
            await asyncio.sleep(0.005)
        except Exception as e:
            logger.error(f"UDP reader error: {e}")
            await asyncio.sleep(0.01)


async def broadcast_ws_task():
    while True:
        await asyncio.sleep(0.04)  # ~25 Hz UI refresh rate
        hub.decay_nodes()
        if hub.ws_clients:
            msg = json.dumps(hub.get_state())
            coros = [ws.send_str(msg) for ws in list(hub.ws_clients)]
            await asyncio.gather(*coros, return_exceptions=True)


async def websocket_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)
    hub.ws_clients.add(ws)
    logger.info(f"WebSocket client connected from {request.remote}")

    # Immediately push current state
    await ws.send_str(json.dumps(hub.get_state()))

    try:
        async for msg in ws:
            if msg.type == web.WSMsgType.TEXT:
                data = json.loads(msg.data)
                action = data.get("action")
                if action == "add_route":
                    route = data.get("route")
                    if route:
                        hub.routes.append(route)
                        logger.info(f"Added route: {route}")
                elif action == "remove_route":
                    r_id = data.get("route_id")
                    hub.routes = [r for r in hub.routes if r.get("id") != r_id]
                    logger.info(f"Removed route: {r_id}")
                elif action == "trigger_onset":
                    tgt = int(data.get("target_id", 0))
                    hub.trigger_onset(tgt)
                elif action == "send_control":
                    tgt = int(data.get("target_id", 0))
                    param_id = int(data.get("param_id", 0))
                    val = float(data.get("value", 0.0))
                    hub.send_control(tgt, param_id, val)
    finally:
        hub.ws_clients.remove(ws)
        logger.info(f"WebSocket client disconnected ({request.remote})")

    return ws


def make_app():
    app = web.Application()
    static_dir = Path(__file__).resolve().parent / "static"
    app.router.add_get("/ws", websocket_handler)
    app.router.add_static("/", static_dir, show_index=True)
    return app


async def main():
    hub.init_udp()
    app = make_app()
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", HTTP_PORT)
    await site.start()

    logger.info("================================================================")
    logger.info(" CinemaKnot Central Nervous System Hub active!")
    logger.info(f" Open dashboard: http://127.0.0.1:{HTTP_PORT}")
    logger.info(f" Listening for ESP32 & Pi nodes on UDP port {IOT_UDP_PORT}")
    logger.info("================================================================")

    await asyncio.gather(
        udp_reader_task(),
        broadcast_ws_task()
    )


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Shutting down CinemaKnot Central Nervous System Hub...")
