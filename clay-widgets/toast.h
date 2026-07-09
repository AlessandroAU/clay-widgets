#ifndef CLAY_WIDGETS_TOAST_H
#define CLAY_WIDGETS_TOAST_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before toast.h"
#endif

// Transient notifications ("toasts"). Call ClayWidgets_ShowToast when
// something happens (e.g. inside a button handler); it appends the message and
// a countdown to a small queue (CLAY_WIDGETS_MAX_TOASTS, oldest evicted when
// full). Call ClayWidgets_ToastLayer once per frame near the root - it counts
// the timers down and, while any are active, draws a floating stack of cards
// at the bottom center, on top of everything, newest nearest the screen edge.
//
//   if (ClayWidgets_Button(&ui, CLAY_ID("Save"), CLAY_STRING("Save"))) {
//       ClayWidgets_ShowToast(&ui, CLAY_STRING("Saved"), CLAY_WIDGETS_BADGE_SUCCESS, 2.5f);
//   }
//   ...
//   ClayWidgets_ToastLayer(&ui); // once, at the end of the frame
//
// `variant` reuses the badge palette for the accent stripe.
void ClayWidgets_ShowToast(ClayWidgets_Context *ctx, Clay_String message, ClayWidgets_BadgeVariant variant, float durationSeconds);
void ClayWidgets_ToastLayer(ClayWidgets_Context *ctx);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

void ClayWidgets_ShowToast(ClayWidgets_Context *ctx, Clay_String message, ClayWidgets_BadgeVariant variant, float durationSeconds) {
    if (!ctx) {
        return;
    }

    // Queue full: evict the oldest so the newest message always shows.
    if (ctx->toastCount >= CLAY_WIDGETS_MAX_TOASTS) {
        memmove(&ctx->toasts[0], &ctx->toasts[1], (size_t)(CLAY_WIDGETS_MAX_TOASTS - 1) * sizeof(ctx->toasts[0]));
        ctx->toastCount = CLAY_WIDGETS_MAX_TOASTS - 1;
    }

    ClayWidgets_ToastSlot *slot = &ctx->toasts[ctx->toastCount];
    int32_t capacity = (int32_t)sizeof(slot->message) - 1;
    int32_t length = (message.length < capacity) ? message.length : capacity;
    if (length < 0) {
        length = 0;
    }
    for (int32_t i = 0; i < length; ++i) {
        slot->message[i] = message.chars[i];
    }
    slot->message[length] = '\0';
    slot->length = length;
    slot->variant = (int32_t)variant;
    slot->remaining = durationSeconds > 0.0f ? durationSeconds : 2.0f;
    ctx->toastCount++;
}

void ClayWidgets_ToastLayer(ClayWidgets_Context *ctx) {
    if (!ctx || ctx->toastCount <= 0) {
        return;
    }

    // Count down and compact away expired slots, keeping arrival order.
    int32_t alive = 0;
    for (int32_t i = 0; i < ctx->toastCount; ++i) {
        ctx->toasts[i].remaining -= ctx->input.deltaTime;
        if (ctx->toasts[i].remaining > 0.0f && ctx->toasts[i].length > 0) {
            if (alive != i) {
                ctx->toasts[alive] = ctx->toasts[i];
            }
            alive++;
        }
    }
    ctx->toastCount = alive;
    if (ctx->toastCount <= 0) {
        return;
    }

    // The accent stripe is each card's own background showing through a left
    // inset: a 4px child can't bend around the card's larger corner radius, so
    // painting it as a stripe element would poke past the rounded silhouette.
    // The surface layer covers everything but the leading 4px, its left
    // corners tightened by the inset so the stripe tracks the card's arc.
    float stripeWidth = 4.0f;
    float rOuter = (float)ctx->theme.radiusMd;
    float rInner = rOuter > stripeWidth ? rOuter - stripeWidth : 0.0f;

    // One floating column holds the stack: oldest on top, newest nearest the
    // bottom edge. Pointer-passthrough so toasts never block the UI beneath.
    Clay_ElementId stackId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsToastStack"), 1);
    CLAY(stackId, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
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
    }) {
        for (int32_t i = 0; i < ctx->toastCount; ++i) {
            ClayWidgets_ToastSlot *slot = &ctx->toasts[i];

            Clay_String message;
            message.chars = slot->message;
            message.length = slot->length;
            message.isStaticallyAllocated = false;

            Clay_Color accent = ClayWidgets__SemanticColor(ctx, (ClayWidgets_BadgeVariant)slot->variant);
            Clay_ElementId toastId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsToast"), (uint32_t)(i + 1));

            CLAY(toastId, {
                .layout = {
                    .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
                    .padding = { .left = (uint16_t)stripeWidth, .right = 0, .top = 0, .bottom = 0 },
                },
                .backgroundColor = accent,
                .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
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
    }
}

#endif

#endif
