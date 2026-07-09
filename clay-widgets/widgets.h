#ifndef CLAY_WIDGETS_H
#define CLAY_WIDGETS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "clay.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CLAY_WIDGETS_TEXT_MAX_BYTES
#define CLAY_WIDGETS_TEXT_MAX_BYTES 2048
#endif

#ifndef CLAY_WIDGETS_MAX_FOCUSABLES
#define CLAY_WIDGETS_MAX_FOCUSABLES 128
#endif

#ifndef CLAY_WIDGETS_SCROLLBAR_WIDTH
#define CLAY_WIDGETS_SCROLLBAR_WIDTH 8
#endif

#ifndef CLAY_WIDGETS_MAX_ANIMS
#define CLAY_WIDGETS_MAX_ANIMS 64
#endif

#ifndef CLAY_WIDGETS_MAX_SCROLL_NESTING
#define CLAY_WIDGETS_MAX_SCROLL_NESTING 8
#endif

// Number of scratch buffers for dynamic per-frame strings (stepper values,
// slider value labels). Clay retains text by POINTER until render, so every
// dynamic string alive in one frame needs its own slot; overflow is reported
// via the error handler / CLAY_WIDGETS_ASSERT because reused slots render as
// corrupted text. Raise this if a screen shows more dynamic values at once.
#ifndef CLAY_WIDGETS_TEXT_SCRATCH_COUNT
#define CLAY_WIDGETS_TEXT_SCRATCH_COUNT 8
#endif

// Maximum simultaneously visible toasts; ShowToast drops the oldest when full.
#ifndef CLAY_WIDGETS_MAX_TOASTS
#define CLAY_WIDGETS_MAX_TOASTS 4
#endif

// Debug backstop for cap overflows (scratch strings, focus order, table
// columns). Fires only when no error handler is installed on the context
// (see ClayWidgets_SetErrorHandler). Define it away, or to your own logger,
// to change the behavior; defaults to print + abort in debug builds and to a
// no-op when NDEBUG is set, so release builds degrade silently as before.
#ifndef CLAY_WIDGETS_ASSERT
#ifdef NDEBUG
#define CLAY_WIDGETS_ASSERT(message) ((void)0)
#else
#include <stdio.h>
#include <stdlib.h>
#define CLAY_WIDGETS_ASSERT(message) (fprintf(stderr, "clay-widgets: %s\n", (message)), abort())
#endif
#endif

typedef struct ClayWidgets_Input {
    float mouseX;
    float mouseY;
    bool pointerDown;
    bool pointerPressed;
    bool pointerReleased;
    bool pointerRightPressed;
    float scrollX;
    float scrollY;
    float deltaTime;

    const char *textUtf8;
    int32_t textUtf8Length;

    bool keyBackspace;
    bool keyDelete;
    bool keyHome;
    bool keyEnd;
    bool keyLeft;
    bool keyRight;
    bool keyUp;
    bool keyDown;
    bool keyEnter;
    bool keyEscape;
    bool keyTab;      // with shiftDown: reverse focus traversal
    bool keySelectAll; // e.g. Ctrl+A
    bool keyCopy;      // e.g. Ctrl+C - copy the text-input selection
    bool keyCut;       // e.g. Ctrl+X
    bool keyPaste;     // e.g. Ctrl+V
    bool shiftDown;
} ClayWidgets_Input;

