/**
 * CinemaKnot Central Nervous System Dashboard
 * Interactive Node Topology, Real-time Patch Matrix, and Telemetry Visualizer
 */

class CentralNervousSystemUI {
  constructor() {
    this.ws = null;
    this.nodes = new Map(); // id -> node object
    this.routes = [];
    this.selectedNodeId = null;
    this.nodePositions = new Map(); // id -> {x, y}

    this.canvas = document.getElementById("patchCanvas");
    this.ctx = this.canvas.getContext("2d");
    this.overlay = document.getElementById("nodesOverlay");
    this.container = document.getElementById("canvasContainer");

    this.draggedNodeId = null;
    this.dragOffset = { x: 0, y: 0 };
    this.animationFrameId = null;
    this.particlePhase = 0;

    this.initElements();
    this.initCanvasResize();
    this.connectWebSocket();
    this.startRenderLoop();
  }

  initElements() {
    // Buttons
    document.getElementById("btnAutoArrange").addEventListener("click", () => this.autoArrangeNodes());
    document.getElementById("btnOpenPatchModal").addEventListener("click", () => this.openPatchModal());
    document.getElementById("btnClosePatchModal").addEventListener("click", () => this.closePatchModal());
    document.getElementById("btnCancelPatch").addEventListener("click", () => this.closePatchModal());
    document.getElementById("btnSavePatch").addEventListener("click", () => this.saveNewRoute());

    // Window drag handlers
    window.addEventListener("mousemove", (e) => this.onDragMove(e));
    window.addEventListener("mouseup", () => this.onDragEnd());
  }

  initCanvasResize() {
    const resize = () => {
      const rect = this.container.getBoundingClientRect();
      this.canvas.width = rect.width * window.devicePixelRatio;
      this.canvas.height = rect.height * window.devicePixelRatio;
      this.ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    };
    window.addEventListener("resize", resize);
    setTimeout(resize, 50);
  }

  connectWebSocket() {
    const proto = window.location.protocol === "https:" ? "wss:" : "ws:";
    const url = `${proto}//${window.location.host}/ws`;

    const statusChip = document.getElementById("hubStatusChip");
    const statusText = document.getElementById("hubStatusText");

    this.ws = new WebSocket(url);

    this.ws.onopen = () => {
      statusChip.querySelector(".status-dot").style.background = "var(--accent-green)";
      statusText.textContent = "ONLINE (AIR-GAPPED)";
    };

    this.ws.onmessage = (event) => {
      try {
        const state = JSON.parse(event.data);
        this.updateState(state);
      } catch (err) {
        console.error("WS Parse error:", err);
      }
    };

    this.ws.onclose = () => {
      statusChip.querySelector(".status-dot").style.background = "var(--accent-red)";
      statusText.textContent = "DISCONNECTED";
      setTimeout(() => this.connectWebSocket(), 1500);
    };
  }

  updateState(state) {
    document.getElementById("nodeCount").textContent = state.nodes.length;
    document.getElementById("routeCount").textContent = state.routes.length;
    document.getElementById("packetCount").textContent = state.packet_count;

    this.routes = state.routes;

    // Track active nodes
    const currentIds = new Set();
    state.nodes.forEach((n) => {
      currentIds.add(n.id);
      this.nodes.set(n.id, n);

      // Default placement if new
      if (!this.nodePositions.has(n.id)) {
        const isSink = n.role_code === 3 || n.id >= 10; // TV / Output
        const x = isSink ? 520 : 60 + (this.nodePositions.size % 3) * 40;
        const y = 60 + (this.nodePositions.size * 140) % 360;
        this.nodePositions.set(n.id, { x, y });
      }

      this.renderOrUpdateNodeCard(n);
    });

    // Remove defunct DOM nodes
    for (const [id] of this.nodes) {
      if (!currentIds.has(id)) {
        const el = document.getElementById(`node-card-${id}`);
        if (el) el.remove();
        this.nodes.delete(id);
      }
    }

    this.renderPatchbayList();
    this.renderInspector();
  }

