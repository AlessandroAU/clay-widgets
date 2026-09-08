// Demo application state and data model.
//
// All demo state lives in DemoState (plain structs, rebuilt into UI every
// frame); the screen files under demo/ read and mutate it. Kept separate from
// the widget library, which knows nothing about the demo.
#ifndef CLAY_WIDGETS_DEMO_STATE_H
#define CLAY_WIDGETS_DEMO_STATE_H

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "clay-widgets/widgets.h"
#include "raylib.h"

namespace {

// Builds a Clay_String view of a null-terminated C string, for state that isn't
// a compile-time literal (e.g. the status line).
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

// ---------------------------------------------------------------------------
// Demo data and state
// ---------------------------------------------------------------------------

enum DemoView {
    kViewDashboard = 0,
    kViewTasks = 1,
    kViewGallery = 2,
    kViewSettings = 3,
};

constexpr int32_t kMaxTasks = 32;
constexpr int32_t kTaskTitleCap = 64;

// Priorities are indexes into these tables: 0 = High, 1 = Medium, 2 = Low.
static const Clay_String kPriorityNames[] = {
    CLAY_STRING("High"),
    CLAY_STRING("Medium"),
    CLAY_STRING("Low"),
};
static const Clay_Color kPrioritySwatches[] = {
    Clay_Color{224, 82, 64, 255},
    Clay_Color{240, 180, 60, 255},
    Clay_Color{110, 168, 224, 255},
};

static const Clay_String kTaskFilterNames[] = {
    CLAY_STRING("All"),
    CLAY_STRING("Active"),
    CLAY_STRING("Done"),
};

// Short enough to fit the header's segmented switcher; the Settings radios
// spell them out. Index + 1 is the ClayWidgets_ThemePreset value.
static const Clay_String kThemeNames[] = {
    CLAY_STRING("Slate"),
    CLAY_STRING("Sand"),
    CLAY_STRING("Forest"),
    CLAY_STRING("Win95"),
    CLAY_STRING("Mac"),
    CLAY_STRING("Mac Dark"),
};
static const int32_t kThemeCount = (int32_t)(sizeof(kThemeNames) / sizeof(kThemeNames[0]));

static const Clay_String kDensityNames[] = {
    CLAY_STRING("Compact"),
    CLAY_STRING("Cozy"),
    CLAY_STRING("Comfortable"),
};

static const Clay_String kChannelNames[] = {
    CLAY_STRING("Stable"),
    CLAY_STRING("Beta"),
    CLAY_STRING("Nightly"),
    CLAY_STRING("Canary"),
};

static const Clay_String kLogLevelNames[] = {
    CLAY_STRING("Error"),
    CLAY_STRING("Warning"),
    CLAY_STRING("Info"),
    CLAY_STRING("Debug"),
    CLAY_STRING("Trace"),
};

static const Clay_String kBuildConfigNames[] = {
    CLAY_STRING("Debug"),
    CLAY_STRING("Release"),
    CLAY_STRING("Release with Debug Info"),
    CLAY_STRING("Minimum Size"),
};

// Rows for the Dashboard "Recent builds" table.
static const Clay_String kBuildNames[] = {
    CLAY_STRING("web build"),
    CLAY_STRING("widget sweep"),
    CLAY_STRING("theme audit"),
    CLAY_STRING("wasm smoke test"),
};
static const Clay_String kBuildStates[] = {
    CLAY_STRING("passing"),
    CLAY_STRING("passing"),
    CLAY_STRING("flaky"),
    CLAY_STRING("broken"),
};
static const Clay_String kBuildTimes[] = {
    CLAY_STRING("2m 14s"),
    CLAY_STRING("1m 03s"),
    CLAY_STRING("4m 41s"),
    CLAY_STRING("0m 12s"),
};
constexpr int32_t kBuildRowCount = 4;

// Pages for the Gallery's attached-tab mini plane.
struct MiniTabPage {
    Clay_String label;
    Clay_String body;
};
static const MiniTabPage kMiniTabs[] = {
    {
        CLAY_STRING("Overview"),
        CLAY_STRING("Attached tabs share one framed surface with the panel below, so the strip and body read as a single control."),
    },
    {
        CLAY_STRING("Behavior"),
        CLAY_STRING("Each tab is a radio bound to one shared integer. Rebuild the body from that integer every frame and the panel swaps for free."),
    },
    {
        CLAY_STRING("Keyboard"),
        CLAY_STRING("Tabs join the global focus order: Tab reaches the strip and Enter activates the focused tab."),
    },
};
constexpr int32_t kMiniTabCount = static_cast<int32_t>(sizeof(kMiniTabs) / sizeof(kMiniTabs[0]));

struct DemoTask {
    char title[kTaskTitleCap];
    int32_t priority; // index into kPriorityNames
    bool done;
};

struct DemoSeedTask {
    const char *title;
    int32_t priority;
    bool done;
};
static const DemoSeedTask kSeedTasks[] = {
    {"Ship the 0.2 release notes", 0, false},
    {"Fix wheel scroll over clipped tables", 0, true},
    {"Add the selectable list-row widget", 1, true},
    {"Document the theme presets", 1, false},
    {"Profile text measurement on web", 2, false},
    {"Run the wasm smoke test", 2, false},
    {"Refactor focus-ring drawing", 1, false},
    {"Design a date-picker widget", 2, false},
};

constexpr int32_t kScratchSlots = 24;
constexpr int32_t kScratchBytes = 128;

struct DemoState {
    int32_t activeView = kViewDashboard;

