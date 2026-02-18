#!/usr/bin/env bash
# About: Download NASA LOLA elevation data (CGI Moon Kit, SVS 4720).

set -euo pipefail

DEST="assets/terrain"
FILE="ldem_16.tif"
URL="https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/${FILE}"

mkdir -p "$DEST"

if [ -f "$DEST/$FILE" ]; then
    echo "$FILE already exists, skipping download."
    exit 0
fi

echo "Downloading $FILE (63 MB)..."
curl -L --progress-bar -o "$DEST/$FILE" "$URL"
echo "Done. Saved to $DEST/$FILE"