  renderOrUpdateNodeCard(node) {
    let card = document.getElementById(`node-card-${node.id}`);
    const pos = this.nodePositions.get(node.id);

    if (!card) {
      card = document.createElement("div");
      card.id = `node-card-${node.id}`;
      card.className = "node-card";
      card.style.left = `${pos.x}px`;
      card.style.top = `${pos.y}px`;

      card.addEventListener("mousedown", (e) => this.onDragStart(e, node.id));
      card.addEventListener("click", () => this.selectNode(node.id));

      this.overlay.appendChild(card);
    }

    if (this.selectedNodeId === node.id) {
      card.classList.add("selected");
    } else {
      card.classList.remove("selected");
    }

    const t = node.telemetry || {};
    const rmsWidth = Math.min(100, (t.rms || 0) * 100);
    const centroidWidth = Math.min(100, (t.centroid || 0) * 100);
    const flatnessWidth = Math.min(100, (t.flatness || 0) * 100);

    const isOnline = node.status === "ONLINE";

    card.innerHTML = `
      <div class="node-header">
        <div class="node-title-group">
          <span class="node-name">${node.label}</span>
          <span class="node-role">${node.role_name} (ID: ${node.id})</span>
        </div>
        <span class="node-badge ${isOnline ? "online" : "offline"}">
          ${isOnline ? "LIVE" : "OFFLINE"}
        </span>
      </div>
      <div class="node-body">
        <div class="telemetry-row">
          <div class="telemetry-meta">
            <span>RMS LOUDNESS</span>
            <span>${(t.rms || 0).toFixed(2)}</span>
          </div>
          <div class="meter-track">
            <div class="meter-fill" style="width: ${rmsWidth}%"></div>
          </div>
        </div>
        <div class="telemetry-row">
          <div class="telemetry-meta">
            <span>SPECTRAL CENTROID</span>
            <span>${(t.centroid || 0).toFixed(2)}</span>
          </div>
          <div class="meter-track">
            <div class="meter-fill" style="width: ${centroidWidth}%"></div>
          </div>
        </div>
        <div class="telemetry-row">
          <div class="telemetry-meta">
            <span>SPECTRAL FLATNESS</span>
            <span>${(t.flatness || 0).toFixed(2)}</span>
          </div>
          <div class="meter-track">
            <div class="meter-fill flatness" style="width: ${flatnessWidth}%"></div>
          </div>
        </div>
        <div class="node-ports">
          <span class="port-tag"><span class="port-socket active"></span> IN: ADC/SENSOR</span>
          <span class="port-tag">OUT: KNOT <span class="port-socket active"></span></span>
        </div>
      </div>
    `;
  }

  onDragStart(e, nodeId) {
    if (e.target.closest("button") || e.target.closest("input")) return;
    this.draggedNodeId = nodeId;
    const pos = this.nodePositions.get(nodeId);
    this.dragOffset.x = e.clientX - pos.x;
    this.dragOffset.y = e.clientY - pos.y;
    this.selectNode(nodeId);
  }

  onDragMove(e) {
    if (!this.draggedNodeId) return;
    const card = document.getElementById(`node-card-${this.draggedNodeId}`);
    if (!card) return;

    const newX = Math.max(10, e.clientX - this.dragOffset.x);
    const newY = Math.max(10, e.clientY - this.dragOffset.y);

    this.nodePositions.set(this.draggedNodeId, { x: newX, y: newY });
    card.style.left = `${newX}px`;
    card.style.top = `${newY}px`;
  }

  onDragEnd() {
    this.draggedNodeId = null;
  }

  selectNode(nodeId) {
    this.selectedNodeId = nodeId;
    document.querySelectorAll(".node-card").forEach((c) => c.classList.remove("selected"));
    const sel = document.getElementById(`node-card-${nodeId}`);
    if (sel) sel.classList.add("selected");
    this.renderInspector();
  }

