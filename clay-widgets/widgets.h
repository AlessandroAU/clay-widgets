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
#define CLAY_WIDGETS_TEXT_SCRATCH_COUNT 16
#endif

// Maximum simultaneously visible toasts; ShowToast drops the oldest when full.
#ifndef CLAY_WIDGETS_MAX_TOASTS
#define CLAY_WIDGETS_MAX_TOASTS 4
#endif

// Slots for per-frame element decorations - the 3D edges and drop shadows a
// beveled theme paints in EndFrame (see ClayWidgets_SetEdge). One slot per
// decorated element per frame, in an open-addressed table, so this must be a
// power of two and comfortably larger than the number of controls on screen.
// Overflow is reported via the error handler / CLAY_WIDGETS_ASSERT.
#ifndef CLAY_WIDGETS_MAX_DECORATIONS
#define CLAY_WIDGETS_MAX_DECORATIONS 512
#endif
// The table is indexed by masking, so the size has to be a power of two.
typedef char ClayWidgets__DecorationsArePowerOfTwo[
    (CLAY_WIDGETS_MAX_DECORATIONS & (CLAY_WIDGETS_MAX_DECORATIONS - 1)) == 0 ? 1 : -1];

// The per-widget retained state pool (see ClayWidgets_GetState): how many
// widgets can hold internal state at once, and how many bytes each may keep.
// Slots are recycled least-recently-used once a widget stops requesting its
// state; exhaustion while every slot is still live is reported via the error
// handler / CLAY_WIDGETS_ASSERT.
#ifndef CLAY_WIDGETS_MAX_STATE_SLOTS
#define CLAY_WIDGETS_MAX_STATE_SLOTS 64
#endif
#ifndef CLAY_WIDGETS_STATE_SLOT_SIZE
#define CLAY_WIDGETS_STATE_SLOT_SIZE 64
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

// API convention: widgets that edit a value take it as a pointer and return
// true when they modified it this frame - checkbox, toggle, radio, tab,
// segmented, slider, stepper, combo, listbox, and (with the caller's buffer)
// text input and text area. Stateless activation widgets (button, menu item)
// return true when activated. Begin/End pairs return whether their body
// should be emitted. Display-only widgets return void.

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
    bool keySpace;
    bool controlDown; // word navigation / additive selection (Command on macOS)
    bool keyUndo;
    bool keyRedo;
    // Preedit is transient; committed composition arrives through textUtf8.
    const char *compositionUtf8;
    int32_t compositionLength;
    int32_t compositionCursor;
} ClayWidgets_Input;

#ifndef CLAY_WIDGETS_HISTORY_DEPTH
#define CLAY_WIDGETS_HISTORY_DEPTH 16
#endif
#ifndef CLAY_WIDGETS_HISTORY_BYTES
#define CLAY_WIDGETS_HISTORY_BYTES 8192
#endif
typedef struct ClayWidgets_TextSnapshot {
    char text[CLAY_WIDGETS_HISTORY_BYTES];
    int32_t cursor, anchor;
} ClayWidgets_TextSnapshot;
// Optional caller-owned history. Zero-initialize; one instance per editor.
typedef struct ClayWidgets_TextHistory {
    uint32_t id;
    int32_t count, position;
    ClayWidgets_TextSnapshot entries[CLAY_WIDGETS_HISTORY_DEPTH];
} ClayWidgets_TextHistory;
typedef struct ClayWidgets_EditResult {
    bool changed, submitted, cancelled, rejected, historyUnavailable, clipboardFailed;
} ClayWidgets_EditResult;
typedef bool (*ClayWidgets_ValidateTextFunction)(const char *text, int32_t length, void *userData);

typedef enum ClayWidgets_SemanticRole {
    CLAY_WIDGETS_ROLE_BUTTON, CLAY_WIDGETS_ROLE_CHECKBOX, CLAY_WIDGETS_ROLE_TEXT_FIELD,
    CLAY_WIDGETS_ROLE_LIST, CLAY_WIDGETS_ROLE_SLIDER, CLAY_WIDGETS_ROLE_DIALOG,
    CLAY_WIDGETS_ROLE_MENU_ITEM, CLAY_WIDGETS_ROLE_ROW, CLAY_WIDGETS_ROLE_GENERIC
} ClayWidgets_SemanticRole;
typedef struct ClayWidgets_SemanticNode {
    Clay_ElementId id;
    uint32_t parentId;
    ClayWidgets_SemanticRole role;
    Clay_String label, value;
    Clay_BoundingBox bounds;
    bool focused, disabled, selected, readOnly;
} ClayWidgets_SemanticNode;
typedef void (*ClayWidgets_SemanticFunction)(const ClayWidgets_SemanticNode *node, void *userData);
typedef void (*ClayWidgets_ImeFunction)(Clay_BoundingBox caret, const char *preedit, int32_t length, int32_t cursor, void *userData);

