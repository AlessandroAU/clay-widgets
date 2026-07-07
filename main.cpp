#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CLAY_IMPLEMENTATION
#include "clay.h"

#define CLAY_WIDGETS_IMPLEMENTATION
#include "clay-widgets/widgets.h"

#include "raylib.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <functional>
#endif

namespace {

#ifdef __EMSCRIPTEN__
// The browser owns the frame loop, so on web we hand a per-frame callback to
// emscripten instead of spinning our own while() loop. The callback captures
// main()'s locals by reference; compiling with -sASYNCIFY and
// simulate_infinite_loop=1 keeps main()'s stack (and therefore those locals)
// alive for the lifetime of the page.
static std::function<void()> g_webFrame;
static void ClayWidgets_WebFrame() {
    if (g_webFrame) {
        g_webFrame();
    }
}

// Resize the raylib window (and thus the canvas) to match the browser window.
// raylib reads GetScreenWidth()/GetScreenHeight() every frame, so this is all
// the Clay layout needs to reflow responsively. Emscripten's GLFW handles the
// HiDPI backing-store scaling, so text stays crisp.
static void ClayWidgets_SyncCanvasToWindow() {
    int w = EM_ASM_INT({ return window.innerWidth; });
    int h = EM_ASM_INT({ return window.innerHeight; });
    if (w > 0 && h > 0) {
        SetWindowSize(w, h);
    }
}

static EM_BOOL ClayWidgets_OnResize(int, const EmscriptenUiEvent *e, void *) {
    if (e && e->windowInnerWidth > 0 && e->windowInnerHeight > 0) {
        SetWindowSize(e->windowInnerWidth, e->windowInnerHeight);
    } else {
        ClayWidgets_SyncCanvasToWindow();
    }
    return EM_FALSE;
}
#endif

static Clay_String ClayStringFromCString(const char *text) {
    Clay_String s = {};
    if (!text) {
        s.chars = "";
        s.length = 0;
        s.isStaticallyAllocated = true;
        return s;
    }
    s.chars = text;
    s.length = static_cast<int32_t>(std::strlen(text));
    s.isStaticallyAllocated = false;
    return s;
}

static Color ToRaylibColor(Clay_Color c) {
    auto clampByte = [](float v) -> unsigned char {
        if (v < 0.0f) return 0;
        if (v > 255.0f) return 255;
        return static_cast<unsigned char>(std::round(v));
    };
    return Color{clampByte(c.r), clampByte(c.g), clampByte(c.b), clampByte(c.a)};
}

static Color MixOverlay(Color base, Color overlay) {
    float a = static_cast<float>(overlay.a) / 255.0f;
    Color out = {
        static_cast<unsigned char>(base.r + (overlay.r - base.r) * a),
        static_cast<unsigned char>(base.g + (overlay.g - base.g) * a),
        static_cast<unsigned char>(base.b + (overlay.b - base.b) * a),
        base.a,
    };
    return out;
}

static void AppendUtf8FromCodepoint(char *buffer, int &length, int maxLength, int codepoint) {
    if (maxLength <= 0 || length >= maxLength) {
        return;
    }

    auto pushByte = [&](unsigned char b) {
        if (length < maxLength) {
            buffer[length++] = static_cast<char>(b);
        }
    };

    if (codepoint <= 0x7F) {
        pushByte(static_cast<unsigned char>(codepoint));
        return;
    }
    if (codepoint <= 0x7FF) {
        pushByte(static_cast<unsigned char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if (codepoint <= 0xFFFF) {
        pushByte(static_cast<unsigned char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F)));
        pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    pushByte(static_cast<unsigned char>(0xF0 | ((codepoint >> 18) & 0x07)));
    pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 12) & 0x3F)));
    pushByte(static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F)));
    pushByte(static_cast<unsigned char>(0x80 | (codepoint & 0x3F)));
}

static Clay_Dimensions MeasureTextRaylib(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Font *fonts = static_cast<Font *>(userData);
    Font font = fonts ? fonts[config->fontId] : GetFontDefault();

    std::string tmp;
    tmp.assign(text.chars, text.length);

    float fontSize = static_cast<float>(config->fontSize);
    float spacing = static_cast<float>(config->letterSpacing);
    Vector2 measured = MeasureTextEx(font, tmp.c_str(), fontSize, spacing);

    Clay_Dimensions out = {};
    out.width = measured.x;
    out.height = measured.y;
    return out;
}

static void HandleClayError(Clay_ErrorData errorData) {
    std::string msg(errorData.errorText.chars, errorData.errorText.length);
    TraceLog(LOG_ERROR, "Clay error (%d): %s", static_cast<int>(errorData.errorType), msg.c_str());
}

static Rectangle IntersectRects(Rectangle a, Rectangle b) {
    float x1 = std::max(a.x, b.x);
    float y1 = std::max(a.y, b.y);
    float x2 = std::min(a.x + a.width, b.x + b.width);
    float y2 = std::min(a.y + a.height, b.y + b.height);
    if (x2 < x1 || y2 < y1) {
        return Rectangle{x1, y1, 0, 0};
    }
    return Rectangle{x1, y1, x2 - x1, y2 - y1};
}

