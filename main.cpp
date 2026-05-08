#include "whisper.h"
#include <portaudio.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#else
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

static constexpr int SAMPLE_RATE    = 16000;
static constexpr int FRAMES_PER_BUF = 512;

struct Capture {
    std::vector<float> pcm;
    std::atomic<bool>  active{false};
    std::mutex         mtx;
};

static void pa_check(PaError err, const char *msg) {
    if (err != paNoError) {
        fprintf(stderr, "%s: %s\n", msg, Pa_GetErrorText(err));
        exit(1);
    }
}

static int pa_cb(const void *in, void * /*unused*/, unsigned long n,
                 const PaStreamCallbackTimeInfo * /*unused*/, PaStreamCallbackFlags /*unused*/,
                 void *ud) {
    auto                       &cap = *static_cast<Capture *>(ud);
    std::lock_guard<std::mutex> lock(cap.mtx);
    if (cap.active) {
        cap.pcm.insert(cap.pcm.end(), static_cast<const float *>(in),
                       static_cast<const float *>(in) + n);
    }
    return paContinue;
}

// ---- platform: type text into the focused window ----

#ifdef __APPLE__
static void type_text(const std::string &text) {
    // Escape for two layers: AppleScript double-quote string, then shell single-quoting.
    std::string safe;
    safe.reserve(text.size() * 2);
    for (char c : text) {
        if (c == '\\' || c == '"') {
            safe += '\\'; // AppleScript escape
            safe += c;
        } else if (c == '\'') {
            safe += "'\\''"; // shell single-quote escape
        } else {
            safe += c;
        }
    }
    std::string cmd =
        "osascript -e 'tell application \"System Events\" to keystroke \"" + safe + "\"'";
    (void)system(cmd.c_str());
}
#else
static void type_text(const std::string &text) {
    std::string safe;
    safe.reserve(text.size() * 2);
    for (char c : text) {
        safe += (c == '\'') ? "'\\''" : std::string(1, c);
    }
    std::string cmd = "xdotool type --clearmodifiers --delay 0 -- '" + safe + "'";
    (void)system(cmd.c_str());
}
#endif

// ---- transcription ----

static std::string transcribe(whisper_context *ctx, const std::vector<float> &pcm) {
    if (pcm.size() < static_cast<size_t>(SAMPLE_RATE / 4)) { // ignore < 0.25 s
        return {};
    }

    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.print_progress      = false;
    p.print_realtime      = false;
    p.print_timestamps    = false;
    p.language            = "en";
    p.n_threads           = 4;

    if (whisper_full(ctx, p, pcm.data(), static_cast<int>(pcm.size())) != 0) {
        return {};
    }

    std::string result;
    int         n = whisper_full_n_segments(ctx);
    for (int i = 0; i < n; ++i) {
        result += whisper_full_get_segment_text(ctx, i);
    }

    size_t s = result.find_first_not_of(" \t\n\r");
    size_t e = result.find_last_not_of(" \t\n\r");
    result   = (s == std::string::npos) ? "" : result.substr(s, e - s + 1);

    using namespace std::string_view_literals;
    static constexpr std::array junk = {
        "Thank you."sv,
        "Thanks for watching."sv,
        "Thanks for watching!"sv,
        "you"sv,
        "."sv,
        "..."sv,
        "Bye."sv,
        "Bye!"sv,
        "goodbye."sv,
        "Goodbye."sv,
        "[BLANK_AUDIO]"sv,
        "(silence)"sv,
        "[silence]"sv,
        "[Music]"sv,
        "[music]"sv,
        "www.mooji.org"sv,
        "Subtitles by"sv,
        "Subscribe"sv,
    };
    if (std::any_of(junk.begin(), junk.end(),
                    [&result](std::string_view j) { return result == j; })) {
        return {};
    }

    return result;
}

// ---- platform: hotkey event loop ----

static void on_press(Capture &cap) {
    cap.pcm.clear();
    cap.active = true;
    printf("Recording...\n");
}

