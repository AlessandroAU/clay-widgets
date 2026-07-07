// clay-widgets demo application (entry point).
//
// Structured like a small real program rather than a widget dump:
//   - Dashboard: live task statistics, a data table, and quick actions.
//   - Tasks:     a working to-do manager (add / edit / filter / delete) built
//                from the kit's list rows, inputs, radios and modal dialog.
//   - Gallery:   the full widget catalog, grouped into themed cards.
//   - Settings:  theme presets, behavior toggles and live diagnostics.
//
// All state lives in DemoState (see demo/demo_state.h); each screen lives in its
// own file under demo/. Rendering is raylib via backends/raylib; the same file
// compiles to desktop and WebAssembly.

#include <algorithm>
#include <cstdlib>
#include <cstring>

// Clay and the widget library are header-only: their implementations are
// compiled into exactly this translation unit via the *_IMPLEMENTATION defines.
#define CLAY_IMPLEMENTATION
#include "clay.h"

#define CLAY_WIDGETS_IMPLEMENTATION
#include "clay-widgets/widgets.h"

#include "raylib.h"

// Clay -> raylib renderer, font-atlas cache, text measurement and web glue.
#include "backends/raylib/clay_raylib_renderer.h"

// Demo state plus one file per screen.
#include "demo/demo_state.h"
#include "demo/chrome.h"
#include "demo/screens/dashboard.h"
#include "demo/screens/tasks.h"
#include "demo/screens/gallery.h"
#include "demo/screens/settings.h"
#include "demo/floating_layers.h"

int main(int argc, char **argv) {
    // Optional headless screenshot harness so the demo can be verified without a
    // human at the keyboard. Examples:
    //   clay-widgets-demo --shot out.png --view 0 --frames 3
    //   clay-widgets-demo --shot hover.png --mouse 400 300 --mousedown
    // When --shot is set the app renders `frames` frames (optionally injecting a
    // synthetic mouse position/press), writes a PNG, and exits.
    const char *shotPath = nullptr;
    int shotFrames = 3;
    int shotView = -1;
    float forceMouseX = -1.0f;
    float forceMouseY = -1.0f;
    bool forceMouseDown = false;
    float forceScrollY = 0.0f;
    int shotTheme = -1;
    bool shotOpenModal = false;
    // Optional second interaction phase (e.g. open a menu, then act on an item).
    float forceMouse2X = -1.0f;
    float forceMouse2Y = -1.0f;
    bool forceMouseDown2 = false;
    bool forceRightClick = false; // makes the phase-1 click a right-click
    bool shotToast = false;
    bool disableAnim = false; // --no-anim: snap all widget transitions for deterministic shots
    int screenWidth = 1080;   // --size W H: initial window size (shots at other layouts)
    int screenHeight = 720;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc) {
            shotPath = argv[++i];
        } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            shotFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--view") == 0 && i + 1 < argc) {
            shotView = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--mouse") == 0 && i + 2 < argc) {
            forceMouseX = (float)std::atof(argv[++i]);
            forceMouseY = (float)std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--mousedown") == 0) {
            forceMouseDown = true;
        } else if (std::strcmp(argv[i], "--scroll") == 0 && i + 1 < argc) {
            // per-frame vertical wheel delta injected under the synthetic mouse
            forceScrollY = (float)std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--theme") == 0 && i + 1 < argc) {
            shotTheme = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--openmodal") == 0) {
            shotOpenModal = true;
        } else if (std::strcmp(argv[i], "--mouse2") == 0 && i + 2 < argc) {
            forceMouse2X = (float)std::atof(argv[++i]);
            forceMouse2Y = (float)std::atof(argv[++i]);
        } else if (std::strcmp(argv[i], "--mousedown2") == 0) {
            forceMouseDown2 = true;
        } else if (std::strcmp(argv[i], "--rightclick") == 0) {
            forceRightClick = true;
        } else if (std::strcmp(argv[i], "--toast") == 0) {
            shotToast = true;
        } else if (std::strcmp(argv[i], "--no-anim") == 0) {
            disableAnim = true;
        } else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
            screenWidth = std::atoi(argv[++i]);
            screenHeight = std::atoi(argv[++i]);
        }
    }
    if (shotFrames < 1) {
        shotFrames = 1;
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
    InitWindow(screenWidth, screenHeight, "clay-widgets demo");
    SetTargetFPS(60);

    // If the platform/GL context could not be created (e.g. no display, a locked
    // or disconnected session), bail before touching the GPU so we exit cleanly
    // instead of crashing on the first texture upload.
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "Window/display unavailable; cannot start the demo.");
        return 2;
    }

