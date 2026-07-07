#ifndef CLAY_WIDGETS_SCROLL_PANEL_H
#define CLAY_WIDGETS_SCROLL_PANEL_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before scroll-panel.h"
#endif

// A scroll panel is a themed surface whose scrollable content is inset by a
// vertical fade margin (content clips before the panel's outer edge) and by a
// right-hand gutter (content never renders underneath the scrollbar). The
// scrollbar is emitted automatically. Usage:
//
//   ClayWidgets_BeginScrollPanel(ctx, id, options);
//   ... child widgets ...
//   ClayWidgets_EndScrollPanel(ctx, id);
//
// Note: scroll panels are not designed to be nested inside one another.
Clay_ElementId ClayWidgets_BeginScrollPanel(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    ClayWidgets_ScrollPanelOptions options
);
void ClayWidgets_EndScrollPanel(ClayWidgets_Context *ctx, Clay_ElementId id);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static Clay_ElementId ClayWidgets__ScrollPanelContentId(Clay_ElementId id) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollPanelContent"), id.id);
}

Clay_ElementId ClayWidgets_BeginScrollPanel(
    ClayWidgets_Context *ctx,
    Clay_ElementId id,
    ClayWidgets_ScrollPanelOptions options
) {
    Clay_ElementId contentId = ClayWidgets__ScrollPanelContentId(id);
    if (!ctx) {
        return contentId;
    }

    // Record this panel's scroll container so clip widgets nested inside it (a
    // table, a text field) can forward a swallowed wheel back to it.
    if (ctx->scrollPanelDepth < CLAY_WIDGETS_MAX_SCROLL_NESTING) {
        ctx->scrollPanelStack[ctx->scrollPanelDepth] = contentId.id;
    }
    ctx->scrollPanelDepth++;

    uint16_t fadeMargin = options.fadeMargin > 0 ? options.fadeMargin : ctx->theme.spacing.lg;
    uint16_t padding = options.padding > 0 ? options.padding : ctx->theme.spacing.md;
    uint16_t childGap = options.childGap > 0 ? options.childGap : ctx->theme.spacing.md;
    // Reserve room for the floating scrollbar (its width plus a small gap) so
    // scrollable content never sits underneath it.
    uint16_t scrollbarGutter = (uint16_t)(CLAY_WIDGETS_SCROLLBAR_WIDTH + ctx->theme.spacing.sm);

    // Outer surface: draws the background and provides the fade margin via padding.
    Clay__OpenElementWithId(id);
    Clay__ConfigureOpenElement(CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = options.width, .height = options.height },
            .padding = { .left = padding, .right = padding, .top = fadeMargin, .bottom = fadeMargin },
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(ctx->theme.radiusMd),
    });

    // Inner clip element: this is the actual scroll container. Clay scissors to
    // this element's box, which is inset from the surface by the fade margin, so
    // content disappears before the panel edge.
    Clay__OpenElementWithId(contentId);
    Clay__ConfigureOpenElement(CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0) },
            .padding = { .right = scrollbarGutter },
            .childGap = childGap,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
    });

    return contentId;
}

void ClayWidgets_EndScrollPanel(ClayWidgets_Context *ctx, Clay_ElementId id) {
    if (!ctx) {
        return;
    }
    if (ctx->scrollPanelDepth > 0) {
        ctx->scrollPanelDepth--;
    }
    ClayWidgets_ScrollBar(ctx, ClayWidgets__ScrollPanelContentId(id));
    Clay__CloseElement(); // inner clip content
    Clay__CloseElement(); // outer surface
}

#endif

#endif