typedef Clay_Dimensions (*ClayWidgets_MeasureTextFunction)(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData);

// Clipboard bridge, injected like the measure-text function so the library
// stays platform-agnostic. `get` returns a NUL-terminated UTF-8 string (or
// NULL); `set` receives one. See ClayWidgets_SetClipboardFunctions.
typedef const char *(*ClayWidgets_GetClipboardTextFunction)(void *userData);
typedef void (*ClayWidgets_SetClipboardTextFunction)(const char *textUtf8, void *userData);
typedef bool (*ClayWidgets_TrySetClipboardTextFunction)(const char *textUtf8, void *userData);

// Called when a compile-time cap is exceeded at runtime (scratch strings,
// focus order, table columns) - once per category per frame. When no handler
// is installed the CLAY_WIDGETS_ASSERT backstop fires instead.
typedef void (*ClayWidgets_ErrorHandlerFunction)(const char *message, void *userData);

// Which mouse cursor the hovered widget wants this frame. Widgets set it
// while hovered during layout; read it after ClayWidgets_EndFrame with
// ClayWidgets_GetCursor and apply it via the platform's cursor API (e.g.
// raylib's SetMouseCursor). Resets to DEFAULT at each BeginFrame, so a frame
// with no hovered widget yields the arrow.
typedef enum ClayWidgets_Cursor {
    CLAY_WIDGETS_CURSOR_DEFAULT = 0, // arrow
    CLAY_WIDGETS_CURSOR_POINTER,     // hand: clickable things (buttons, toggles, rows, thumbs)
    CLAY_WIDGETS_CURSOR_TEXT,        // I-beam: editable text
    CLAY_WIDGETS_CURSOR_RESIZE_X,
    CLAY_WIDGETS_CURSOR_RESIZE_Y,
    CLAY_WIDGETS_CURSOR_RESIZE_XY,
} ClayWidgets_Cursor;

// How widgets draw their edges.
//
// FLAT is the modern look: one 1px line per element in the theme's borderColor.
// BEVEL is the classic Windows 3.x/9x look, where an edge is two mirrored
// two-tone bands that make a control read as raised out of, or sunk into, the
// surface it sits on.
//
// A Clay element carries a single border color and a classic edge needs four,
// so beveled edges are not drawn as Clay borders. Widgets tag their element
// with ClayWidgets_SetEdge and ClayWidgets_EndFrame paints the bands directly
// into the render command array, which costs no layout elements and needs no
// renderer support. Under a FLAT theme the tags are ignored and the 1px border
// is drawn as before.
typedef enum ClayWidgets_EdgeStyle {
    CLAY_WIDGETS_EDGE_STYLE_FLAT = 0,
    CLAY_WIDGETS_EDGE_STYLE_BEVEL = 1,
} ClayWidgets_EdgeStyle;

