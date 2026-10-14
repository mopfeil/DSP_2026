#!/usr/bin/env bash
# Create/update the Wokwi for VS Code copies of the Wokwi projects:
#
#   code/wokwi/<proj>/            -> code/wokwi_vscode/<proj>/
#   code/solutions/wokwi/<proj>/  -> code/solutions/wokwi_vscode/<proj>/
#
# Copied:    sketch.ino and *.h -> src/, diagram.json
# Generated: platformio.ini (board from diagram.json), wokwi.toml (chips from diagram.json)
# Not copied: *.chip.c/json (VS Code uses the prebuilt chips/*.chip.wasm),
#             other *.c files (PC helper programs, e.g. tooth_sim.c)
#
#   ./sync.sh          create/update changed files
#   ./sync.sh --check  only report differences, exit 1 if any
#
# code/wokwi/ and code/solutions/wokwi/ stay the master copies.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
code="$(cd "$here/.." && pwd)"
check=0
[[ "${1:-}" == "--check" ]] && check=1
status=0

# Pinned pioarduino release = Arduino-ESP32 core 3.3.x (the sketches use the
# core-3 API, e.g. ledcAttach; the official espressif32 platform ships core 2).
ESP32_PLATFORM="https://github.com/pioarduino/platform-espressif32/releases/download/55.03.312/platform-espressif32.zip"

# put <content> <file> <label>: write if different (or report with --check)
put() {
  local content="$1" to="$2" label="$3"
  if [[ -f "$to" ]] && [[ "$(cat "$to")" == "$content" ]]; then return; fi
  status=1
  if (( check )); then echo "differs: $label"; return; fi
  mkdir -p "$(dirname "$to")"
  printf '%s\n' "$content" > "$to"
  echo "updated: $label"
}

platformio_ini() {  # <rel-src> <board: uno|esp32>
  echo "; Build for Wokwi for VS Code (see code/wokwi_vscode/README.md)."
  echo "; Generated copy of $1 -- edit there and run code/wokwi_vscode/sync.sh."
  if [[ "$2" == esp32 ]]; then
    echo "[env:esp32]"
    echo "platform = $ESP32_PLATFORM"
    echo "board = esp32dev"
  else
    echo "[env:uno]"
    echo "platform = atmelavr"
    echo "board = uno"
  fi
  echo "framework = arduino"
  echo "monitor_speed = 115200"
}

wokwi_toml() {  # <board> <chips-dir relative to project> <chip names...>
  local board="$1" chipdir="$2"; shift 2
  local fw
  [[ "$board" == esp32 ]] && fw=".pio/build/esp32/firmware.bin" || fw=".pio/build/uno/firmware.hex"
  echo "# Wokwi for VS Code. Build first (PlatformIO: Build, or \"pio run\"),"
  echo "# then F1 -> \"Wokwi: Start Simulator\". See code/wokwi_vscode/README.md."
  echo "[wokwi]"
  echo "version = 1"
  echo "firmware = '$fw'"
  echo "elf = '.pio/build/$board/firmware.elf'"
  echo "# Logic analyzer capture, written when the simulation stops"
  echo "vcdFile = 'wokwi.vcd'"
  echo "# For GDB debugging (\"Wokwi: Start Simulator and Wait for Debugger\")"
  echo "gdbServerPort = 3333"
  local c
  for c in "$@"; do
    echo
    echo "# Custom chip, prebuilt from code/wokwi/chips/$c.chip.c (see chips/Makefile)"
    echo "[[chip]]"
    echo "name = '$c'"
    echo "binary = '$chipdir/$c.chip.wasm'"
  done
}

sync_tree() {  # <src root> <dst root> <chips dir relative to a project in dst>
  local src_root="$1" dst_root="$2" chipdir="$3" src ex dst board f chips
  for src in "$src_root"/*/; do
    ex="$(basename "$src")"
    [[ -f "$src/diagram.json" ]] || continue
    dst="$dst_root/$ex"
    grep -q 'wokwi-esp32' "$src/diagram.json" && board=esp32 || board=uno
    chips=$( (grep -o '"chip-[a-z0-9-]*"' "$src/diagram.json" || true) | tr -d '"' | sed 's/^chip-//' | sort -u)

    for f in "$src"/sketch.ino "$src"/*.h; do
      [[ -f "$f" ]] || continue
      put "$(cat "$f")" "$dst/src/$(basename "$f")" "${dst#$code/}/src/$(basename "$f")"
    done
    put "$(cat "$src/diagram.json")" "$dst/diagram.json" "${dst#$code/}/diagram.json"
    put "$(platformio_ini "code/${src_root#$code/}/$ex" "$board")" "$dst/platformio.ini" "${dst#$code/}/platformio.ini"
    # shellcheck disable=SC2086
    put "$(wokwi_toml "$board" "$chipdir" $chips)" "$dst/wokwi.toml" "${dst#$code/}/wokwi.toml"

    # files in src/ that no longer exist in the master copy
    for f in "$dst"/src/*; do
      if [[ -f "$f" && ! -f "$src/$(basename "$f")" ]]; then
        echo "stale: ${f#$code/}" >&2; status=1
      fi
    done
  done
}

sync_tree "$code/wokwi"           "$code/wokwi_vscode"           "../chips"
sync_tree "$code/solutions/wokwi" "$code/solutions/wokwi_vscode" "../../../wokwi_vscode/chips"

# prebuilt chips up to date with the generated chip sources?
for c in engine-sim lambda-probe; do
  if [[ "$(md5sum < "$code/wokwi/chips/$c.chip.c" | cut -d' ' -f1)" != "$(cat "$here/chips/$c.chip.src.md5" 2>/dev/null)" ]]; then
    echo "outdated: wokwi_vscode/chips/$c.chip.wasm -- run 'make WASI_SDK=...' in code/wokwi_vscode/chips" >&2
    status=1
  fi
  cmp -s "$code/wokwi/chips/$c.chip.json" "$here/chips/$c.chip.json" ||
    { echo "outdated: wokwi_vscode/chips/$c.chip.json -- run make in code/wokwi_vscode/chips" >&2; status=1; }
done

if (( check )); then exit $status; fi
(( status )) || echo "all Wokwi for VS Code copies up to date"
