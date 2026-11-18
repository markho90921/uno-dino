
from arduino.app_utils import App, Bridge
from arduino.app_bricks.web_ui import WebUI
from threading import Lock

ui = WebUI()
rpc_lock = Lock()


def jump():
    with rpc_lock:
        return {"jumped": Bridge.call("jump")}


def status():
    with rpc_lock:
        packed = int(Bridge.call("get_status"))

    return {
        "best": packed % 1000,
        "sound": (packed // 1000) % 10,
        "sequence": packed // 10000
    }


def record(value: int):
    with rpc_lock:
        return {
            "best": int(
                Bridge.call(
                    "set_best",
                    max(0, min(999, value))
                )
            )
        }


ui.expose_api("POST", "/jump", jump)
ui.expose_api("GET", "/status", status)
ui.expose_api("POST", "/record", record)

App.run()