// Which 3D edge a widget wants. Widgets ask for the role, never for colors, so
// one theme switch restyles every control. Ignored while the theme is FLAT.
typedef enum ClayWidgets_Edge {
    CLAY_WIDGETS_EDGE_NONE = 0,
    CLAY_WIDGETS_EDGE_RAISED,      // 2px, pops out: buttons, panels, tabs, thumbs
    CLAY_WIDGETS_EDGE_SUNKEN,      // 2px, recessed well: fields, lists, tables, a held button
    CLAY_WIDGETS_EDGE_RAISED_THIN, // 1px raised: chips, badges, small dividers
    CLAY_WIDGETS_EDGE_SUNKEN_THIN, // 1px etched: group boxes, separators, troughs
    CLAY_WIDGETS_EDGE_FRAME,       // 1px hard outline in edgeDarkColor: tooltips, popup frames
} ClayWidgets_Edge;

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

    // The highlight bar behind a hovered menu item, dropdown item or list row,
    // and the text drawn on it. Defaults to hoverColor over the normal text
    // color - a subtle wash; the classic preset turns it into the solid navy
    // bar with white text that a Windows menu paints.
    Clay_Color selectionColor;
    Clay_Color onSelectionColor;

    // Background of anything the user types or picks into - text fields, list
    // boxes, table bodies, combo dropdowns. Usually the same as surfaceAltColor;
    // the classic preset makes it paper white so those wells read as sunken.
    Clay_Color fieldColor;

    // Edge treatment and the classic 3D palette. The four edge colors are only
    // consulted while edgeStyle is BEVEL, where an edge is two bands: the outer
    // one in light/dark, the inner one in highlight/shadow (mirrored for a
    // sunken edge). See ClayWidgets_Edge.
    ClayWidgets_EdgeStyle edgeStyle;
    Clay_Color edgeLightColor;     // outer top/left of a raised edge
    Clay_Color edgeHighlightColor; // inner top/left of a raised edge
    Clay_Color edgeShadowColor;    // inner bottom/right of a raised edge
    Clay_Color edgeDarkColor;      // outer bottom/right of a raised edge

    // Hard drop shadow cast by floating chrome (menus, dropdowns, dialogs,
    // toasts). Offset is in pixels; 0 disables the shadow entirely.
    Clay_Color shadowColor;
    uint16_t shadowOffset;

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

// One entry of the generic per-widget state pool: a POD block keyed by element
// id, zeroed when first claimed, retained across frames and recycled
// least-recently-used under pool pressure. See ClayWidgets_GetState.
typedef struct ClayWidgets_StateSlot {
    uint32_t id;    // 0 = empty
    uint32_t frame; // last frame this slot was requested (drives LRU recycling)
    union {
        max_align_t align; // fundamental alignment; over-aligned states require caller storage
        unsigned char bytes[CLAY_WIDGETS_STATE_SLOT_SIZE];
    } data;
} ClayWidgets_StateSlot;

// One queued transient notification; see toast.h.
typedef struct ClayWidgets_ToastSlot {
    char message[160];
    int32_t length;
    float remaining;
    int32_t variant;
} ClayWidgets_ToastSlot;

// One element's queued decoration for this frame: which 3D edge to paint and
// whether to add a drop shadow or a focus rectangle. Stored in an open-addressed
// table keyed by element id and consumed by ClayWidgets_EndFrame.
typedef struct ClayWidgets_Decoration {
    uint32_t id; // 0 = empty slot
    uint8_t edge;  // ClayWidgets_Edge
    uint8_t flags; // CLAY_WIDGETS__DECOR_*
} ClayWidgets_Decoration;

