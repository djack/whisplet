# Building Whisplet

## Quick start

```bash
make model        # download small.en model (~500MB), or: make model MODEL=base.en
make              # configure + build
make run          # run
make check        # format, lint, and static analysis
```

## Dependencies

### macOS
```bash
brew install portaudio cmake pkg-config
xcode-select --install   # if you don't have Xcode CLI tools
```

For `make check` (optional):
```bash
brew install llvm cppcheck   # provides clang-format, clang-tidy, cppcheck
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

### Linux (Ubuntu/Debian)
```bash
sudo apt install portaudio19-dev libx11-dev libvulkan-dev glslang-tools spirv-headers pkg-config cmake
```

For `make check` (optional):
```bash
sudo apt install clang-format clang-tidy cppcheck
```
For `glslc` (required for the Vulkan GPU backend):
```bash
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
sudo apt update && sudo apt install shaderc
```

---

## FAQ

### CMake can't find a package

Check what CMake actually found before looking at the full build error:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -E "Could NOT|Found|Error"
```

### Build errors are unreadable

Disable parallel builds and turn on verbose output so errors don't bury each other:
```bash
cmake --build build -j1 --verbose
```

### The app exits immediately on macOS

You need to grant Accessibility permission — `CGEventTap` (the global hotkey mechanism) requires it. The app prints a message telling you this if it's missing.

Quick check:
```bash
osascript -e 'tell application "System Events" to keystroke "test"'
```
If "test" appears somewhere, you're good. If it errors, go to **System Settings → Privacy & Security → Accessibility** and add your terminal or the whisplet binary.

### macOS: "no developer tools found" or `xcrun: error`
```bash
xcode-select --install
```

### macOS: app starts but crashes immediately (Apple Silicon)

Metal sometimes can't find its shader files relative to your working directory. Run from the repo root:
```bash
GGML_METAL_PATH_RESOURCES=./build ./build/whisplet models/ggml-small.en.bin
```

### Linux: "no GPU found" at startup

Vulkan is not picking up your GPU. Check:
```bash
vulkaninfo --summary
```
If that errors, your Vulkan driver isn't set up. For AMD: make sure `mesa-vulkan-drivers` is installed. For NVIDIA: install the proprietary driver.

### Transcription is slow

You're probably running on CPU. Check the startup output for:
- macOS: `ggml_metal: GPU name: ...`
- Linux: `ggml_vulkan: Found 1 Vulkan device: ...`

If you see "no GPU found" instead, refer to the GPU section above.
