#ifndef CLAY_WIDGETS_TREE_H
#define CLAY_WIDGETS_TREE_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before tree.h"
#endif

// Tree view rows. The caller owns the hierarchy and recurses; these draw one
// indented row each. TreeNode is a branch: it shows a twisty, toggles the
// caller's `expanded` bool on click, and returns the (possibly updated) expanded
// state - declare the node's children only when it returns true, passing
// depth + 1. TreeLeaf is a leaf row (indented to align under a branch's label)
// and returns true on the frame it is clicked. Depth is 0 for roots.
//
//   if (ClayWidgets_TreeNode(&ui, CLAY_ID("Src"), CLAY_STRING("src"), 0, &srcOpen)) {
//       ClayWidgets_TreeLeaf(&ui, CLAY_ID("Main"), CLAY_STRING("main.cpp"), 1);
//   }
bool ClayWidgets_TreeNode(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label, int32_t depth, bool *expanded);
bool ClayWidgets_TreeLeaf(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label, int32_t depth);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

// Renders one indented tree row. `twisty` is the disclosure glyph for branches
// or an empty string for leaves (which still reserve the glyph's width so labels
// line up). Returns true when the row is clicked.
static bool ClayWidgets__TreeRow(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label, int32_t depth, Clay_String twisty) {
    bool over = Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);

    if (depth < 0) {
        depth = 0;
    }
    uint16_t indent = (uint16_t)(ctx->theme.spacing.sm + depth * 18);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = { .left = indent, .right = ctx->theme.spacing.sm, .top = ctx->theme.spacing.xs, .bottom = ctx->theme.spacing.xs },
            .childGap = ctx->theme.spacing.xs,
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
            .layoutDirection = CLAY_LEFT_TO_RIGHT,
        },
        .backgroundColor = over ? ctx->theme.hoverColor : ClayWidgets__FadeToClear(ctx->theme.hoverColor),
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
        .transition = ClayWidgets__ColorTransition(ctx),
    }) {
        CLAY_AUTO_ID({
            .layout = {
                .sizing = { .width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIT(0, 0) },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
            },
        }) {
            if (twisty.length > 0 && twisty.chars) {
                CLAY_TEXT(twisty, {
                    .textColor = ctx->theme.textMutedColor,
                    .fontId = ctx->theme.fontBody,
                    .fontSize = ctx->theme.fontSizeBody,
                });
            }
        }
        CLAY_TEXT(label, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .wrapMode = CLAY_TEXT_WRAP_NONE,
        });
    }

    return clicked;
}

bool ClayWidgets_TreeNode(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label, int32_t depth, bool *expanded) {
    if (!ctx || !expanded) {
        return false;
    }
    Clay_String twisty = *expanded ? CLAY_STRING("v") : CLAY_STRING(">");
    if (ClayWidgets__TreeRow(ctx, id, label, depth, twisty)) {
        *expanded = !(*expanded);
    }
    return *expanded;
}

bool ClayWidgets_TreeLeaf(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label, int32_t depth) {
    if (!ctx) {
        return false;
    }
    Clay_String none = { 0 };
    return ClayWidgets__TreeRow(ctx, id, label, depth, none);
}

#endif

#endif