    // Set by --shot. Live readouts (frame rate) are replaced with a placeholder
    // so a capture of the same view is byte-identical every run - CI commits
    // the regenerated PNGs, and a per-run frame rate would make every push a
    // diff. Does not affect normal interactive runs.
    bool deterministic = false;

    // Tasks
    DemoTask tasks[kMaxTasks] = {};
    int32_t taskCount = 0;
    int32_t selectedTask = 0;
    int32_t taskFilter = 0; // index into kTaskFilterNames
    char newTaskTitle[kTaskTitleCap] = "";
    int32_t newTaskPriority = 1;
    int32_t contextTask = -1; // task the context menu was opened on

    // Structural task edits (delete / clear / reset) are deferred to the top of
    // the next frame: applying them mid-layout would move task memory that
    // Clay_Strings declared earlier this frame still point into.
    int32_t pendingDelete = -1;
    bool pendingClearCompleted = false;
    bool pendingReset = false;

    // Delete confirmation modal
    bool showDeleteModal = false;
    bool showGalleryModal = false;
    bool galleryModalDraggable = false;
    int32_t deleteTarget = -1; // -1 falls back to the selected task

    // Dashboard
    int32_t selectedBuild = 1;

    // Gallery
    int32_t galleryClicks = 0;
    bool autosave = true;
    bool telemetryLocked = true; // shown via a disabled checkbox
    int32_t quality = 2;         // radio values 1..3
    float volume = 0.4f;
    int32_t density = 1;
    int32_t retryCount = 3;
    char galleryText[128] = "";
    char galleryNotes[512] = "1. Multi-line editing: Enter breaks lines, Up/Down keep their column.\n2. Long lines soft-wrap at word boundaries.\n3. The wheel scrolls this text once it overflows the field.\n4. Selection spans lines; copy/paste keep the newlines.\n5. This line exists so the field overflows.\n6. And this one makes sure of it.";
    int32_t buildConfig = 1;
    int32_t channel = 0;
    bool treeSrcOpen = true;
    bool treeWidgetsOpen = true;
    bool treeAssetsOpen = false;
    int32_t miniTab = 0;

    // Settings
    int32_t themePreset = CLAY_WIDGETS_THEME_PRESET_SLATE;
    bool animationsOn = true;
    bool notifications = true;
    bool verboseLogging = false;
    bool experimentalGpu = false;
    bool showAdvanced = false;
    int32_t logLevel = 2;

    // Status bar
    char statusLine[128] = "Ready";