typedef Clay_Dimensions (*ClayWidgets_MeasureTextFunction)(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

// Clipboard bridge, injected like the measure-text function so the library
// stays platform-agnostic. `get` returns a NUL-terminated UTF-8 string (or
// NULL); `set` receives one. See ClayWidgets_SetClipboardFunctions.
typedef const char *(*ClayWidgets_GetClipboardTextFunction)(void *userData);
typedef void (*ClayWidgets_SetClipboardTextFunction)(const char *textUtf8, void *userData);

// Called when a compile-time cap is exceeded at runtime (scratch strings,
// focus order, table columns) - once per category per frame. When no handler
// is installed the CLAY_WIDGETS_ASSERT backstop fires instead.
typedef void (*ClayWidgets_ErrorHandlerFunction)(const char *message, void *userData);

typedef struct ClayWidgets_Spacing {
    uint16_t xs;
    uint16_t sm;
    uint16_t md;
    uint16_t lg;
} ClayWidgets_Spacing;

typedef struct ClayWidgets_Theme {
    Clay_Color textColor;
    Clay_Color textMutedColor;
    Clay_Color surfaceColor;
    Clay_Color surfaceAltColor;
    Clay_Color accentColor;
    Clay_Color accentMutedColor;
    Clay_Color borderColor;
    Clay_Color hoverColor;
    Clay_Color pressedColor;
    Clay_Color focusRingColor;

    // Semantic status palette, shared by badges, toasts and danger buttons so
    // a status reads as the same color everywhere and presets can restyle it.
    Clay_Color successColor;
    Clay_Color warningColor;
    Clay_Color dangerColor;
    // Text/glyphs drawn on accent or status fills (button labels, check marks,
    // the toggle knob).
    Clay_Color onAccentColor;
    // The modal dimming overlay.
    Clay_Color scrimColor;
    // How far disabled fills/text mix toward surfaceColor (0 = unchanged,
    // 1 = fully flattened into the surface).
    float disabledMix;

    uint16_t radiusSm;
    uint16_t radiusMd;

    uint16_t fontBody;
    uint16_t fontHeading;
    uint16_t fontMono;

    uint16_t fontSizeBody;
    uint16_t fontSizeHeading;
    uint16_t fontSizeSmall;

    ClayWidgets_Spacing spacing;
} ClayWidgets_Theme;

// One eased scalar for a widget effect that a Clay property transition can't
// express (e.g. a toggle knob's position). Keyed by element id; reclaimed when
// its id stops being requested. See ClayWidgets__AnimTo.
typedef struct ClayWidgets_AnimSlot {
    uint32_t id;
    uint32_t frame; // last frame this slot was requested; stale slots get reused
    float value;    // current eased value
} ClayWidgets_AnimSlot;

// One queued transient notification; see toast.h.
typedef struct ClayWidgets_ToastSlot {
    char message[160];
    int32_t length;
    float remaining;
    int32_t variant;
} ClayWidgets_ToastSlot;

typedef struct ClayWidgets_Context {
    ClayWidgets_Input input;
    ClayWidgets_Theme theme;
    ClayWidgets_MeasureTextFunction measureText;
    void *measureTextUserData;
    ClayWidgets_GetClipboardTextFunction getClipboardText;
    ClayWidgets_SetClipboardTextFunction setClipboardText;
    void *clipboardUserData;
    ClayWidgets_ErrorHandlerFunction errorHandler;
    void *errorHandlerUserData;
    uint32_t frameErrorFlags; // one bit per overflow category, reset each frame (dedupes reports)
    Clay_Dimensions layoutDimensions;

    // When false, widgets emit no Clay transitions (colors/enter states snap
    // instantly) and ClayWidgets__AnimTo returns its target directly. Defaults to
    // true; turn off for deterministic screenshots or to honor a reduce-motion
    // preference. See ClayWidgets__ColorTransition and ClayWidgets__AnimTo.
    bool animationsEnabled;

    // Per-widget eased scalars (Route B animations) plus the frame counter used
    // to age out slots whose widget has disappeared.
    ClayWidgets_AnimSlot anims[CLAY_WIDGETS_MAX_ANIMS];
    uint32_t animFrame;

    // A hovered widget clip that Clay treats as a scroll container but that can't
    // consume a vertical wheel (a horizontally-clipped table, a single-line text
    // field). Recorded during layout; next BeginFrame forwards the swallowed
    // wheel straight to the enclosing scroll panel (wheelFallthroughPanelId) so
    // the panel scrolls even when the clip covers all of it. Box is last frame's
    // geometry, used to confirm the pointer is still over the clip.
    uint32_t wheelFallthroughId;
    uint32_t wheelFallthroughPanelId; // content id of the scroll panel to forward to (0 = none)
    Clay_BoundingBox wheelFallthroughBox;

    // Stack of open scroll-panel content ids, so a clip widget can find the
    // scroll panel it lives in. Pushed by BeginScrollPanel, popped by
    // EndScrollPanel, reset each frame.
    uint32_t scrollPanelStack[CLAY_WIDGETS_MAX_SCROLL_NESTING];
    int32_t scrollPanelDepth;

    uint32_t activeId;
    uint32_t focusedId;
    uint32_t focusOrder[CLAY_WIDGETS_MAX_FOCUSABLES];
    int32_t focusCount;

    // Focus trap (modal). While a trap was active last frame, widgets declared
    // outside it neither register for Tab focus nor keep keyboard focus, so
    // Tab cycles the dialog and Enter can't activate controls behind the
    // scrim. One frame of lag, like all pointer queries in this library.
    uint32_t focusTrapId;     // set by BeginModal during this frame's layout
    uint32_t focusTrapPrevId; // last frame's value; what registration checks
    bool insideFocusTrap;     // true between BeginModal and EndModal
    uint32_t textInputId;
    int32_t textCursor;
    int32_t textSelectionAnchor;
    float textScrollX;
    bool textPointerSelecting;
    float caretBlinkTime;
    float elapsedTime;
    uint32_t lastClickId;
    float lastClickTime;
    float lastClickX;
    float lastClickY;
    bool clickConsumed;

    uint32_t scrollBarDragContainerId;
    float scrollBarDragStartMouseY;
    float scrollBarDragStartScrollY;

    uint32_t openComboId;
    int32_t comboHighlightIndex;
    // Set when the keyboard moved the highlight (or the dropdown just opened)
    // so the dropdown scrolls the highlighted item into view. Not set on
    // pointer hover - scrolling under the pointer would re-highlight and fight
    // the wheel.
    bool comboScrollToHighlight;

    uint32_t hoverTooltipId;
    float hoverTooltipTime;

    uint32_t openMenuId;

    uint32_t openContextMenuId;
    float contextMenuX;
    float contextMenuY;

    // Pool of small buffers for dynamic strings (e.g. a stepper's number). Clay
    // retains text by pointer until render, so these must outlive the layout;
    // reused across frames, reset at the start of each frame. Exhaustion is
    // reported via ClayWidgets__ReportError - see CLAY_WIDGETS_TEXT_SCRATCH_COUNT.
    char textScratch[CLAY_WIDGETS_TEXT_SCRATCH_COUNT][24];
    uint32_t textScratchNext;

    // NUL-termination staging for clipboard writes (a text-input selection is
    // a slice of the caller's buffer, not a terminated string).
    char clipboardScratch[CLAY_WIDGETS_TEXT_MAX_BYTES];

    // Queue of transient toasts, oldest first. Counted down and drawn by
    // ClayWidgets_ToastLayer each frame; ShowToast appends, evicting the
    // oldest when full.
    ClayWidgets_ToastSlot toasts[CLAY_WIDGETS_MAX_TOASTS];
    int32_t toastCount;

    // Column widths captured by BeginTable so TableRow can size its cells to
    // match the header without the caller passing them again.
    Clay_SizingAxis tableColWidths[12];
    int32_t tableColCount;
} ClayWidgets_Context;

typedef struct ClayWidgets_SliderOptions {
    float minValue;
    float maxValue;
    float step;
    bool showValue;        // draw the live value centered over the track
    int32_t valueDecimals; // decimals for the value text; <= 0 = auto from step/range
    bool disabled;         // inert: no focus, pointer or keyboard interaction
} ClayWidgets_SliderOptions;

typedef struct ClayWidgets_StepperOptions {
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
    bool disabled; // inert: no focus, pointer or keyboard interaction
} ClayWidgets_StepperOptions;

typedef struct ClayWidgets_TextInputOptions {
    const char *placeholder;
    bool clearOnEnter;
    bool disabled; // inert: not focusable or editable; text still shown
} ClayWidgets_TextInputOptions;

typedef struct ClayWidgets_ScrollPanelOptions {
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    uint16_t fadeMargin; // vertical inset where content clips before the panel edge (0 = theme default)
    uint16_t padding;    // horizontal content padding (0 = theme default)
    uint16_t childGap;   // gap between child widgets (0 = theme default)
} ClayWidgets_ScrollPanelOptions;

#include "core.h"
#include "themes.h"
#include "text.h"
#include "image.h"
#include "badge.h"
#include "card.h"
#include "collapsible.h"
#include "tree.h"
#include "button.h"
#include "checkbox.h"
#include "toggle.h"
#include "radio.h"
#include "tab.h"
#include "slider.h"
#include "progress-bar.h"
#include "text-input.h"
#include "combo.h"
#include "listbox.h"
#include "list-row.h"
#include "segmented.h"
#include "stepper.h"
#include "table.h"
#include "tooltip.h"
#include "modal.h"
#include "toast.h"
#include "menu.h"
#include "scroll-bar.h"
#include "scroll-panel.h"

#ifdef __cplusplus
}
#endif

#endif