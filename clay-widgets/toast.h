#ifndef CLAY_WIDGETS_TOAST_H
#define CLAY_WIDGETS_TOAST_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before toast.h"
#endif

// A transient notification ("toast"). Call ClayWidgets_ShowToast when something
// happens (e.g. inside a button handler); it stores the message and a
// countdown. Call ClayWidgets_ToastLayer once per frame near the root - it
// counts the timer down and, while active, draws a floating card at the bottom
// center, on top of everything. Only one toast is shown at a time; a new one
// replaces the old.
//
//   if (ClayWidgets_Button(&ui, CLAY_ID("Save"), CLAY_STRING("Save"))) {
//       ClayWidgets_ShowToast(&ui, CLAY_STRING("Saved"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
//   }
//   ...
//   ClayWidgets_ToastLayer(&ui); // once, at the end of the frame
//
// `variant` reuses the badge palette for the accent stripe/text.
void ClayWidgets_ShowToast(ClayWidgets_Context *ctx, Clay_String message, ClayWidgets_BadgeVariant variant, float durationSeconds);
void ClayWidgets_ToastLayer(ClayWidgets_Context *ctx);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static Clay_Color ClayWidgets__ToastAccent(ClayWidgets_Context *ctx, int32_t variant) {
    switch (variant) {
        case CLAY_WIDGETS_BADGE_SUCCESS: return (Clay_Color){ 46, 160, 67, 255 };
        case CLAY_WIDGETS_BADGE_WARNING: return (Clay_Color){ 191, 135, 0, 255 };
        case CLAY_WIDGETS_BADGE_DANGER:  return (Clay_Color){ 207, 54, 54, 255 };
        case CLAY_WIDGETS_BADGE_NEUTRAL: return ctx->theme.borderColor;
        case CLAY_WIDGETS_BADGE_ACCENT:
        default:                         return ctx->theme.accentColor;
    }
}

void ClayWidgets_ShowToast(ClayWidgets_Context *ctx, Clay_String message, ClayWidgets_BadgeVariant variant, float durationSeconds) {
    if (!ctx) {
        return;
    }
    int32_t capacity = (int32_t)sizeof(ctx->toastMessage) - 1;
    int32_t length = (message.length < capacity) ? message.length : capacity;
    if (length < 0) {
        length = 0;
    }
    for (int32_t i = 0; i < length; ++i) {
        ctx->toastMessage[i] = message.chars[i];
    }
    ctx->toastMessage[length] = '\0';
    ctx->toastLength = length;
    ctx->toastVariant = (int32_t)variant;
    ctx->toastRemaining = durationSeconds > 0.0f ? durationSeconds : 2.0f;
}

void ClayWidgets_ToastLayer(ClayWidgets_Context *ctx) {
    if (!ctx || ctx->toastRemaining <= 0.0f || ctx->toastLength <= 0) {
        return;
    }

    ctx->toastRemaining -= ctx->input.deltaTime;
    if (ctx->toastRemaining <= 0.0f) {
        ctx->toastRemaining = 0.0f;
        return;
    }

    Clay_String message;
    message.chars = ctx->toastMessage;
    message.length = ctx->toastLength;
    message.isStaticallyAllocated = false;

    Clay_Color accent = ClayWidgets__ToastAccent(ctx, ctx->toastVariant);
    Clay_ElementId toastId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsToast"), 1);

    // The accent stripe is the card's own background showing through a left
    // inset: a 4px child can't bend around the card's larger corner radius, so
    // painting it as a stripe element would poke past the rounded silhouette.
    // The surface layer covers everything but the leading 4px, its left
    // corners tightened by the inset so the stripe tracks the card's arc.
    float stripeWidth = 4.0f;
    float rOuter = (float)ctx->theme.radiusMd;
    float rInner = rOuter > stripeWidth ? rOuter - stripeWidth : 0.0f;

    CLAY(toastId, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = { .left = (uint16_t)stripeWidth, .right = 0, .top = 0, .bottom = 0 },
        },
        .backgroundColor = accent,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
        .floating = {
            .offset = { .x = 0.0f, .y = -28.0f },
            .zIndex = 600,
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_CENTER_BOTTOM,
                .parent = CLAY_ATTACH_POINT_CENTER_BOTTOM,
            },
            .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_GROW(0) },
                .padding = {
                    .left = (uint16_t)(ctx->theme.spacing.md + ctx->theme.spacing.xs),
                    .right = ctx->theme.spacing.lg,
                    .top = ctx->theme.spacing.md,
                    .bottom = ctx->theme.spacing.md,
                },
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = ctx->theme.surfaceAltColor,
            .cornerRadius = { .topLeft = rInner, .topRight = rOuter, .bottomLeft = rInner, .bottomRight = rOuter },
        }) {
            CLAY_TEXT(message, {
                .textColor = ctx->theme.textColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            });
        }
    }
}

#endif

#endif