static void RenderClayCommands(Clay_RenderCommandArray commands, Font *fonts) {
    std::vector<Rectangle> scissorStack;
    std::vector<Color> overlayStack;
    bool scissorEnabled = false;
    const int roundedCornerSegments = 24;

    auto reapplyScissor = [&]() {
        if (scissorEnabled) {
            EndScissorMode();
            scissorEnabled = false;
        }
        if (!scissorStack.empty()) {
            Rectangle r = scissorStack.back();
            BeginScissorMode(static_cast<int>(r.x), static_cast<int>(r.y), static_cast<int>(r.width), static_cast<int>(r.height));
            scissorEnabled = true;
        }
    };

    auto applyOverlay = [&](Color c) -> Color {
        Color out = c;
        for (Color overlay : overlayStack) {
            out = MixOverlay(out, overlay);
        }
        return out;
    };

    for (int i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&commands, i);
        Rectangle rect = {
            cmd->boundingBox.x,
            cmd->boundingBox.y,
            cmd->boundingBox.width,
            cmd->boundingBox.height,
        };

        switch (cmd->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                Color color = applyOverlay(ToRaylibColor(cmd->renderData.rectangle.backgroundColor));
                float radius = cmd->renderData.rectangle.cornerRadius.topLeft;
                float minDim = std::max(1.0f, std::min(rect.width, rect.height));
                float roundness = std::min(1.0f, 2.0f * radius / minDim);
                DrawRectangleRounded(rect, roundness, roundedCornerSegments, color);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                const Clay_TextRenderData &text = cmd->renderData.text;
                Font font = fonts[text.fontId];
                std::string tmp(text.stringContents.chars, text.stringContents.length);
                Color color = applyOverlay(ToRaylibColor(text.textColor));
                DrawTextEx(font, tmp.c_str(), Vector2{rect.x, rect.y}, static_cast<float>(text.fontSize), static_cast<float>(text.letterSpacing), color);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                const Clay_BorderRenderData &b = cmd->renderData.border;
                Color c = applyOverlay(ToRaylibColor(b.color));
                float radius = b.cornerRadius.topLeft;
                bool uniformWidth = b.width.left == b.width.right
                    && b.width.top == b.width.bottom
                    && b.width.left == b.width.top;
                if (radius > 0.0f && uniformWidth && b.width.left > 0) {
                    // Draw a rounded, inset border whose outer edge aligns with the
                    // element's rounded background so fills never appear to bleed past it.
                    float t = static_cast<float>(b.width.left);
                    Rectangle inner = { rect.x + t, rect.y + t, rect.width - 2.0f * t, rect.height - 2.0f * t };
                    float minDim = std::min(inner.width, inner.height);
                    float innerRadius = std::max(0.0f, radius - t);
                    float roundness = minDim > 0.0f ? std::min(1.0f, 2.0f * innerRadius / minDim) : 0.0f;
                    DrawRectangleRoundedLinesEx(inner, roundness, roundedCornerSegments, t, c);
                } else {
                    if (b.width.top > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), static_cast<int>(rect.width), b.width.top, c);
                    if (b.width.bottom > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y + rect.height - b.width.bottom), static_cast<int>(rect.width), b.width.bottom, c);
                    if (b.width.left > 0) DrawRectangle(static_cast<int>(rect.x), static_cast<int>(rect.y), b.width.left, static_cast<int>(rect.height), c);
                    if (b.width.right > 0) DrawRectangle(static_cast<int>(rect.x + rect.width - b.width.right), static_cast<int>(rect.y), b.width.right, static_cast<int>(rect.height), c);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                if (scissorStack.empty()) {
                    scissorStack.push_back(rect);
                } else {
                    scissorStack.push_back(IntersectRects(scissorStack.back(), rect));
                }
                reapplyScissor();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                if (!scissorStack.empty()) {
                    scissorStack.pop_back();
                }
                reapplyScissor();
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START: {
                overlayStack.push_back(ToRaylibColor(cmd->renderData.overlayColor.color));
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END: {
                if (!overlayStack.empty()) {
                    overlayStack.pop_back();
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
                const Clay_ImageRenderData &img = cmd->renderData.image;
                if (img.imageData) {
                    Texture2D *tex = static_cast<Texture2D *>(img.imageData);
                    // The widget layer packs an optional 0xRRGGBBAA tint into
                    // userData (see clay-widgets/image.h). A null/zero value
                    // means untinted, so we draw the texture as-is (WHITE).
                    Color tint = WHITE;
                    if (cmd->userData) {
                        tint = ToRaylibColor(ClayWidgets_UnpackTint(cmd->userData));
                    }
                    tint = applyOverlay(tint);
                    Rectangle src = {0.0f, 0.0f, static_cast<float>(tex->width), static_cast<float>(tex->height)};
                    DrawTexturePro(*tex, src, rect, Vector2{0.0f, 0.0f}, 0.0f, tint);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
            case CLAY_RENDER_COMMAND_TYPE_NONE:
            default:
                break;
        }
    }

    if (scissorEnabled) {
        EndScissorMode();
    }
}

// A document shown in the "Documents" view of the demo.
struct DemoDocument {
    Clay_String title;
    Clay_String subtitle;
    Clay_String body;
};

static const DemoDocument kDemoDocuments[] = {
    {
        CLAY_STRING("Getting Started"),
        CLAY_STRING("Bring your own renderer and input."),
        CLAY_STRING(
            "clay-widgets is a small immediate-mode UI kit that sits on top of Clay's layout engine.\n"
            "\n"
            "Each frame you feed the widgets an input snapshot, declare your controls, and then hand the resulting Clay render commands to a backend of your choice. This demo uses raylib, but the same widget code works with any renderer that can draw rounded rectangles, borders and text.\n"
            "\n"
            "Because everything is rebuilt every frame, there is no retained widget tree to keep in sync - the state you pass in is the single source of truth."
        ),
    },
    {
        CLAY_STRING("Layout Model"),
        CLAY_STRING("Rows, columns, and grow/fit sizing."),
        CLAY_STRING(
            "Clay lays out elements as nested rows and columns. Every element sizes itself with one of three strategies: FIXED for an exact pixel size, FIT to hug its content, and GROW to share the remaining space with its siblings.\n"
            "\n"
            "Panels in this demo use GROW so they expand to fill the window, while controls like checkboxes use FIXED boxes with FIT labels. Switching between horizontal and vertical stacking is a single field, which is how the demo collapses to a single column on narrow windows."
        ),
    },
    {
        CLAY_STRING("Theming & Presets"),
        CLAY_STRING("Slate, Sand and Forest out of the box."),
        CLAY_STRING(
            "A theme is just a struct of colors, corner radii, font ids and spacing steps. Widgets read from the active theme every frame, so you can swap the entire look of the UI by assigning a new theme before you build the layout.\n"
            "\n"
            "Try the Theme radio buttons on the Settings tab to flip between the built-in presets and watch every widget update instantly."
        ),
    },
    {
        CLAY_STRING("Widget Catalog"),
        CLAY_STRING("What ships in the kit today."),
        CLAY_STRING(
            "Buttons, checkboxes, radio groups, sliders, progress bars, single-line text inputs with selection, dropdown combo boxes, headings, labels and separators.\n"
            "\n"
            "Container widgets round things out: scroll panels with a draggable scroll bar, plus the navigation bar and selectable list you are using right now, which are assembled in the demo from raw Clay elements."
        ),
    },
    {
        CLAY_STRING("Keyboard & Focus"),
        CLAY_STRING("Tab, arrows and activation."),
        CLAY_STRING(
            "Focusable widgets register themselves each frame in declaration order. Tab and Shift+Tab move focus, Enter activates the focused control, and text inputs support Home/End, word jumps and Ctrl+A select-all.\n"
            "\n"
            "The focused widget id is printed live on the Settings tab so you can watch focus move as you press Tab."
        ),
    },
};
static const int kDemoDocumentCount = (int)(sizeof(kDemoDocuments) / sizeof(kDemoDocuments[0]));

// Pages shown by the "Tab Plane" view: a tab strip switches the content panel below.
struct TabPage {
    Clay_String label;
    Clay_String heading;
    Clay_String body;
};

static const TabPage kTabPages[] = {
    {
        CLAY_STRING("Overview"),
        CLAY_STRING("Tabbed panels"),
        CLAY_STRING(
            "A tab plane is just a row of tabs above a shared content panel.\n"
            "\n"
            "Each tab is a ClayWidgets_Tab that behaves like a radio button: it takes a distinct option value and a shared selection pointer. Clicking a tab (or focusing it and pressing Enter) updates the selection, and the panel below swaps its contents to match."
        ),
    },
    {
        CLAY_STRING("Usage"),
        CLAY_STRING("How to build one"),
        CLAY_STRING(
            "Lay several ClayWidgets_Tab calls inside a horizontal container to form the strip, passing each an index and a pointer to your active-tab variable.\n"
            "\n"
            "Then branch on that variable to declare the body. Because the whole UI is rebuilt every frame, there is no retained tab control to keep in sync - the active-tab integer is the single source of truth."
        ),
    },
    {
        CLAY_STRING("Keyboard"),
        CLAY_STRING("Focus and activation"),
        CLAY_STRING(
            "Tabs register as focusable widgets in declaration order, so Tab and Shift+Tab move between them and Enter activates the focused tab.\n"
            "\n"
            "This makes the tab plane fully keyboard navigable without any extra wiring - the same focus system every other widget in the kit uses."
        ),
    },
    {
        CLAY_STRING("Styling"),
        CLAY_STRING("Theme-driven look"),
        CLAY_STRING(
            "The active tab paints with the theme accent color, hover uses the hover color, and the focused tab shows the focus ring on its border.\n"
            "\n"
            "Switch themes on the Settings tab to see the tab strip restyle instantly along with the rest of the widgets."
        ),
    },
};
static const int kTabPageCount = (int)(sizeof(kTabPages) / sizeof(kTabPages[0]));


// A full-width selectable row used for the document sidebar. Returns true when clicked.
static bool DocListItem(ClayWidgets_Context &ui, Clay_ElementId id, Clay_String title, bool selected) {
    bool over = Clay_PointerOver(id);
    bool clicked = over && IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    Clay_Color background = selected ? ui.theme.accentMutedColor : (over ? ui.theme.hoverColor : ui.theme.surfaceAltColor);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = CLAY_PADDING_ALL(ui.theme.spacing.md),
        },
        .backgroundColor = background,
        .cornerRadius = CLAY_CORNER_RADIUS(ui.theme.radiusSm),
        .border = {
            .color = selected ? ui.theme.accentColor : ui.theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_TEXT(title, {
            .textColor = ui.theme.textColor,
            .fontId = ui.theme.fontBody,
            .fontSize = ui.theme.fontSizeBody,
        });
    }

    return clicked;
}

} // namespace

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
        }
    }
    if (shotFrames < 1) {
        shotFrames = 1;
    }

    const int screenWidth = 1080;
    const int screenHeight = 720;
    const char *fontPath = "assets/fonts/Roboto-Regular.ttf";

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

    Font fonts[8] = {};
    fonts[0] = GetFontDefault();
    bool customFontLoaded = false;
    if (FileExists(fontPath)) {
        Font roboto = LoadFontEx(fontPath, 64, nullptr, 0);
        if (roboto.texture.id != 0) {
            fonts[0] = roboto;
            customFontLoaded = true;
        } else {
            TraceLog(LOG_WARNING, "Failed to load font '%s'. Using default font.", fontPath);
        }
    } else {
        TraceLog(LOG_WARNING, "Font file not found: %s. Using default font.", fontPath);
    }

    if (fonts[0].texture.id != 0) {
        SetTextureFilter(fonts[0].texture, TEXTURE_FILTER_BILINEAR);
    }

    // Procedurally generated white glyph textures used to demonstrate the image
    // element. Drawing them white lets ClayWidgets_Icon recolor them per theme
    // via its tint. Passed to the widgets as opaque Texture2D* handles.
    Texture2D iconCheck, iconPlay, iconCircle, iconSquare;
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawLineEx(&im, Vector2{14, 34}, Vector2{27, 47}, 7, WHITE);
        ImageDrawLineEx(&im, Vector2{27, 47}, Vector2{52, 16}, 7, WHITE);
        iconCheck = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawTriangle(&im, Vector2{22, 14}, Vector2{22, 50}, Vector2{52, 32}, WHITE);
        iconPlay = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawCircle(&im, 32, 32, 20, WHITE);
        iconCircle = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    {
        Image im = GenImageColor(64, 64, BLANK);
        ImageDrawRectangle(&im, 14, 14, 36, 36, WHITE);
        iconSquare = LoadTextureFromImage(im);
        UnloadImage(im);
    }
    SetTextureFilter(iconCheck, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconPlay, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconCircle, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(iconSquare, TEXTURE_FILTER_BILINEAR);

    uint32_t clayMemorySize = Clay_MinMemorySize();
    void *clayMemory = std::malloc(clayMemorySize);
    if (!clayMemory) {
        TraceLog(LOG_ERROR, "Failed to allocate Clay arena");
        CloseWindow();
        return 1;
    }

    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, clayMemory);
    Clay_Initialize(arena, Clay_Dimensions{static_cast<float>(screenWidth), static_cast<float>(screenHeight)}, Clay_ErrorHandler{HandleClayError, 0});
    Clay_SetMeasureTextFunction(MeasureTextRaylib, fonts);

    ClayWidgets_Context ui = {};
    ClayWidgets_Theme theme = ClayWidgets_DefaultTheme();
    ClayWidgets_Init(&ui, theme);
    ClayWidgets_SetMeasureTextFunction(&ui, MeasureTextRaylib, fonts);
    if (disableAnim) {
        ui.animationsEnabled = false;
    }

    bool featureA = true;
    bool featureB = false;
    bool notificationsEnabled = true;
    bool showAdvanced = true;      // collapsible section, open by default
    bool verboseLogging = false;
    bool experimentalGpu = false;
    int32_t selectedProfile = 2;
    int32_t selectedTheme = CLAY_WIDGETS_THEME_PRESET_SLATE;
    float masterVolume = 0.35f;
    float uiScale = 1.0f;
    char nameBuffer[128] = "Clay User";
    char projectBuffer[128] = "clay-widgets";
    int clickCount = 0;
    int applyCount = 0;
    char statusLine[128] = "Ready";

    static const Clay_String comboItems[] = {
        CLAY_STRING("Debug"),
        CLAY_STRING("Release"),
        CLAY_STRING("Release with Debug Info"),
        CLAY_STRING("Minimum Size"),
    };
    static const int32_t comboItemCount = (int32_t)(sizeof(comboItems) / sizeof(comboItems[0]));
    int32_t selectedBuildConfig = 1;

    static const Clay_String logLevelItems[] = {
        CLAY_STRING("Error"),
        CLAY_STRING("Warning"),
        CLAY_STRING("Info"),
        CLAY_STRING("Debug"),
        CLAY_STRING("Trace"),
    };
    static const int32_t logLevelItemCount = (int32_t)(sizeof(logLevelItems) / sizeof(logLevelItems[0]));
    int32_t selectedLogLevel = 2;

    static const Clay_String densityItems[] = {
        CLAY_STRING("Compact"),
        CLAY_STRING("Cozy"),
        CLAY_STRING("Comfortable"),
    };
    static const int32_t densityItemCount = (int32_t)(sizeof(densityItems) / sizeof(densityItems[0]));
    int32_t densityMode = 1;

    int32_t retryCount = 3;

    static const Clay_String tableNames[] = {
        CLAY_STRING("widgets.h"), CLAY_STRING("core.h"), CLAY_STRING("menu.h"), CLAY_STRING("table.h"),
    };
    static const Clay_String tableTypes[] = {
        CLAY_STRING("umbrella"), CLAY_STRING("core"), CLAY_STRING("widget"), CLAY_STRING("widget"),
    };
    static const Clay_String tableSizes[] = {
        CLAY_STRING("4.2 KB"), CLAY_STRING("14 KB"), CLAY_STRING("6.8 KB"), CLAY_STRING("5.1 KB"),
    };
    static const int32_t tableRowCount = 4;
    int32_t selectedTableRow = 1;

    bool treeSrcOpen = true;
    bool treeWidgetsOpen = true;
    bool treeAssetsOpen = false;

    int32_t activeView = 0; // 0 = Settings, 1 = Documents, 2 = Tab Plane
    int32_t selectedDoc = 0;
    int32_t activeTab = 0;  // selected page within the Tab Plane view
    bool showConfirmModal = false;

    if (shotView >= 0) {
        activeView = shotView;
    }
    if (shotOpenModal) {
        showConfirmModal = true;
    }
    if (shotTheme >= 1) {
        selectedTheme = shotTheme;
    }
    if (shotToast) {
        ClayWidgets_ShowToast(&ui, CLAY_STRING("Changes applied"), CLAY_WIDGETS_BADGE_SUCCESS, 6.0f);
    }
    int shotFrameCounter = 0;

    bool webQuit = false;
    auto frameStep = [&]() {
        float dt = GetFrameTime();

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

        ui.theme = ClayWidgets_ThemeFromPreset((ClayWidgets_ThemePreset)selectedTheme);

        ClayWidgets_BeginFrame(
            &ui,
            input,
            Clay_Dimensions{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
            true
        );

        bool compactLayout = GetScreenWidth() < 1120;
        Clay_SizingAxis leftPanelWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.58f);
        Clay_SizingAxis rightPanelWidth = compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_PERCENT(0.42f);
        Clay_SizingAxis leftPanelHeight = compactLayout ? CLAY_SIZING_PERCENT(0.64f) : CLAY_SIZING_GROW(0);
        Clay_SizingAxis rightPanelHeight = compactLayout ? CLAY_SIZING_PERCENT(0.36f) : CLAY_SIZING_GROW(0);

        CLAY(CLAY_ID("Root"), {
            .layout = {
                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                .padding = CLAY_PADDING_ALL(16),
                .childGap = 14,
                .layoutDirection = CLAY_TOP_TO_BOTTOM,
            },
            .backgroundColor = ui.theme.surfaceColor,
        }) {
            ClayWidgets_Heading(&ui, CLAY_STRING("clay-widgets demo"));
            ClayWidgets_Label(&ui, CLAY_STRING("A starter UI kit on top of Clay + raylib"));
            ClayWidgets_Separator(&ui);

            CLAY(CLAY_ID("MenuBar"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .padding = CLAY_PADDING_ALL(ui.theme.spacing.xs),
                    .childGap = ui.theme.spacing.xs,
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
                .backgroundColor = ui.theme.surfaceAltColor,
                .cornerRadius = CLAY_CORNER_RADIUS(ui.theme.radiusSm),
            }) {
                if (ClayWidgets_BeginMenu(&ui, CLAY_ID("FileMenu"), CLAY_STRING("File"))) {
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuNew"), CLAY_STRING("New Project"))) {
                        std::strncpy(statusLine, "Menu: New Project", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuOpen"), CLAY_STRING("Open..."))) {
                        std::strncpy(statusLine, "Menu: Open", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuSave"), CLAY_STRING("Save"))) {
                        std::strncpy(statusLine, "Menu: Save", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }
                    ClayWidgets_MenuSeparator(&ui);
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuDeleteItem"), CLAY_STRING("Delete Project..."))) {
                        showConfirmModal = true;
                    }
                    ClayWidgets_EndMenu(&ui, CLAY_ID("FileMenu"));
                }

                if (ClayWidgets_BeginMenu(&ui, CLAY_ID("EditMenu"), CLAY_STRING("Edit"))) {
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuUndo"), CLAY_STRING("Undo"))) {
                        std::strncpy(statusLine, "Menu: Undo", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuRedo"), CLAY_STRING("Redo"))) {
                        std::strncpy(statusLine, "Menu: Redo", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }
                    ClayWidgets_EndMenu(&ui, CLAY_ID("EditMenu"));
                }

                if (ClayWidgets_BeginMenu(&ui, CLAY_ID("ViewMenu"), CLAY_STRING("View"))) {
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewSettings"), CLAY_STRING("Settings"))) {
                        activeView = 0;
                    }
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewDocuments"), CLAY_STRING("Documents"))) {
                        activeView = 1;
                    }
                    if (ClayWidgets_MenuItem(&ui, CLAY_ID("MenuViewTabPlane"), CLAY_STRING("Tab Plane"))) {
                        activeView = 2;
                    }
                    ClayWidgets_EndMenu(&ui, CLAY_ID("ViewMenu"));
                }
            }

            CLAY(CLAY_ID("NavBar"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                    .childGap = 10,
                    .layoutDirection = CLAY_LEFT_TO_RIGHT,
                },
            }) {
                ClayWidgets_Tab(&ui, CLAY_ID("NavSettings"), CLAY_STRING("Settings"), 0, &activeView);
                ClayWidgets_Tab(&ui, CLAY_ID("NavDocuments"), CLAY_STRING("Documents"), 1, &activeView);
                ClayWidgets_Tab(&ui, CLAY_ID("NavTabPlane"), CLAY_STRING("Tab Plane"), 2, &activeView);

                ClayWidgets_Tooltip(&ui, CLAY_ID("NavSettings"), CLAY_STRING("Forms, controls and theming"));
                ClayWidgets_Tooltip(&ui, CLAY_ID("NavDocuments"), CLAY_STRING("A sidebar list with a reading pane"));
                ClayWidgets_Tooltip(&ui, CLAY_ID("NavTabPlane"), CLAY_STRING("Tabs sharing one framed panel"));
            }

            if (activeView == 0) {
            CLAY(CLAY_ID("MainColumns"), {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                    .childGap = 16,
                    .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                },
            }) {
                ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("LeftPanel"),
                    ClayWidgets_ScrollPanelOptions{ leftPanelWidth, leftPanelHeight, 0, 0, ui.theme.spacing.md });
                {
                    CLAY(CLAY_ID("IconRow"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .childGap = ui.theme.spacing.md,
                            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        },
                    }) {
                        ClayWidgets_Icon(&ui, CLAY_ID("IconCheck"), &iconCheck, 26, ui.theme.accentColor);
                        ClayWidgets_Icon(&ui, CLAY_ID("IconPlay"), &iconPlay, 26, ui.theme.textColor);
                        ClayWidgets_Icon(&ui, CLAY_ID("IconCircle"), &iconCircle, 26, ui.theme.accentColor);
                        ClayWidgets_Icon(&ui, CLAY_ID("IconSquare"), &iconSquare, 26, ui.theme.textMutedColor);
                        ClayWidgets_Label(&ui, CLAY_STRING("Icon elements (theme-tinted textures)"));
                    }

                    CLAY(CLAY_ID("CtxTarget"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .padding = CLAY_PADDING_ALL(ui.theme.spacing.md),
                            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
                        },
                        .backgroundColor = ui.theme.surfaceColor,
                        .cornerRadius = CLAY_CORNER_RADIUS(ui.theme.radiusSm),
                        .border = {
                            .color = ui.theme.borderColor,
                            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
                        },
                    }) {
                        ClayWidgets_Label(&ui, CLAY_STRING("Right-click here for a context menu"));
                    }
                    if (ClayWidgets_RightClicked(&ui, CLAY_ID("CtxTarget"))) {
                        ClayWidgets_OpenContextMenu(&ui, CLAY_ID("CanvasMenu"), ui.input.mouseX, ui.input.mouseY);
                    }

                    ClayWidgets_Label(&ui, CLAY_STRING("Keyboard: Tab moves focus, Enter activates the focused control."));

                    ClayWidgets_Label(&ui, CLAY_STRING("Density (segmented control)"));
                    ClayWidgets_Segmented(&ui, CLAY_ID("DensitySeg"), densityItems, densityItemCount, &densityMode);

                    ClayWidgets_Label(&ui, CLAY_STRING("Retry count (stepper)"));
                    ClayWidgets_Stepper(&ui, CLAY_ID("RetryStepper"), &retryCount, ClayWidgets_StepperOptions{0, 10, 1});

                    ClayWidgets_Label(&ui, CLAY_STRING("Badges (chips / tags)"));
                    CLAY(CLAY_ID("BadgeRow"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .childGap = ui.theme.spacing.sm,
                            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT,
                        },
                    }) {
                        ClayWidgets_Badge(&ui, CLAY_STRING("Neutral"), CLAY_WIDGETS_BADGE_NEUTRAL);
                        ClayWidgets_Badge(&ui, CLAY_STRING("Accent"), CLAY_WIDGETS_BADGE_ACCENT);
                        ClayWidgets_Badge(&ui, CLAY_STRING("Success"), CLAY_WIDGETS_BADGE_SUCCESS);
                        ClayWidgets_Badge(&ui, CLAY_STRING("Warning"), CLAY_WIDGETS_BADGE_WARNING);
                        ClayWidgets_Badge(&ui, CLAY_STRING("Danger"), CLAY_WIDGETS_BADGE_DANGER);
                    }

                    ClayWidgets_Label(&ui, CLAY_STRING("Project tree"));
                    if (ClayWidgets_TreeNode(&ui, CLAY_ID("TreeSrc"), CLAY_STRING("src"), 0, &treeSrcOpen)) {
                        ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeMain"), CLAY_STRING("main.cpp"), 1);
                        if (ClayWidgets_TreeNode(&ui, CLAY_ID("TreeWidgets"), CLAY_STRING("clay-widgets"), 1, &treeWidgetsOpen)) {
                            ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeButtonH"), CLAY_STRING("button.h"), 2);
                            ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeTreeH"), CLAY_STRING("tree.h"), 2);
                            ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeWidgetsH"), CLAY_STRING("widgets.h"), 2);
                        }
                    }
                    if (ClayWidgets_TreeNode(&ui, CLAY_ID("TreeAssets"), CLAY_STRING("assets"), 0, &treeAssetsOpen)) {
                        ClayWidgets_TreeLeaf(&ui, CLAY_ID("TreeFont"), CLAY_STRING("Roboto-Regular.ttf"), 1);
                    }

                    ClayWidgets_Label(&ui, CLAY_STRING("Files (table - click a row to select)"));
                    {
                        ClayWidgets_TableColumn tableCols[] = {
                            { CLAY_STRING("Name"), CLAY_SIZING_GROW(0) },
                            { CLAY_STRING("Type"), CLAY_SIZING_FIXED(110) },
                            { CLAY_STRING("Size"), CLAY_SIZING_FIXED(90) },
                        };
                        ClayWidgets_BeginTable(&ui, CLAY_ID("FilesTable"), tableCols, 3);
                        for (int r = 0; r < tableRowCount; ++r) {
                            Clay_ElementId rid = Clay_GetElementIdWithIndex(CLAY_STRING("FileRow"), (uint32_t)r);
                            Clay_String cells[] = { tableNames[r], tableTypes[r], tableSizes[r] };
                            if (ClayWidgets_TableRow(&ui, rid, cells, 3, r, r == selectedTableRow)) {
                                selectedTableRow = r;
                            }
                        }
                        ClayWidgets_EndTable(&ui, CLAY_ID("FilesTable"));
                    }

                    CLAY(CLAY_ID("ActionRow"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                            .childGap = 10,
                            .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                        },
                    }) {
                        if (ClayWidgets_Button(&ui, CLAY_ID("PrimaryButton"), CLAY_STRING("Apply Changes"))) {
                            clickCount += 1;
                            applyCount += 1;
                            std::snprintf(statusLine, sizeof(statusLine), "Applied preset %d", selectedProfile);
                            ClayWidgets_ShowToast(&ui, CLAY_STRING("Changes applied"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
                        }

                        if (ClayWidgets_Button(&ui, CLAY_ID("ResetButton"), CLAY_STRING("Reset Form"))) {
                            featureA = true;
                            featureB = false;
                            notificationsEnabled = true;
                            selectedProfile = 2;
                            selectedTheme = CLAY_WIDGETS_THEME_PRESET_SLATE;
                            masterVolume = 0.35f;
                            uiScale = 1.0f;
                            selectedBuildConfig = 1;
                            std::strncpy(nameBuffer, "Clay User", sizeof(nameBuffer));
                            nameBuffer[sizeof(nameBuffer) - 1] = '\0';
                            std::strncpy(projectBuffer, "clay-widgets", sizeof(projectBuffer));
                            projectBuffer[sizeof(projectBuffer) - 1] = '\0';
                            std::strncpy(statusLine, "Form reset", sizeof(statusLine));
                            statusLine[sizeof(statusLine) - 1] = '\0';
                        }

                        if (ClayWidgets_Button(&ui, CLAY_ID("DeleteButton"), CLAY_STRING("Delete Project..."))) {
                            showConfirmModal = true;
                        }
                    }

                    // A card groups the project form fields under a titled,
                    // bordered surface.
                    ClayWidgets_BeginCard(&ui, CLAY_ID("ProjectCard"), CLAY_STRING("Project Settings"));
                    {
                        ClayWidgets_TextInput(
                            &ui,
                            CLAY_ID("NameInput"),
                            CLAY_STRING("Display name"),
                            nameBuffer,
                            static_cast<int32_t>(sizeof(nameBuffer)),
                            ClayWidgets_TextInputOptions{"Type your name", false}
                        );

                        ClayWidgets_TextInput(
                            &ui,
                            CLAY_ID("ProjectInput"),
                            CLAY_STRING("Project slug"),
                            projectBuffer,
                            static_cast<int32_t>(sizeof(projectBuffer)),
                            ClayWidgets_TextInputOptions{"workspace identifier", false}
                        );

                        ClayWidgets_Combo(
                            &ui,
                            CLAY_ID("BuildConfigCombo"),
                            CLAY_STRING("Build configuration"),
                            comboItems,
                            comboItemCount,
                            &selectedBuildConfig
                        );
                    }
                    ClayWidgets_EndCard(&ui, CLAY_ID("ProjectCard"));

                    if (ClayWidgets_Button(&ui, CLAY_ID("PingButton"), CLAY_STRING("Increment Counter"))) {
                        clickCount += 1;
                        std::strncpy(statusLine, "Counter incremented", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }

                    ClayWidgets_Checkbox(&ui, CLAY_ID("CheckboxA"), CLAY_STRING("Enable particles"), &featureA);
                    ClayWidgets_Checkbox(&ui, CLAY_ID("CheckboxB"), CLAY_STRING("Enable shadows"), &featureB);
                    ClayWidgets_Checkbox(&ui, CLAY_ID("CheckboxNotifications"), CLAY_STRING("Enable notifications"), &notificationsEnabled);

                    ClayWidgets_Separator(&ui);
                    ClayWidgets_Label(&ui, CLAY_STRING("Switches (bound to the same state as the checkboxes above)"));
                    ClayWidgets_Toggle(&ui, CLAY_ID("ToggleParticles"), CLAY_STRING("Enable particles"), &featureA);
                    ClayWidgets_Toggle(&ui, CLAY_ID("ToggleShadows"), CLAY_STRING("Enable shadows"), &featureB);
                    ClayWidgets_Toggle(&ui, CLAY_ID("ToggleNotifications"), CLAY_STRING("Enable notifications"), &notificationsEnabled);

                    // A collapsible disclosure section groups rarely-used options.
                    if (ClayWidgets_BeginCollapsible(&ui, CLAY_ID("AdvancedSection"), CLAY_STRING("Advanced options"), &showAdvanced)) {
                        ClayWidgets_Checkbox(&ui, CLAY_ID("VerboseLog"), CLAY_STRING("Verbose logging"), &verboseLogging);
                        ClayWidgets_Checkbox(&ui, CLAY_ID("ExperimentalGpu"), CLAY_STRING("Experimental GPU path"), &experimentalGpu);
                        ClayWidgets_EndCollapsible(&ui, CLAY_ID("AdvancedSection"));
                    }

                    ClayWidgets_Label(&ui, CLAY_STRING("Log level (list box: click or focus + arrows)"));
                    ClayWidgets_ListBox(&ui, CLAY_ID("LogLevelList"), logLevelItems, logLevelItemCount, &selectedLogLevel);

                    ClayWidgets_Label(&ui, CLAY_STRING("Profile"));
                    ClayWidgets_Radio(&ui, CLAY_ID("Profile1"), CLAY_STRING("Minimal"), 1, &selectedProfile);
                    ClayWidgets_Radio(&ui, CLAY_ID("Profile2"), CLAY_STRING("Balanced"), 2, &selectedProfile);
                    ClayWidgets_Radio(&ui, CLAY_ID("Profile3"), CLAY_STRING("Quality"), 3, &selectedProfile);

                    ClayWidgets_Label(&ui, CLAY_STRING("Theme"));
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme1"), CLAY_STRING("Slate"), CLAY_WIDGETS_THEME_PRESET_SLATE, &selectedTheme);
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme2"), CLAY_STRING("Sand"), CLAY_WIDGETS_THEME_PRESET_SAND, &selectedTheme);
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme3"), CLAY_STRING("Forest"), CLAY_WIDGETS_THEME_PRESET_FOREST, &selectedTheme);
                    ClayWidgets_Radio(&ui, CLAY_ID("Theme4"), CLAY_STRING("Windows"), CLAY_WIDGETS_THEME_PRESET_WIN95, &selectedTheme);

                    ClayWidgets_Label(&ui, CLAY_STRING("Master Volume"));
                    masterVolume = ClayWidgets_Slider(
                        &ui,
                        CLAY_ID("MasterVolume"),
                        masterVolume,
                        ClayWidgets_SliderOptions{0.0f, 1.0f, 0.01f}
                    );

                    ClayWidgets_ProgressBar(
                        &ui,
                        CLAY_ID("Progress"),
                        masterVolume,
                        CLAY_STRING("Volume level")
                    );

                    ClayWidgets_Label(&ui, CLAY_STRING("UI Scale"));
                    uiScale = ClayWidgets_Slider(
                        &ui,
                        CLAY_ID("UiScale"),
                        uiScale,
                        ClayWidgets_SliderOptions{0.75f, 1.5f, 0.05f}
                    );

                    ClayWidgets_ProgressBar(
                        &ui,
                        CLAY_ID("ApplyProgress"),
                        uiScale / 1.5f,
                        CLAY_STRING("Scale calibration")
                    );
                }
                ClayWidgets_EndScrollPanel(&ui, CLAY_ID("LeftPanel"));

                ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("RightPanel"),
                    ClayWidgets_ScrollPanelOptions{ rightPanelWidth, rightPanelHeight, 0, 0, ui.theme.spacing.sm });
                {
                    ClayWidgets_Label(&ui, CLAY_STRING("Live State"));

                    char stateLines[14][256] = {};
                    int stateLineCount = 0;
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Button clicks: %d", clickCount);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Apply count: %d", applyCount);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Feature A: %s", featureA ? "ON" : "OFF");
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Feature B: %s", featureB ? "ON" : "OFF");
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Notifications: %s", notificationsEnabled ? "ON" : "OFF");
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Profile: %d", selectedProfile);

                    const char *themeName = "Slate";
                    if (selectedTheme == CLAY_WIDGETS_THEME_PRESET_SAND) {
                        themeName = "Sand";
                    } else if (selectedTheme == CLAY_WIDGETS_THEME_PRESET_FOREST) {
                        themeName = "Forest";
                    } else if (selectedTheme == CLAY_WIDGETS_THEME_PRESET_WIN95) {
                        themeName = "Windows";
                    }
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Theme: %s", themeName);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Volume: %.2f", masterVolume);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "UI Scale: %.2f", uiScale);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Name: %s", nameBuffer);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Project: %s", projectBuffer);

                    const char *buildConfigName = (selectedBuildConfig >= 0 && selectedBuildConfig < comboItemCount)
                        ? comboItems[selectedBuildConfig].chars : "(none)";
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Build config: %s", buildConfigName);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Focused widget id: %u", ui.focusedId);
                    std::snprintf(stateLines[stateLineCount++], sizeof(stateLines[0]), "Status: %s", statusLine);

                    for (int i = 0; i < stateLineCount; ++i) {
                        ClayWidgets_Label(&ui, ClayStringFromCString(stateLines[i]));
                    }
                }
                ClayWidgets_EndScrollPanel(&ui, CLAY_ID("RightPanel"));
            }
            } else if (activeView == 1) {
                CLAY(CLAY_ID("DocumentsView"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                        .childGap = 16,
                        .layoutDirection = compactLayout ? CLAY_TOP_TO_BOTTOM : CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("DocSidebar"),
                        ClayWidgets_ScrollPanelOptions{
                            compactLayout ? CLAY_SIZING_GROW(0) : CLAY_SIZING_FIXED(240),
                            compactLayout ? CLAY_SIZING_PERCENT(0.4f) : CLAY_SIZING_GROW(0),
                            0, 0, ui.theme.spacing.sm });
                    {
                        ClayWidgets_Label(&ui, CLAY_STRING("Library"));
                        for (int i = 0; i < kDemoDocumentCount; ++i) {
                            Clay_ElementId itemId = Clay_GetElementIdWithIndex(CLAY_STRING("DocItem"), (uint32_t)i);
                            if (DocListItem(ui, itemId, kDemoDocuments[i].title, selectedDoc == i)) {
                                selectedDoc = i;
                            }
                        }
                    }
                    ClayWidgets_EndScrollPanel(&ui, CLAY_ID("DocSidebar"));

                    ClayWidgets_BeginScrollPanel(&ui, CLAY_ID("DocContent"),
                        ClayWidgets_ScrollPanelOptions{ CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0), 0, 0, ui.theme.spacing.md });
                    {
                        int docIndex = (selectedDoc >= 0 && selectedDoc < kDemoDocumentCount) ? selectedDoc : 0;
                        ClayWidgets_Heading(&ui, kDemoDocuments[docIndex].title);
                        ClayWidgets_Label(&ui, kDemoDocuments[docIndex].subtitle);
                        ClayWidgets_Separator(&ui);
                        ClayWidgets_Label(&ui, kDemoDocuments[docIndex].body);
                    }
                    ClayWidgets_EndScrollPanel(&ui, CLAY_ID("DocContent"));
                }
            } else {
                CLAY(CLAY_ID("TabPlaneView"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                        .childGap = 12,
                        .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    },
                }) {
                    ClayWidgets_Label(&ui, CLAY_STRING("A tabbed panel: the tabs and their content share one framed surface."));

                    // The tab plane itself: one bordered card whose top row holds
                    // the tabs and whose body swaps with the active tab.
                    CLAY(CLAY_ID("TabPlane"), {
                        .layout = {
                            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                            .layoutDirection = CLAY_TOP_TO_BOTTOM,
                        },
                        .backgroundColor = ui.theme.surfaceAltColor,
                        .cornerRadius = CLAY_CORNER_RADIUS(ui.theme.radiusMd),
                        .border = {
                            .color = ui.theme.borderColor,
                            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
                        },
                    }) {
                        CLAY(CLAY_ID("TabStrip"), {
                            .layout = {
                                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                                .padding = { .left = ui.theme.spacing.sm, .right = ui.theme.spacing.sm, .top = ui.theme.spacing.xs, .bottom = 0 },
                                .childGap = ui.theme.spacing.xs,
                                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                            },
                        }) {
                            for (int i = 0; i < kTabPageCount; ++i) {
                                Clay_ElementId tabId = Clay_GetElementIdWithIndex(CLAY_STRING("TabPageTab"), (uint32_t)i);
                                ClayWidgets_TabEx(&ui, tabId, kTabPages[i].label, i, &activeTab, CLAY_WIDGETS_TAB_STYLE_ATTACHED);
                            }
                        }

                        CLAY(CLAY_ID("TabBody"), {
                            .layout = {
                                .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
                                .padding = CLAY_PADDING_ALL(ui.theme.spacing.lg),
                                .childGap = ui.theme.spacing.sm,
                                .layoutDirection = CLAY_TOP_TO_BOTTOM,
                            },
                            .border = {
                                .color = ui.theme.borderColor,
                                .width = { .left = 0, .right = 0, .top = 1, .bottom = 0 },
                            },
                        }) {
                            int tabIndex = (activeTab >= 0 && activeTab < kTabPageCount) ? activeTab : 0;
                            ClayWidgets_Heading(&ui, kTabPages[tabIndex].heading);
                            ClayWidgets_Label(&ui, kTabPages[tabIndex].body);
                        }
                    }
                }
            }

            // Right-click context menu, declared at root so its panel floats
            // above every view. Opened by the CtxTarget region above.
            if (ClayWidgets_BeginContextMenu(&ui, CLAY_ID("CanvasMenu"))) {
                if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxCut"), CLAY_STRING("Cut"))) {
                    std::strncpy(statusLine, "Context: Cut", sizeof(statusLine));
                    statusLine[sizeof(statusLine) - 1] = '\0';
                }
                if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxCopy"), CLAY_STRING("Copy"))) {
                    std::strncpy(statusLine, "Context: Copy", sizeof(statusLine));
                    statusLine[sizeof(statusLine) - 1] = '\0';
                }
                if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxPaste"), CLAY_STRING("Paste"))) {
                    std::strncpy(statusLine, "Context: Paste", sizeof(statusLine));
                    statusLine[sizeof(statusLine) - 1] = '\0';
                }
                ClayWidgets_MenuSeparator(&ui);
                if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxProps"), CLAY_STRING("Properties..."))) {
                    std::strncpy(statusLine, "Context: Properties", sizeof(statusLine));
                    statusLine[sizeof(statusLine) - 1] = '\0';
                }
                ClayWidgets_EndContextMenu(&ui, CLAY_ID("CanvasMenu"));
            }

            // Confirmation dialog, declared last so it overlays every view. The
            // scrim dims and blocks the UI behind it until dismissed.
            if (ClayWidgets_BeginModal(&ui, CLAY_ID("ConfirmModal"), CLAY_STRING("Delete project?"), &showConfirmModal)) {
                ClayWidgets_Label(&ui, CLAY_STRING("This permanently removes the project and cannot be undone."));

                CLAY(CLAY_ID("ModalButtonRow"), {
                    .layout = {
                        .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
                        .childGap = ui.theme.spacing.sm,
                        .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT,
                    },
                }) {
                    if (ClayWidgets_Button(&ui, CLAY_ID("ModalCancel"), CLAY_STRING("Cancel"))) {
                        showConfirmModal = false;
                    }
                    if (ClayWidgets_Button(&ui, CLAY_ID("ModalDelete"), CLAY_STRING("Delete"))) {
                        showConfirmModal = false;
                        std::strncpy(statusLine, "Project deleted", sizeof(statusLine));
                        statusLine[sizeof(statusLine) - 1] = '\0';
                    }
                }

                ClayWidgets_EndModal(&ui, CLAY_ID("ConfirmModal"));
            }

            // Transient toast layer, declared last so it floats above everything.
            ClayWidgets_ToastLayer(&ui);
        }

        Clay_RenderCommandArray commands = ClayWidgets_EndFrame(dt);

        BeginDrawing();
        ClearBackground(Color{18, 20, 25, 255});
        RenderClayCommands(commands, fonts);
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

    UnloadTexture(iconCheck);
    UnloadTexture(iconPlay);
    UnloadTexture(iconCircle);
    UnloadTexture(iconSquare);
    if (customFontLoaded) {
        UnloadFont(fonts[0]);
    }
    std::free(clayMemory);
    CloseWindow();
    return 0;
}
