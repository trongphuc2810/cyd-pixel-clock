#!/usr/bin/env bash
# Compile CYD Pixel Clock firmware and export firmware.bin
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "$0")" && pwd)"
NAME="cyd-pixel-clock"
BUILD_DIR="/tmp/${NAME}-build"
OUT_DIR="${SKETCH_DIR}/build"
FQBN="esp32:esp32:esp32:PartitionScheme=min_spiffs"

echo "==> Compiling ${NAME} with ${FQBN}..."
mkdir -p "${BUILD_DIR}" "${OUT_DIR}"

arduino-cli compile --fqbn "${FQBN}" --output-dir "${BUILD_DIR}" "${SKETCH_DIR}"

echo "==> Packaging firmware binaries..."
cp "${BUILD_DIR}/${NAME}.ino.bin" "${OUT_DIR}/firmware.bin"
cp "${BUILD_DIR}/${NAME}.ino.bootloader.bin" "${OUT_DIR}/bootloader.bin"
cp "${BUILD_DIR}/${NAME}.ino.partitions.bin" "${OUT_DIR}/partitions.bin"

# Discover boot_app0.bin
BOOT_APP0="$(find "$HOME/.arduino15/packages/esp32" -name boot_app0.bin -path '*partitions*' 2>/dev/null | head -1)"
if [ -n "${BOOT_APP0}" ]; then
    cp "${BOOT_APP0}" "${OUT_DIR}/boot_app0.bin"
fi

echo "==> Build complete! Output files in ${OUT_DIR}:"
ls -lh "${OUT_DIR}"
rm -rf "${BUILD_DIR}"
