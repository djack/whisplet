# whisplet

Push-to-talk speech-to-text that types into whatever window has focus.

Hold **F9** → speak → release. Transcribed text is typed at your cursor.

Runs [whisper.cpp](https://github.com/ggml-org/whisper.cpp) locally. No cloud, no API key.
GPU-accelerated on Linux (Vulkan) and macOS (Metal).

## Install

Download a binary from [Releases](https://github.com/djack/whisplet/releases), then:

```bash
./scripts/get-model.sh small.en   # ~500MB, or base.en for faster/smaller
./whisplet models/ggml-small.en.bin
```

> **macOS:** grant Accessibility access when prompted — required for the global hotkey.

## Build from source

See [BUILDING.md](BUILDING.md).
