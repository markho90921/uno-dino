
import ctypes
import json
import os
from pathlib import Path
import queue
import threading
import time
import urllib.request
import winsound


URL = "http://127.0.0.1:7000"

VK_SPACE = 0x20
VK_ESCAPE = 0x1B

user32 = ctypes.windll.user32

SOUND_ENABLED = True

SAVE_DIR = (
    Path(os.getenv("LOCALAPPDATA", str(Path.home())))
    / "UNO_Dino"
)

SAVE_FILE = SAVE_DIR / "highscore.json"


# Sound effects: (frequency, duration)
SOUNDS = {
    1: [(440, 60), (660, 80)],
    2: [(660, 45)],
    3: [(440, 60), (660, 60), (880, 100)],
    4: [(380, 100), (220, 180)],
    5: [(520, 45), (360, 50)],
}


# ==============================
# High score storage
# ==============================

def load_best():
    try:
        return max(
            0,
            min(
                999,
                int(json.loads(SAVE_FILE.read_text())["best"])
            )
        )

    except (OSError, ValueError, TypeError, KeyError):
        return 0


def save_best(best):
    SAVE_DIR.mkdir(parents=True, exist_ok=True)

    temporary = SAVE_FILE.with_suffix(".tmp")

    temporary.write_text(
        json.dumps({"best": best}),
        encoding="utf-8"
    )

    temporary.replace(SAVE_FILE)


# ==============================
# HTTP communication
# ==============================

def request(path, method="GET"):

    req = urllib.request.Request(
        URL + path,
        data=b"" if method == "POST" else None,
        method=method
    )

    with urllib.request.urlopen(req, timeout=2) as response:
        return json.load(response)


# ==============================
# Keyboard input
# ==============================

def pressed(key):
    return bool(
        user32.GetAsyncKeyState(key) & 0x8000
    )


# ==============================
# Sound worker
# ==============================

def sound_worker(events):

    while True:

        event = events.get()

        if event is None:
            return

        if SOUND_ENABLED:

            for frequency, duration in SOUNDS.get(event, []):

                try:
                    winsound.Beep(frequency, duration)

                except RuntimeError:
                    break


# ==============================
# Main controller
# ==============================

def main():

    best = load_best()
    last_sequence = None

    events = queue.Queue()

    threading.Thread(
        target=sound_worker,
        args=(events,),
        daemon=True
    ).start()

    synced = False
    next_poll = 0.0
    retry_after = 0.0

    previous_space = pressed(VK_SPACE)

    print("UNO DINO")
    print("SPACE: Start / Jump / Score / Restart")
    print("ESC: Exit")
    print("High score:", best)
    print("The UNO Q LED matrix is the game display.")
    print("Sound: PC speakers.")

    while not pressed(VK_ESCAPE):

        current_space = pressed(VK_SPACE)

        now = time.monotonic()

        edge = current_space and not previous_space

        previous_space = current_space

        if now >= retry_after:

            try:

                # Send keyboard command
                if edge:
                    request("/jump", "POST")
                    next_poll = 0.0

                # Poll game status
                if now >= next_poll:

                    status = request("/status")

                    board_best = int(status["best"])

                    # Synchronize saved high score
                    if not synced:

                        try:

                            if board_best < best:

                                board_best = int(
                                    request(
                                        "/record?value=" + str(best),
                                        "POST"
                                    )["best"]
                                )

                            synced = True

                        except (OSError, ValueError, KeyError) as exc:

                            print("Record sync unavailable:", exc)

                            synced = True

                    # Save new high score
                    if board_best > best:

                        best = board_best

                        try:
                            save_best(best)

                        except OSError as exc:
                            print("Could not save high score:", exc)

                        print("New high score:", best)

                    # Play sound events
                    sequence = int(status["sequence"])

                    if (
                        last_sequence is not None
                        and sequence != last_sequence
                    ):
                        events.put(int(status["sound"]))

                    last_sequence = sequence

                    next_poll = time.monotonic() + 0.12

            except (OSError, ValueError, KeyError) as exc:

                print("Connection error:", exc)

                synced = False
                last_sequence = None

                retry_after = time.monotonic() + 1.0

        time.sleep(0.01)

    events.put(None)

    print("Controller closed.")


if __name__ == "__main__":

    try:
        main()

    except KeyboardInterrupt:
        print("\nController closed.")