static void on_release(Capture &cap, whisper_context *ctx) {
    cap.active = false;
    std::vector<float> local_pcm;
    {
        std::lock_guard<std::mutex> lock(cap.mtx);
        std::swap(local_pcm, cap.pcm);
    }
    printf("Transcribing %zu samples (%.1fs)...\n", local_pcm.size(),
           static_cast<float>(local_pcm.size()) / SAMPLE_RATE);
    std::string text = transcribe(ctx, local_pcm);
    if (!text.empty()) {
        printf("-> %s\n", text.c_str());
        type_text(text);
    }
}

#ifdef __APPLE__

static Capture         *g_cap = nullptr;
static whisper_context *g_ctx = nullptr;

static CGEventRef event_tap_cb(CGEventTapProxy /*unused*/, CGEventType type, CGEventRef event,
                               void * /*unused*/) {
    if (type == kCGEventKeyDown || type == kCGEventKeyUp) {
        auto key =
            static_cast<CGKeyCode>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
        if (key == kVK_F9) {
            if (type == kCGEventKeyDown && !g_cap->active) {
                on_press(*g_cap);
            } else if (type == kCGEventKeyUp && g_cap->active) {
                on_release(*g_cap, g_ctx);
            }
            return nullptr; // swallow F9 so it doesn't reach other apps
        }
    }
    return event;
}

static void run_event_loop(Capture &cap, whisper_context *ctx) {
    g_cap = &cap;
    g_ctx = ctx;

    CGEventMask   mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);
    CFMachPortRef tap  = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                                          kCGEventTapOptionDefault, mask, event_tap_cb, nullptr);
    if (tap == nullptr) {
        fprintf(
            stderr,
            "Failed to create event tap.\n"
            "Grant Accessibility access: System Settings → Privacy & Security → Accessibility\n");
        exit(1);
    }

    CFRunLoopSourceRef src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), src, kCFRunLoopCommonModes);
    CGEventTapEnable(tap, true);

    printf("Ready. Hold F9 to record, release to transcribe.\n");
    CFRunLoopRun();
}

#else // Linux

static void run_event_loop(Capture &cap, whisper_context *ctx) {
    Display *dpy = XOpenDisplay(nullptr);
    if (dpy == nullptr) {
        fprintf(stderr, "Cannot open X display\n");
        exit(1);
    }

    Window root    = DefaultRootWindow(dpy);
    int    keycode = XKeysymToKeycode(dpy, XK_F9);
    for (unsigned mod :
         {0U, (unsigned)Mod2Mask, (unsigned)LockMask, (unsigned)(Mod2Mask | LockMask)}) {
        XGrabKey(dpy, keycode, mod, root, True, GrabModeAsync, GrabModeAsync);
    }
    XSelectInput(dpy, root, KeyPressMask | KeyReleaseMask);
    XkbSetDetectableAutoRepeat(dpy, True, nullptr);

    printf("Ready. Hold F9 to record, release to transcribe.\n");

    XEvent ev;
    while (true) {
        XNextEvent(dpy, &ev);
        if (ev.xkey.keycode != (unsigned)keycode) {
            continue;
        }
        if (ev.type == KeyPress && !cap.active) {
            on_press(cap);
        } else if (ev.type == KeyRelease && cap.active) {
            on_release(cap, ctx);
        }
    }
}

#endif

// ---- main ----

int main(int argc, char *argv[]) {
    const char *model = (argc > 1) ? argv[1] : "models/ggml-base.en.bin";

    printf("Loading %s ...\n", model);
    whisper_context_params cparams = whisper_context_default_params();
    auto                  *ctx     = whisper_init_from_file_with_params(model, cparams);
    if (ctx == nullptr) {
        fprintf(stderr, "Failed to load model: %s\n", model);
        return 1;
    }

    pa_check(Pa_Initialize(), "PortAudio init failed");

    PaStreamParameters in_p{};
    in_p.device           = Pa_GetDefaultInputDevice();
    in_p.channelCount     = 1;
    in_p.sampleFormat     = paFloat32;
    in_p.suggestedLatency = Pa_GetDeviceInfo(in_p.device)->defaultLowInputLatency;

    Capture   cap;
    PaStream *stream = nullptr;
    pa_check(
        Pa_OpenStream(&stream, &in_p, nullptr, SAMPLE_RATE, FRAMES_PER_BUF, paClipOff, pa_cb, &cap),
        "Failed to open audio stream");
    pa_check(Pa_StartStream(stream), "Failed to start audio stream");

    run_event_loop(cap, ctx);
}