  autoArrangeNodes() {
    let inputIdx = 0;
    let outputIdx = 0;

    for (const [id, node] of this.nodes) {
      const isOutput = node.role_code === 3 || id >= 10;
      if (isOutput) {
        this.nodePositions.set(id, { x: 550, y: 50 + outputIdx * 180 });
        outputIdx++;
      } else {
        this.nodePositions.set(id, { x: 60, y: 50 + inputIdx * 180 });
        inputIdx++;
      }
      const el = document.getElementById(`node-card-${id}`);
      if (el) {
        const pos = this.nodePositions.get(id);
        el.style.left = `${pos.x}px`;
        el.style.top = `${pos.y}px`;
      }
    }
  }

  renderPatchbayList() {
    const list = document.getElementById("patchbayList");
    if (!this.routes.length) {
      list.innerHTML = `<div class="empty-state">No active routes plugged into patch bay.</div>`;
      return;
    }

    list.innerHTML = this.routes
      .map((r) => {
        const srcNode = this.nodes.get(r.src_node) || { label: `Node ${r.src_node}` };
        const dstNode = this.nodes.get(r.dst_node) || { label: `Node ${r.dst_node}` };

        return `
        <div class="route-item">
          <div class="route-info">
            <span class="route-path">${r.src_param.toUpperCase()} ➔ ${r.dst_param.toUpperCase()}</span>
            <span class="route-nodes">${srcNode.label} ➔ ${dstNode.label}</span>
          </div>
          <button class="btn btn-danger" onclick="hubUI.removeRoute('${r.id}')">Unplug</button>
        </div>
      `;
      })
      .join("");
  }

