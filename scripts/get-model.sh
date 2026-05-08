#!/usr/bin/env bash
set -e

MODEL=${1:-base.en}   # base.en | small.en | base | small
OUTDIR="$(dirname "$0")/../models"
mkdir -p "$OUTDIR"

BASE_URL="https://huggingface.co/ggerganov/whisper.cpp/resolve/main"
FILE="ggml-${MODEL}.bin"

if [[ -f "$OUTDIR/$FILE" ]]; then
    echo "Already have $OUTDIR/$FILE"
    exit 0
fi

echo "Downloading $FILE ..."
curl -L --progress-bar -o "$OUTDIR/$FILE" "$BASE_URL/$FILE"
echo "Saved to $OUTDIR/$FILE"
