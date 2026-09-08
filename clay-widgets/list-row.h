#ifndef CLAY_WIDGETS_LIST_ROW_H
#define CLAY_WIDGETS_LIST_ROW_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before list-row.h"
#endif

// A standalone selectable list row: a full-width clickable row with an optional
// leading color swatch, a primary label, and an optional dim right-aligned
// trailing string. Unlike ListBox, you own the loop, the per-row id, and the
// selection state, so rows can carry a swatch and arbitrary ids. Selected rows
// use the accent-muted fill; hover uses the hover fill (both eased). Pass a
// zero-alpha swatch to omit the swatch; pass a zero-length trailing to omit it.
// Returns true the frame the row is clicked.
bool ClayWidgets_SelectRow(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_Color swatch, Clay_String text, bool selected);
bool ClayWidgets_SelectRowEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_Color swatch, Clay_String text, Clay_String trailing, bool selected);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

bool ClayWidgets_SelectRowEx(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_Color swatch, Clay_String text, Clay_String trailing, bool selected) {
    if (!ctx) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    // List rows are not tab-focusable (matching the ListBox row convention), so
    // no focus is registered here - only a pointer click selects them.
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);

    Clay_Color rowBg = ClayWidgets__FadeToClear(ctx->theme.hoverColor);
    if (selected) {
        rowBg = ctx->theme.accentMutedColor;
    } else if (over) {
        rowBg = ctx->theme.hoverColor;
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = {
                .left = ctx->theme.spacing.sm,
                .right = ctx->theme.spacing.sm,
                .top = ctx->theme.spacing.xs,
                .bottom = ctx->theme.spacing.xs,
            },
            .childGap = ctx->theme.spacing.sm,
            .childAlignment = { .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = rowBg,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
        .transition = ClayWidgets__ColorTransition(ctx),
    }) {
        if (swatch.a > 0) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIXED(14),
                        .height = CLAY_SIZING_FIXED(14),
                    },
                },
                .backgroundColor = swatch,
                .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
            }) {}
        }

        CLAY_TEXT(text, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .wrapMode = CLAY_TEXT_WRAP_NONE,
        });

        if (trailing.length > 0) {
            // A grow spacer pushes the trailing string to the right edge.
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_FIXED(1),
                    },
                },
            }) {}

            CLAY_TEXT(trailing, {
                .textColor = ctx->theme.textMutedColor,
                .fontId = ctx->theme.fontBody,
                .fontSize = ctx->theme.fontSizeSmall,
                .wrapMode = CLAY_TEXT_WRAP_NONE,
            });
        }
    }

    ClayWidgets__Describe(ctx,id,CLAY_WIDGETS_ROLE_ROW,text,selected,false);
    return clicked;
}

bool ClayWidgets_SelectRow(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_Color swatch, Clay_String text, bool selected) {
    return ClayWidgets_SelectRowEx(ctx, id, swatch, text, (Clay_String){0}, selected);
}

#endif

#endif