  removeRoute(routeId) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ action: "remove_route", route_id: routeId }));
    }
  }

  renderInspector() {
    const container = document.getElementById("inspectorContent");
    if (!this.selectedNodeId || !this.nodes.has(this.selectedNodeId)) {
      container.innerHTML = `<div class="empty-state">Select a node on the canvas to inspect telemetry and remote controls.</div>`;
      return;
    }

    const n = this.nodes.get(this.selectedNodeId);
    const t = n.telemetry || {};

    container.innerHTML = `
      <table class="inspector-table">
        <tr><td>Node Label:</td><td><b>${n.label}</b></td></tr>
        <tr><td>Role:</td><td>${n.role_name}</td></tr>
        <tr><td>Hardware ID:</td><td>${n.id}</td></tr>
        <tr><td>Network IP:</td><td>${n.ip}:${n.port}</td></tr>
        <tr><td>Uptime:</td><td>${n.uptime_sec}s</td></tr>
        <tr><td>Packet Seq:</td><td>${n.packet_seq}</td></tr>
        <tr><td>Transient Hits:</td><td>${n.onset_count}</td></tr>
        <tr><td>RMS Energy:</td><td>${(t.rms || 0).toFixed(4)}</td></tr>
        <tr><td>Spectral Centroid:</td><td>${(t.centroid || 0).toFixed(4)}</td></tr>
        <tr><td>Spectral Flatness:</td><td>${(t.flatness || 0).toFixed(4)}</td></tr>
        <tr><td>Zero Crossing:</td><td>${(t.zcr || 0).toFixed(4)}</td></tr>
      </table>

      <div style="font-size: 11px; font-weight: 700; color: var(--text-muted); margin-bottom: 6px;">REMOTE STIMULUS & CALIBRATION</div>
      <div class="btn-row">
        <button class="btn btn-secondary" onclick="hubUI.triggerTestOnset(${n.id})">⚡ Inject Onset Pulse</button>
        <button class="btn btn-secondary" onclick="hubUI.sendRemoteControl(${n.id}, 1, 180.0)">Rotate Phase 180°</button>
      </div>
    `;
  }

  triggerTestOnset(targetId) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ action: "trigger_onset", target_id: targetId }));
    }
  }

  sendRemoteControl(targetId, paramId, value) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ action: "send_control", target_id: targetId, param_id: paramId, value: value }));
    }
  }

  openPatchModal() {
    const srcSelect = document.getElementById("modalSrcNode");
    const dstSelect = document.getElementById("modalDstNode");

    srcSelect.innerHTML = "";
    dstSelect.innerHTML = "";

    this.nodes.forEach((n) => {
      srcSelect.innerHTML += `<option value="${n.id}">${n.label} (ID ${n.id})</option>`;
      dstSelect.innerHTML += `<option value="${n.id}">${n.label} (ID ${n.id})</option>`;
    });

    document.getElementById("patchModal").classList.add("open");
  }

  closePatchModal() {
    document.getElementById("patchModal").classList.remove("open");
  }

  saveNewRoute() {
    const srcNode = parseInt(document.getElementById("modalSrcNode").value);
    const srcParam = document.getElementById("modalSrcParam").value;
    const dstNode = parseInt(document.getElementById("modalDstNode").value);
    const dstParam = document.getElementById("modalDstParam").value;

    const newRoute = {
      id: `route-${Date.now()}`,
      src_node: srcNode,
      src_param: srcParam,
      dst_node: dstNode,
      dst_param: dstParam,
      weight: 1.0,
      active: true
    };

    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify({ action: "add_route", route: newRoute }));
    }

    this.closePatchModal();
  }

  startRenderLoop() {
    const render = () => {
      this.drawPatchCables();
      this.particlePhase += 0.05;
      this.animationFrameId = requestAnimationFrame(render);
    };
    render();
  }

  drawPatchCables() {
    const rect = this.container.getBoundingClientRect();
    this.ctx.clearRect(0, 0, rect.width, rect.height);

    if (!this.routes.length) return;

    this.routes.forEach((route, idx) => {
      const srcPos = this.nodePositions.get(route.src_node);
      const dstPos = this.nodePositions.get(route.dst_node);

      if (!srcPos || !dstPos) return;

      // Start socket: right edge of source card
      const startX = srcPos.x + 250;
      const startY = srcPos.y + 110;

      // End socket: left edge of target card
      const endX = dstPos.x;
      const endY = dstPos.y + 110;

      const colors = ["#f59e0b", "#06b6d4", "#10b981", "#ec4899"];
      const cableColor = colors[idx % colors.length];

      this.ctx.beginPath();
      this.ctx.moveTo(startX, startY);

      // Bezier curve with drooping natural sag
      const cp1X = startX + Math.max(50, (endX - startX) * 0.4);
      const cp1Y = startY + 20;
      const cp2X = endX - Math.max(50, (endX - startX) * 0.4);
      const cp2Y = endY + 20;

      this.ctx.bezierCurveTo(cp1X, cp1Y, cp2X, cp2Y, endX, endY);
      this.ctx.strokeStyle = cableColor;
      this.ctx.lineWidth = 3;
      this.ctx.shadowColor = cableColor;
      this.ctx.shadowBlur = 8;
      this.ctx.stroke();
      this.ctx.shadowBlur = 0;

      // Animated energy particle traveling along the cable
      const t = (this.particlePhase + idx * 0.3) % 1.0;
      const px = this.bezierPoint(startX, cp1X, cp2X, endX, t);
      const py = this.bezierPoint(startY, cp1Y, cp2Y, endY, t);

      this.ctx.beginPath();
      this.ctx.arc(px, py, 4, 0, Math.PI * 2);
      this.ctx.fillStyle = "#ffffff";
      this.ctx.shadowColor = cableColor;
      this.ctx.shadowBlur = 12;
      this.ctx.fill();
      this.ctx.shadowBlur = 0;
    });
  }

  bezierPoint(p0, p1, p2, p3, t) {
    const mt = 1 - t;
    return (
      mt * mt * mt * p0 +
      3 * mt * mt * t * p1 +
      3 * mt * t * t * p2 +
      t * t * t * p3
    );
  }
}

// Global instance
let hubUI = null;
window.addEventListener("DOMContentLoaded", () => {
  hubUI = new CentralNervousSystemUI();
});