typedef struct ClayWidgets_Context {
    ClayWidgets_Input input;
    ClayWidgets_Theme theme;
    ClayWidgets_MeasureTextFunction measureText;
    void *measureTextUserData;
    ClayWidgets_GetClipboardTextFunction getClipboardText;
    ClayWidgets_SetClipboardTextFunction setClipboardText;
    ClayWidgets_TrySetClipboardTextFunction trySetClipboardText;
    bool clipboardFailed;
    void *clipboardUserData;
    ClayWidgets_ErrorHandlerFunction errorHandler;
    void *errorHandlerUserData;
    uint32_t frameErrorFlags; // one bit per overflow category, reset each frame (dedupes reports)
    Clay_Dimensions layoutDimensions;

    // The hovered widget's cursor hint, reset each frame; see ClayWidgets_Cursor.
    ClayWidgets_Cursor cursor;

    // When false, widgets emit no Clay transitions (colors/enter states snap
    // instantly) and ClayWidgets__AnimTo returns its target directly. Defaults to
    // true; turn off for deterministic screenshots or to honor a reduce-motion
    // preference. See ClayWidgets__ColorTransition and ClayWidgets__AnimTo.
    bool animationsEnabled;

    // Wheel momentum decay time in seconds (default 0.10). Zero or disabling
    // animations gives immediate wheel scrolling, useful for native trackpad inertia.
    float scrollMomentumTime;
    uint32_t scrollMomentumIds[2];
    float scrollMomentumRemaining[2], scrollMomentumPosition[2];

    // Per-widget eased scalars (Route B animations) plus the frame counter used
    // to age out slots whose widget has disappeared. The counter is shared with
    // the generic state pool below.
    ClayWidgets_AnimSlot anims[CLAY_WIDGETS_MAX_ANIMS];
    uint32_t animFrame;

    // Generic per-widget retained state, keyed by element id and recycled
    // least-recently-used. Used internally (scrollbar drag origin, tooltip
    // dwell timer) and available to custom widgets via ClayWidgets_GetState.
    ClayWidgets_StateSlot states[CLAY_WIDGETS_MAX_STATE_SLOTS];

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
    uint32_t releasedActiveId;
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
    // The caret's remembered horizontal position for the text area's Up/Down
    // navigation, so stepping through a short line and back into a long one
    // returns to the original column. < 0 = unset; any horizontal caret
    // motion clears it (see ClayWidgets__MoveCaret).
    float textPreferredX;
    bool textPointerSelecting;
    float caretBlinkTime;
    float elapsedTime;
    uint32_t lastClickId;
    float lastClickTime;
    float lastClickX;
    float lastClickY;
    bool clickConsumed;

    uint32_t openComboId;
    int32_t comboHighlightIndex;
    // Set when the keyboard moved the highlight (or the dropdown just opened)
    // so the dropdown scrolls the highlighted item into view. Not set on
    // pointer hover - scrolling under the pointer would re-highlight and fight
    // the wheel.
    bool comboScrollToHighlight;

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
    int16_t overlayBases[16];
    int32_t overlayDepth;
    uint32_t modalIds[16];
    uint32_t modalReturnFocus[16];
    uint32_t modalParents[16];
    uint32_t currentModalId;
    int32_t modalCount;
    int32_t modalCountPrev;
    bool focusFirst;
    ClayWidgets_Input disabledInputs[16];
    ClayWidgets_Theme disabledThemes[16];
    int32_t disabledDepth;
    ClayWidgets_SemanticFunction semantic;
    void *semanticUserData;
    ClayWidgets_ImeFunction ime;
    void *imeUserData;
    uint32_t requestedFocusId;
    uint32_t menuNavigationId;
    uint32_t menuItems[CLAY_WIDGETS_MAX_FOCUSABLES];
    int32_t menuItemCount;
    bool menuOpening;
    uint32_t typeAheadId;
    char typeAhead[64];
    int32_t typeAheadLength;
    float typeAheadTime;
    int32_t tableDepth;
    uint32_t tableIds[16];
    Clay_SizingAxis tableSavedWidths[16][12];
    int32_t tableSavedCounts[16];
    // Per-frame 3D edge / drop shadow requests, keyed by element id and painted
    // into the render command array by EndFrame. Cleared at BeginFrame.
    ClayWidgets_Decoration decorations[CLAY_WIDGETS_MAX_DECORATIONS];
    int32_t decorationCount;

    uint32_t menuTriggers[32];
    int32_t menuTriggerCount, menuTriggerCountPrev;
    uint32_t contextMenuReturnFocus;
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
    bool readOnly;
    ClayWidgets_TextHistory *history;
    ClayWidgets_ValidateTextFunction validate;
    void *validationUserData;
    ClayWidgets_EditResult *result;
} ClayWidgets_TextInputOptions;

typedef struct ClayWidgets_TextAreaOptions {
    const char *placeholder;
    float height;  // fixed pixel height of the field; 0 = default (~5 lines)
    bool noWrap;   // long lines scroll horizontally instead of soft-wrapping at word boundaries
    bool disabled; // inert: not focusable or editable; text still shown
    bool readOnly;
    ClayWidgets_TextHistory *history;
    ClayWidgets_ValidateTextFunction validate;
    void *validationUserData;
    ClayWidgets_EditResult *result;
} ClayWidgets_TextAreaOptions;

typedef struct ClayWidgets_ScrollPanelOptions {
    Clay_SizingAxis width;
    Clay_SizingAxis height;
    uint16_t fadeMargin; // vertical inset where content clips before the panel edge (0 = theme default)
    uint16_t padding;    // horizontal content padding (0 = theme default)
    uint16_t childGap;   // gap between child widgets (0 = theme default)
} ClayWidgets_ScrollPanelOptions;

#include "core.h"
#include "themes.h"
#include "text-edit.h"
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
#include "text-area.h"
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
#include "collection.h"
#include "split-pane.h"

#ifdef __cplusplus
}
#endif

#endif
