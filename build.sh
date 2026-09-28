#!/usr/bin/env bash
# Сборка прошивки через arduino-cli: ./build.sh
# Результат - dist/GyverLamp-Wa1den-<версия>-d1_mini.bin, версия берётся из Version.h.
#
# Библиотеки берутся только из папки libs: часть из них пропатчена (см. README), и установленные
# в Документы/Arduino/libraries версии не должны попасть в сборку. Исходники копируются во временную
# папку с именем GyverLamp-Wa1den, потому что Arduino требует совпадения имени папки с главным .ino,
# а рабочая копия может называться иначе. Временная папка короткая: на длинном пути Windows сборка
# падает на файлах NeoPixelBus.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
SKETCH_NAME="GyverLamp-Wa1den"
FQBN="esp8266:esp8266:d1_mini:eesz=4M2M"                    # LOLIN(WEMOS) D1 R2 & mini, flash 4 МБ: FS 2 МБ, OTA ~1 МБ
CORE="esp8266:esp8266@3.1.2"
CORE_URL="https://arduino.esp8266.com/stable/package_esp8266com_index.json"
WORK="${TMPDIR:-/tmp}/glb"

VERSION="$(sed -n 's/^#define FIRMWARE_VERSION *"\([^"]*\)".*/\1/p' "$ROOT/Version.h")"
if [ -z "$VERSION" ]; then
  echo "Не найдена версия FIRMWARE_VERSION в Version.h" >&2
  exit 1
fi

if ! arduino-cli core list | grep -q "^esp8266:esp8266 *3\.1\.2 "; then
  arduino-cli core update-index --additional-urls "$CORE_URL"
  arduino-cli core install "$CORE" --additional-urls "$CORE_URL"
fi

rm -rf "$WORK/$SKETCH_NAME" "$WORK/out"
mkdir -p "$WORK/$SKETCH_NAME"
cp "$ROOT"/*.ino "$ROOT"/*.h "$WORK/$SKETCH_NAME/"

arduino-cli compile \
  --fqbn "$FQBN" \
  --libraries "$ROOT/libs" \
  --build-path "$WORK/build" \
  --output-dir "$WORK/out" \
  --warnings default \
  "$WORK/$SKETCH_NAME"

mkdir -p "$ROOT/dist"
BIN="$ROOT/dist/$SKETCH_NAME-$VERSION-d1_mini.bin"
cp "$WORK/out/$SKETCH_NAME.ino.bin" "$BIN"
echo "Готово: $BIN"