#ifdef __EMSCRIPTEN__
    // Match the canvas to the browser window now, then on every window resize,
    // so the layout fills the page and reflows (e.g. collapses to one column on
    // narrow windows) instead of staying pinned to the initial 1080x720.
    ClayWidgets_SyncCanvasToWindow();
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, ClayWidgets_OnResize);
#endif

    // raylib's HighDPI path multiplies every draw by GetWindowScaleDPI(), so a
    // glyph drawn at logical size S covers S*dpi device pixels. The font cache
    // bakes each atlas at that physical size (and we draw with the logical size),
    // so glyphs sample 1:1 on screen at any DPI.
    Vector2 dpiScale = GetWindowScaleDPI();

    // The UI font is baked into the binary (embedded_font.h), so there are no
    // external asset files. FontCache bakes one gamma-corrected atlas per pixel
    // size the UI asks for instead of scaling a single baked bitmap.
    FontCache fontCache;
    fontCache.dpiScale = std::max(dpiScale.x, dpiScale.y);
    fontCache.fallback = GetFontDefault();
    {
        // Probe once at the body size: fail loudly (and fall back to the default
        // font) if the embedded bytes are bad, and warm the most-used size. The
        // other sizes bake lazily on first use.
        int probeSize = FontCache_PixelSize(fontCache, 16.0f);
        Font probe = ClayWidgets_BakeFont(kEmbeddedRobotoTTF, static_cast<int>(kEmbeddedRobotoTTFSize), probeSize);
        if (probe.texture.id != 0) {
            SetTextureFilter(probe.texture, TEXTURE_FILTER_BILINEAR);
            fontCache.haveEmbedded = true;
            fontCache.atlases.emplace_back(probeSize, probe);
        } else {
            TraceLog(LOG_WARNING, "Failed to decode embedded font. Using default font.");
        }
    }

    // Procedurally generated white glyph textures used to demonstrate the image
    // element. Drawing them white lets ClayWidgets_Icon recolor them per theme
    // via its tint. Passed to the widgets as opaque Texture2D* handles.
    DemoIcons icons = {};
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawLineEx(&im, Vector2{14, 34}, Vector2{27, 47}, 7, WHITE);
        ImageDrawLineEx(&im, Vector2{27, 47}, Vector2{52, 16}, 7, WHITE);
        icons.check = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawTriangle(&im, Vector2{22, 14}, Vector2{22, 50}, Vector2{52, 32}, WHITE);
        icons.play = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawCircle(&im, 32, 32, 20, WHITE);
        icons.circle = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawRectangle(&im, 14, 14, 36, 36, WHITE);
        icons.square = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    SetTextureFilter(icons.check, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(icons.play, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(icons.circle, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(icons.square, TEXTURE_FILTER_BILINEAR);

    uint32_t clayMemorySize = Clay_MinMemorySize();
    void *clayMemory = std::malloc(clayMemorySize);
    if (!clayMemory) {
        TraceLog(LOG_ERROR, "Failed to allocate Clay arena");
        CloseWindow();
        return 1;
    }

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, clayMemory);
    Clay_Initialize(arena, Clay_Dimensions{static_cast<float>(screenWidth), static_cast<float>(screenHeight)}, Clay_ErrorHandler{HandleClayError, 0});
    //Clay_SetDebugModeEnabled(true);
    Clay_SetMeasureTextFunction(MeasureTextRaylib, &fontCache);

    ClayWidgets_Context ui = {};
    ClayWidgets_Theme theme = ClayWidgets_DefaultTheme();
    ClayWidgets_Init(&ui, theme);
    ClayWidgets_SetMeasureTextFunction(&ui, MeasureTextRaylib, &fontCache);

    DemoState demo;
    SeedTasks(demo);

    if (shotView >= kViewDashboard && shotView <= kViewSettings) {
        demo.activeView = shotView;
    }
    if (shotTheme >= 1) {
        demo.themePreset = shotTheme;
    }
    if (shotOpenModal) {
        demo.showDeleteModal = true;
    }
    if (shotToast) {
        ClayWidgets_ShowToast(&ui, CLAY_STRING("Changes applied"), CLAY_WIDGETS_BADGE_SUCCESS, 6.0f);
    }
    int shotFrameCounter = 0;

    bool webQuit = false;
    auto frameStep = [&]() {
        float dt = GetFrameTime();

        demo.scratchUsed = 0;
        ApplyPendingTaskEdits(demo);

        char frameUtf8[64] = {};
        int frameUtf8Len = 0;
        for (;;) {
            int codepoint = GetCharPressed();
            if (codepoint == 0) {
                break;
            }
            AppendUtf8FromCodepoint(frameUtf8, frameUtf8Len, static_cast<int>(sizeof(frameUtf8) - 1), codepoint);
        }
        frameUtf8[frameUtf8Len] = '\0';

        ClayWidgets_Input input = {};
        input.mouseX = static_cast<float>(GetMouseX());
        input.mouseY = static_cast<float>(GetMouseY());
        input.pointerDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        input.pointerPressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        input.pointerReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
        input.pointerRightPressed = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
        Vector2 wheel = GetMouseWheelMoveV();
        input.scrollX = wheel.x;
        input.scrollY = wheel.y;
        input.deltaTime = dt;
        input.textUtf8 = frameUtf8;
        input.textUtf8Length = frameUtf8Len;
        input.keyBackspace = IsKeyPressed(KEY_BACKSPACE);
        input.keyDelete = IsKeyPressed(KEY_DELETE);
        input.keyHome = IsKeyPressed(KEY_HOME);
        input.keyEnd = IsKeyPressed(KEY_END);
        input.keyLeft = IsKeyPressed(KEY_LEFT);
        input.keyRight = IsKeyPressed(KEY_RIGHT);
        input.keyUp = IsKeyPressed(KEY_UP);
        input.keyDown = IsKeyPressed(KEY_DOWN);
        input.keyEnter = IsKeyPressed(KEY_ENTER);
        input.keyEscape = IsKeyPressed(KEY_ESCAPE);
        input.keyTab = IsKeyPressed(KEY_TAB);
        input.keySelectAll = (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_A);
        input.shiftDown = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

        // Screenshot harness: inject a synthetic pointer so hover/press states can
        // be captured. pointerPressed fires only on the first frame it is held.
        if (shotPath && forceMouseX >= 0.0f) {
            // Two scripted interaction phases so tests can, e.g., click to open a
            // menu (phase 1) and then act on an item (phase 2). Each "click" is a
            // press+release a couple frames after its phase begins, so target
            // elements (including floating popups) are laid out before the
            // pointer acts on them.
            bool phase2 = (forceMouse2X >= 0.0f) && (shotFrameCounter >= 4);
            input.mouseX = phase2 ? forceMouse2X : forceMouseX;
            input.mouseY = phase2 ? forceMouse2Y : forceMouseY;

            int clickFrame = phase2 ? 6 : 2;
            bool wantClick = phase2 ? forceMouseDown2 : forceMouseDown;
            bool press = wantClick && (shotFrameCounter == clickFrame);
            bool release = wantClick && (shotFrameCounter == clickFrame + 1);
            // A right-click is only scripted for phase 1 (e.g. open a context
            // menu), then phase 2 uses the left button to choose an item.
            bool rightClick = forceRightClick && !phase2;
            input.pointerDown = press && !rightClick;
            input.pointerPressed = press && !rightClick;
            input.pointerReleased = release && !rightClick;
            input.pointerRightPressed = press && rightClick;
        }
        if (shotPath && forceScrollY != 0.0f) {
            // Feed the wheel delta on every frame but the last, so the panel has
            // settled by the time the screenshot is taken.
            input.scrollY = (shotFrameCounter < shotFrames - 1) ? forceScrollY : 0.0f;
        }

        ui.theme = ClayWidgets_ThemeFromPreset((ClayWidgets_ThemePreset)demo.themePreset);
        ui.animationsEnabled = demo.animationsOn && !disableAnim;

        ClayWidgets_BeginFrame(
            &ui,
            input,
            Clay_Dimensions{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
            true
        );

        bool compactLayout = GetScreenWidth() < 900;

        CLAY(CLAY_ID("Root"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                .padding = CLAY_PADDING_ALL(16),
                .childGap = 12,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = ui.theme.surfaceColor,
        }) {
            DrawHeader(ui, demo, compactLayout);
            DrawMenuBar(ui, demo);
            DrawNavBar(ui, demo);

            switch (demo.activeView) {
                case kViewTasks:
                    DrawTasksView(ui, demo, compactLayout);
                    break;
                case kViewGallery:
                    DrawGalleryView(ui, demo, icons, compactLayout);
                    break;
                case kViewSettings:
                    DrawSettingsView(ui, demo);
                    break;
                case kViewDashboard:
                default:
                    DrawDashboardView(ui, demo, compactLayout);
                    break;
            }

            DrawStatusBar(ui, demo);

            // Floating layers last so they overlay every view; the toast layer
            // floats above even those.
            DrawFloatingLayers(ui, demo);
            ClayWidgets_ToastLayer(&ui);
        }

        Clay_RenderCommandArray commands = ClayWidgets_EndFrame(dt);

        BeginDrawing();
        ClearBackground(Color{18, 20, 25, 255});
        RenderClayCommands(commands, fontCache);
        EndDrawing();

        if (shotPath) {
            if (++shotFrameCounter >= shotFrames) {
                TakeScreenshot(shotPath);
                webQuit = true;
                return;
            }
        }
    };

#ifdef __EMSCRIPTEN__
    // fps=0 => drive from requestAnimationFrame; simulate_infinite_loop=1 (with
    // -sASYNCIFY) suspends here without unwinding main()'s stack.
    g_webFrame = frameStep;
    emscripten_set_main_loop(ClayWidgets_WebFrame, 0, 1);
#else
    while (!WindowShouldClose() && !webQuit) {
        frameStep();
    }
#endif

    UnloadTexture(icons.check);
    UnloadTexture(icons.play);
    UnloadTexture(icons.circle);
    UnloadTexture(icons.square);
    FontCache_Unload(fontCache);
    std::free(clayMemory);
    CloseWindow();
    return 0;
}
