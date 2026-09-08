#ifndef CLAY_WIDGETS_SCROLL_BAR_H
#define CLAY_WIDGETS_SCROLL_BAR_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before scroll-bar.h"
#endif

void ClayWidgets_ScrollBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId
);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static void ClayWidgets__ScrollBarAt(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId,
    float offsetX,
    bool tableColumn
);

// Where the pointer and the scroll position were when a thumb drag started,
// kept per thumb in the state pool for the duration of the drag.
typedef struct ClayWidgets__ScrollBarDragState {
    float startMouseY;
    float startScrollY;
} ClayWidgets__ScrollBarDragState;

void ClayWidgets_ScrollBar(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId
) {
    ClayWidgets__ScrollBarAt(ctx, scrollContainerId, 3.0f, false);
}

static void ClayWidgets__ScrollBarAt(
    ClayWidgets_Context *ctx,
    Clay_ElementId scrollContainerId,
    float offsetX,
    bool tableColumn
) {
    if (!ctx) {
        return;
    }

    Clay_ScrollContainerData scrollData = Clay_GetScrollContainerData(scrollContainerId);
    if (!scrollData.found || !scrollData.scrollPosition) {
        return;
    }

    float containerHeight = scrollData.scrollContainerDimensions.height;
    float contentHeight = scrollData.contentDimensions.height;

    if (containerHeight <= 0.0f || contentHeight <= 0.0f) {
        return;
    }

    float scrollableHeight = contentHeight - containerHeight;
    if (scrollableHeight <= 0.0f) {
        return;
    }

    const float trackWidth = (float)CLAY_WIDGETS_SCROLLBAR_WIDTH + (tableColumn ? 4 : 0);
    const float trackPadding = 1.0f;
    float trackInnerHeight = containerHeight - trackPadding * 2.0f;
    if (trackInnerHeight <= 1.0f) {
        return;
    }

    float scrollProgress = ClayWidgets__Clamp((-scrollData.scrollPosition->y) / scrollableHeight, 0.0f, 1.0f);
    float scrollBarHeight = (containerHeight / contentHeight) * trackInnerHeight;
    scrollBarHeight = ClayWidgets__Clamp(scrollBarHeight, 10.0f, trackInnerHeight);
    float thumbTravel = trackInnerHeight - scrollBarHeight;
    float scrollBarY = scrollProgress * thumbTravel;

    Clay_ElementId scrollBarTrackId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarTrack"), scrollContainerId.id);
    Clay_ElementId scrollBarId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsScrollBarThumb"), scrollContainerId.id);
    bool overThumb = Clay_PointerOver(scrollBarId);
    // Keep the pointer cursor for the whole drag, even when the pointer
    // wanders off the thumb mid-drag.
    if (overThumb || ctx->activeId == scrollBarId.id) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }

    // Drag state lives in the per-widget pool, claimed only while this thumb
    // is pressed or being dragged so idle scrollbars don't occupy slots. A
    // NULL from an exhausted pool just means the thumb can't drag this frame.
    ClayWidgets__ScrollBarDragState *drag = NULL;
    if ((ctx->input.pointerPressed && overThumb) || ctx->activeId == scrollBarId.id) {
        drag = (ClayWidgets__ScrollBarDragState *)ClayWidgets_GetState(ctx, scrollBarId.id, (int32_t)sizeof(*drag));
    }

    if (ctx->input.pointerPressed && overThumb && drag) {
        ctx->activeId = scrollBarId.id;
        drag->startMouseY = ctx->input.mouseY;
        drag->startScrollY = scrollData.scrollPosition->y;
    }

    bool draggingThumb = drag
        && ctx->input.pointerDown
        && ctx->activeId == scrollBarId.id;

    if (draggingThumb) {
        if (thumbTravel > 0.0f) {
            float mouseDeltaY = ctx->input.mouseY - drag->startMouseY;
            float newScrollY = drag->startScrollY - (mouseDeltaY / thumbTravel) * scrollableHeight;
            scrollData.scrollPosition->y = ClayWidgets__Clamp(newScrollY, -scrollableHeight, 0.0f);

            scrollProgress = ClayWidgets__Clamp((-scrollData.scrollPosition->y) / scrollableHeight, 0.0f, 1.0f);
            scrollBarY = scrollProgress * thumbTravel;
        }
    }

    if (ctx->input.pointerReleased && ctx->activeId == scrollBarId.id) {
        ctx->activeId = 0;
    }

    CLAY(scrollBarTrackId, {
        .layout = {
            .sizing = {
                .width = CLAY_SIZING_FIXED(trackWidth),
                .height = CLAY_SIZING_FIXED(containerHeight),
            },
            .padding = { .left = (uint16_t)(tableColumn ? 3 : 1), .right = (uint16_t)(tableColumn ? 3 : 1),
                .top = (uint16_t)trackPadding, .bottom = (uint16_t)trackPadding },
            .childGap = 0,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = CLAY_CORNER_RADIUS(tableColumn ? 0.0f : 4.0f),
        .floating = {
            .offset = { .x = offsetX, .y = 0.0f },
            .parentId = scrollContainerId.id,
            .zIndex = ClayWidgets__OverlayZ(ctx, 20),
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_RIGHT_TOP,
                .parent = CLAY_ATTACH_POINT_RIGHT_TOP,
            },
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
            // Clip to whatever clips the scroll container itself, so a
            // scrollbar on a widget nested inside a scroll panel (e.g. a text
            // area) is cut at the panel edge along with its widget instead of
            // floating over the elements outside. For a top-level panel the
            // container has no enclosing clip and this is a no-op.
            .clipTo = CLAY_CLIP_TO_ATTACHED_PARENT,
        },
        .border = {
            .color = ctx->theme.borderColor,
            // The table gutter owns the shared left edge.
            .width = { .left = (uint16_t)(tableColumn ? 0 : 1), .right = (uint16_t)(tableColumn ? 0 : 1),
                .top = 1, .bottom = (uint16_t)(tableColumn ? 0 : 1) },
        },
    }) {
        if (scrollBarY > 0.0f) {
            CLAY_AUTO_ID({
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_FIXED(scrollBarY),
                    },
                },
            }) {}
        }

        CLAY(scrollBarId, {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_FIXED(scrollBarHeight),
                },
            },
            .backgroundColor = draggingThumb ? ctx->theme.accentColor : (overThumb ? ctx->theme.accentMutedColor : ctx->theme.borderColor),
            .cornerRadius = CLAY_CORNER_RADIUS(3),
        }) {}
    }
}

#endif

#endif
