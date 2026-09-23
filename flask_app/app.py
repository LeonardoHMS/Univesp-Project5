from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, jsonify, render_template, request

app = Flask(__name__)
BASE_DIR = Path(__file__).resolve().parent
STATE_FILE = BASE_DIR / "state.json"

DEFAULT_STATE = {
    "moisture": 0,
    "pump_running": False,
    "reservoir_empty": False,
    "last_irrigation": None,
    "last_update": None,
    "alert_message": "Sistema pronto.",
    "target_moisture": 60,
}

STATE = DEFAULT_STATE.copy()


def now_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def save_state() -> None:
    STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    with STATE_FILE.open("w", encoding="utf-8") as file:
        json.dump(STATE, file, indent=2)


def load_state() -> None:
    global STATE
    if STATE_FILE.exists():
        try:
            with STATE_FILE.open("r", encoding="utf-8") as file:
                loaded = json.load(file)
            STATE.update(DEFAULT_STATE)
            STATE.update(loaded)
        except (json.JSONDecodeError, OSError):
            STATE = DEFAULT_STATE.copy()
    save_state()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/status", methods=["GET"])
def api_status():
    payload = {
        **STATE,
        "last_update": STATE.get("last_update") or "N/A",
    }
    return jsonify(payload)


@app.route("/api/esp32/status", methods=["GET"])
def esp32_status():
    payload = {
        "reservoir_empty": bool(STATE.get("reservoir_empty", False)),
        "target_moisture": int(STATE.get("target_moisture", 60)),
        "last_irrigation": STATE.get("last_irrigation"),
        "alert_message": STATE.get("alert_message", "Sistema pronto."),
    }
    return jsonify(payload)


@app.route("/api/esp32/update", methods=["POST"])
def esp32_update():
    global STATE

    payload = request.get_json(silent=True) or {}

    previous_pump = bool(STATE.get("pump_running", False))

    if "moisture" in payload:
        STATE["moisture"] = int(payload["moisture"])

    if "pump_running" in payload:
        STATE["pump_running"] = bool(payload["pump_running"])

    if "reservoir_empty" in payload:
        STATE["reservoir_empty"] = bool(payload["reservoir_empty"])

    if "alert_message" in payload and payload["alert_message"]:
        STATE["alert_message"] = str(payload["alert_message"])

    if payload.get("reservoir_empty") is True:
        STATE["alert_message"] = "Reservatório vazio. Encha o reservatório para retomar a irrigação."

    if previous_pump and not STATE["pump_running"]:
        STATE["last_irrigation"] = now_iso()
        STATE["alert_message"] = "Ciclo de irrigação concluído."

    STATE["last_update"] = now_iso()
    save_state()

    return jsonify({"ok": True, "state": STATE})


@app.route("/api/reservoir/filled", methods=["POST"])
def reservoir_filled():
    STATE["reservoir_empty"] = False
    STATE["alert_message"] = "Reservatório reabastecido. O sistema pode retomar a irrigação."
    STATE["last_update"] = now_iso()
    save_state()
    return jsonify({"ok": True, "reservoir_empty": False})


@app.route("/api/reservoir/empty", methods=["POST"])
def reservoir_empty():
    STATE["reservoir_empty"] = True
    STATE["alert_message"] = "Reservatório vazio. Encha o reservatório para retomar a irrigação."
    STATE["last_update"] = now_iso()
    save_state()
    return jsonify({"ok": True, "reservoir_empty": True})


if __name__ == "__main__":
    load_state()
    app.run(host="0.0.0.0", port=5000, debug=True)