    // Per-frame scratch for formatted labels. Clay retains Clay_String pointers
    // until render, so these live here (reset each frame) instead of on an
    // inner scope's stack that closes before the frame is drawn.
    char scratch[kScratchSlots][kScratchBytes] = {};
    int32_t scratchUsed = 0;
};

// Textures for the tintable-icon demo, generated procedurally at startup.
struct DemoIcons {
    Texture2D check;
    Texture2D play;
    Texture2D circle;
    Texture2D square;
};

static void SetStatus(DemoState &s, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(s.statusLine, sizeof(s.statusLine), fmt, ap);
    va_end(ap);
}

// Formats into the per-frame scratch ring and returns a Clay_String view of it.
static Clay_String FormatString(DemoState &s, const char *fmt, ...) {
    char *buf = s.scratch[s.scratchUsed % kScratchSlots];
    s.scratchUsed++;

    va_list ap;
    va_start(ap, fmt);
    int n = std::vsnprintf(buf, kScratchBytes, fmt, ap);
    va_end(ap);
    if (n < 0) {
        n = 0;
    }
    if (n > kScratchBytes - 1) {
        n = kScratchBytes - 1;
    }

    Clay_String out = {};
    out.isStaticallyAllocated = false;
    out.length = n;
    out.chars = buf;
    return out;
}

// Small dim caption. The kit only ships Label, so this drops down to a raw
// CLAY_TEXT - which is also how apps are expected to extend the kit.
static void MutedLabel(ClayWidgets_Context &ui, Clay_String text) {
    CLAY_TEXT(text, {
        .textColor = ui.theme.textMutedColor,
        .fontId = ui.theme.fontBody,
        .fontSize = ui.theme.fontSizeSmall,
    });
}

// All demo toasts route through here so the Settings "notifications" toggle
// actually governs app behavior.
static void Notify(ClayWidgets_Context &ui, DemoState &s, Clay_String message, ClayWidgets_BadgeVariant variant, float seconds) {
    if (s.notifications) {
        ClayWidgets_ShowToast(&ui, message, variant, seconds);
    }
}

// Appends a task. Appending never moves existing entries in the fixed array,
// so it is safe to call mid-layout (unlike delete, which is deferred).
static bool AddTask(DemoState &s, const char *title, int32_t priority) {
    if (!title || !title[0] || s.taskCount >= kMaxTasks) {
        return false;
    }
    DemoTask &t = s.tasks[s.taskCount++];
    std::snprintf(t.title, sizeof(t.title), "%s", title);
    t.priority = priority < 0 ? 0 : (priority > 2 ? 2 : priority);
    t.done = false;
    return true;
}

static void SeedTasks(DemoState &s) {
    s.taskCount = 0;
    for (const DemoSeedTask &seed : kSeedTasks) {
        if (AddTask(s, seed.title, seed.priority)) {
            s.tasks[s.taskCount - 1].done = seed.done;
        }
    }
}

static void ResetDemoState(DemoState &s) {
    int32_t view = s.activeView;
    s = DemoState{};
    s.activeView = view;
    SeedTasks(s);
    SetStatus(s, "Demo state reset");
}

// Applies deferred structural edits. Runs at the top of each frame, before any
// layout is declared, so no live Clay_String can point into moved task memory.
static void ApplyPendingTaskEdits(DemoState &s) {
    if (s.pendingReset) {
        ResetDemoState(s);
        return;
    }
    if (s.pendingClearCompleted) {
        s.pendingClearCompleted = false;
        int32_t kept = 0;
        for (int32_t i = 0; i < s.taskCount; ++i) {
            if (!s.tasks[i].done) {
                s.tasks[kept++] = s.tasks[i];
            }
        }
        s.taskCount = kept;
    }
    if (s.pendingDelete >= 0) {
        if (s.pendingDelete < s.taskCount) {
            for (int32_t i = s.pendingDelete; i + 1 < s.taskCount; ++i) {
                s.tasks[i] = s.tasks[i + 1];
            }
            --s.taskCount;
        }
        s.pendingDelete = -1;
    }
    if (s.selectedTask >= s.taskCount) {
        s.selectedTask = s.taskCount - 1;
    }
    if (s.selectedTask < 0) {
        s.selectedTask = 0;
    }
    if (s.contextTask >= s.taskCount) {
        s.contextTask = -1;
    }
    if (s.deleteTarget >= s.taskCount) {
        s.deleteTarget = -1;
    }
}

static int32_t CountCompletedTasks(const DemoState &s) {
    int32_t done = 0;
    for (int32_t i = 0; i < s.taskCount; ++i) {
        if (s.tasks[i].done) {
            ++done;
        }
    }
    return done;
}

static void AddSampleTask(ClayWidgets_Context &ui, DemoState &s) {
    if (AddTask(s, "Explore the Tasks view", 1)) {
        s.selectedTask = s.taskCount - 1;
        SetStatus(s, "Added a sample task");
        Notify(ui, s, CLAY_STRING("Task added"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
    } else {
        SetStatus(s, "Task list is full (%d max)", kMaxTasks);
    }
}

static void DuplicateTask(ClayWidgets_Context &ui, DemoState &s, int32_t index) {
    if (index < 0 || index >= s.taskCount) {
        return;
    }
    char copyTitle[kTaskTitleCap];
    std::snprintf(copyTitle, sizeof(copyTitle), "%s (copy)", s.tasks[index].title);
    if (AddTask(s, copyTitle, s.tasks[index].priority)) {
        s.selectedTask = s.taskCount - 1;
        SetStatus(s, "Duplicated: %s", s.tasks[index].title);
        Notify(ui, s, CLAY_STRING("Task duplicated"), CLAY_WIDGETS_BADGE_ACCENT, 2.5f);
    } else {
        SetStatus(s, "Task list is full (%d max)", kMaxTasks);
    }
}

static void RequestDeleteTask(DemoState &s, int32_t index) {
    if (s.taskCount <= 0) {
        SetStatus(s, "No tasks to delete");
        return;
    }
    s.deleteTarget = (index >= 0 && index < s.taskCount) ? index : s.selectedTask;
    s.showDeleteModal = true;
}

} // namespace

#endif // CLAY_WIDGETS_DEMO_STATE_H
