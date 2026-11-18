# UNO Dino v1.0

An endless runner for the Arduino UNO Q's built-in 8 × 13 LED matrix, inspired by Chrome Dino. The PC is a keyboard controller and sound output, not the game display. Developed for an AIoT midterm project.

## Features

- Space to start, jump, show score, or restart; Esc exits the Windows controller.
- Solid LED ground; player jumps three cells, with about 1.05 seconds airborne.
- Four obstacle types: normal, tall, flying, and two-cell-wide, with random spacing.
- One point for each obstacle passed. Movement interval starts at 300 ms and decreases by 20 ms per three points, to a minimum of 140 ms.
- Blinking power-up scrolls from right to left; collection activates flight on the second row and triples the current scroll speed for 6.5 seconds. The player blinks in the last second and waits for a safe landing.
- Game Over X transitions to the score after 1.5 seconds or on Space; score and high score alternate. Another Space starts a new round.
- High scores stored locally on Windows and synchronized with the board; sound effects play on the PC.

## Architecture

Windows `keyboard.py` (keyboard, sound, high-score storage) → USB ADB port forward (HTTP 7000) → UNO Q Linux `main.py` (WebUI API) → Arduino Router Bridge RPC → STM32 MCU `sketch.ino` (game state, timing, collision, rendering) → LED matrix.

## Files

- `sketch.ino`: MCU game logic
- `main.py`: Linux-side HTTP-to-RPC service
- `keyboard.py`: Windows controller, sound and local record
- `app.yaml`: Arduino App configuration
- `.gitignore`: excludes common local and secret files

## Setup and run

Requirements: Arduino UNO Q with Arduino App CLI installed, Windows PC with Python 3 and ADB, USB data cable, SSH access to the board. The Windows controller uses only Python standard-library modules. Tested by the project author with App CLI 0.13.0.

1. Connect the board using USB. In Windows PowerShell, locate ADB (if not on PATH):

```powershell
$adb = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\arduino\tools\adb" -Recurse -Filter adb.exe | Select-Object -First 1 -ExpandProperty FullName
& $adb devices -l
& $adb forward tcp:2222 tcp:22
& $adb forward tcp:7000 tcp:7000
ssh -p 2222 arduino@127.0.0.1
```

2. On the board, create an app **only if it does not already exist**. Its generated `sketch/sketch.yaml` supplies the sketch profile. If updating an existing app, stop it first.

```bash
arduino-app-cli app new uno-dino
```

3. In a separate Windows PowerShell, from this repository directory:

```powershell
scp -P 2222 .\sketch.ino arduino@127.0.0.1:/home/arduino/ArduinoApps/uno-dino/sketch/sketch.ino
scp -P 2222 .\main.py arduino@127.0.0.1:/home/arduino/ArduinoApps/uno-dino/python/main.py
scp -P 2222 .\app.yaml arduino@127.0.0.1:/home/arduino/ArduinoApps/uno-dino/app.yaml
```

4. On the board:

```bash
arduino-app-cli app start user:uno-dino
```

5. On Windows:

```powershell
python keyboard.py
```

Press Space when RUN appears. End the controller with Esc and stop the board app with `arduino-app-cli app stop user:uno-dino`.

The controller listens for Space and Esc globally while active. Local record file: `%LOCALAPPDATA%\UNO_Dino\highscore.json`. Set `SOUND_ENABLED = False` in `keyboard.py` to mute. Sound polling may miss rapid successive events.

## Development and verification

The student designed the gameplay, tuned jump/boost timing and screen layout, deployed to UNO Q, and tested successive iterations on the physical board. ChatGPT assisted with implementation and troubleshooting. The final package is reconstructed from the previously supplied polished source with the solid-ground change; compare with the local tested source before publishing if further changes were made. No passwords or API keys are included.
