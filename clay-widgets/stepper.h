#ifndef CLAY_WIDGETS_STEPPER_H
#define CLAY_WIDGETS_STEPPER_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before stepper.h"
#endif

// A numeric stepper: [-] value [+], joined into one bordered control. Clicking
// the buttons steps the caller-owned int by options.step, clamped to
// [minValue, maxValue]; when focused, Up/Right increment and Down/Left
// decrement. Returns true on the frame the value changes.
//
//   ClayWidgets_Stepper(&ui, CLAY_ID("Count"), &count,
//                       ClayWidgets_StepperOptions{ 0, 99, 1 });
bool ClayWidgets_Stepper(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    int32_t *value,
    ClayWidgets_StepperOptions options
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_Stepper(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    int32_t *value,
    ClayWidgets_StepperOptions options
) {
    if (!ctx || !value) {
        return false;
    }
    options.disabled = options.disabled || ctx->disabledDepth > 0;
    if (options.disabled && ctx->activeId == id.id) ctx->activeId = 0;


    int32_t minValue = options.minValue;
    int32_t maxValue = options.maxValue;
    if (maxValue < minValue) {
        int32_t tmp = minValue; minValue = maxValue; maxValue = tmp;
    }
    int32_t step = options.step > 0 ? options.step : 1;

    Clay_ElementId minusId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsStepperMinus"), id.id);
    Clay_ElementId plusId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsStepperPlus"), id.id);

    bool over = options.disabled ? false : Clay_PointerOver(id);
    if (!options.disabled && (Clay_PointerOver(minusId) || Clay_PointerOver(plusId))) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER); // only the +/- buttons are click targets
    }
    bool focused = options.disabled ? false : ClayWidgets__RegisterFocusable(ctx, id, over);
    bool changed = false;

    int32_t start = *value;
    int64_t candidate = *value;
    if (!options.disabled) {
        if (ClayWidgets__ConsumeClick(ctx, Clay_PointerOver(minusId))) {
            candidate -= step;
        }
        if (ClayWidgets__ConsumeClick(ctx, Clay_PointerOver(plusId))) {
            candidate += step;
        }
        if (focused) {
            if (ctx->input.keyUp || ctx->input.keyRight) {
                candidate += step;
            }
            if (ctx->input.keyDown || ctx->input.keyLeft) {
                candidate -= step;
            }
        }
    }
    *value = (int32_t)(candidate < minValue ? minValue : candidate > maxValue ? maxValue : candidate);
    changed = (*value != start);

    float r = (float)ctx->theme.radiusSm;
    bool minusOver = !options.disabled && Clay_PointerOver(minusId);
    bool plusOver = !options.disabled && Clay_PointerOver(plusId);
    Clay_Color buttonFace = options.disabled
        ? ClayWidgets__MixColor(ctx->theme.surfaceAltColor, ctx->theme.surfaceColor, ctx->theme.disabledMix)
        : ctx->theme.surfaceAltColor;
    Clay_Color glyphColor = options.disabled ? ctx->theme.textMutedColor : ctx->theme.textColor;

    // Two raised spin buttons around a sunken value well - the classic spinner.
    Clay_ElementId valueId = ClayWidgets__ChildId(id, CLAY_STRING("ClayWidgetsStepperValue"), 0);
    bool beveled = ClayWidgets__IsBeveled(ctx);
    ClayWidgets_SetEdge(ctx, minusId, minusOver && ctx->input.pointerDown ? CLAY_WIDGETS_EDGE_SUNKEN : CLAY_WIDGETS_EDGE_RAISED);
    ClayWidgets_SetEdge(ctx, plusId, plusOver && ctx->input.pointerDown ? CLAY_WIDGETS_EDGE_SUNKEN : CLAY_WIDGETS_EDGE_RAISED);
    ClayWidgets_SetEdge(ctx, valueId, CLAY_WIDGETS_EDGE_SUNKEN);
    if (focused) {
        ClayWidgets__FocusRect(ctx, valueId);
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .childGap = 0,
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .cornerRadius = CLAY_CORNER_RADIUS(r),
        .border = ClayWidgets__Border(ctx, focused ? ctx->theme.focusRingColor : ctx->theme.borderColor),
    }) {
        // Minus button.
        CLAY(minusId, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(34), .height = CLAY_SIZING_FIT(0, 0) },
                .padding = { .left = 0, .right = 0, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = minusOver ? ctx->theme.hoverColor : buttonFace,
            .cornerRadius = { .topLeft = r, .topRight = 0.0f, .bottomLeft = r, .bottomRight = 0.0f },
        }) {
            CLAY_TEXT(CLAY_STRING("-"), {
                .textColor = glyphColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }

        // Value display.
        CLAY(valueId, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIT(50, 0), .height = CLAY_SIZING_GROW(0) },
                .padding = { .left = ctx->theme.spacing.sm, .right = ctx->theme.spacing.sm, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = beveled ? ctx->theme.fieldColor : buttonFace,
            .border = ClayWidgets__EdgeBorder(ctx, ctx->theme.borderColor, CLAY__INIT(Clay_BorderWidth){ 1, 1, 0, 0, 0 }),
        }) {
            CLAY_TEXT(ClayWidgets__ScratchInt(ctx, *value), {
                .textColor = glyphColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
                .textAlignment = CLAY_TEXT_ALIGN_CENTER,
            });
        }

        // Plus button.
        CLAY(plusId, {
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(34), .height = CLAY_SIZING_FIT(0, 0) },
                .padding = { .left = 0, .right = 0, .top = ctx->theme.spacing.sm, .bottom = ctx->theme.spacing.sm },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
            .backgroundColor = plusOver ? ctx->theme.hoverColor : buttonFace,
            .cornerRadius = { .topLeft = 0.0f, .topRight = r, .bottomLeft = 0.0f, .bottomRight = r },
        }) {
            CLAY_TEXT(CLAY_STRING("+"), {
                .textColor = glyphColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeBody,
            });
        }
    }

    return changed;
}

#endif

#endif
