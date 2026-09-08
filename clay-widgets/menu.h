#ifndef CLAY_WIDGETS_MENU_H
#define CLAY_WIDGETS_MENU_H

#ifndef CLAY_WIDGETS_H
#error "Include widgets.h before menu.h"
#endif

// A drop-down menu, as used in a menu bar. Lay several BeginMenu blocks out in a
// horizontal row to form the bar; each renders a clickable title and, while
// open, a floating panel of items beneath it. Usage:
//
//   CLAY(CLAY_ID("MenuBar"), { ...horizontal row... }) {
//       if (ClayWidgets_BeginMenu(&ui, CLAY_ID("FileMenu"), CLAY_STRING("File"))) {
//           if (ClayWidgets_MenuItem(&ui, CLAY_ID("New"),  CLAY_STRING("New")))  { ... }
//           if (ClayWidgets_MenuItem(&ui, CLAY_ID("Open"), CLAY_STRING("Open"))) { ... }
//           ClayWidgets_MenuSeparator(&ui);
//           if (ClayWidgets_MenuItem(&ui, CLAY_ID("Quit"), CLAY_STRING("Quit"))) { ... }
//           ClayWidgets_EndMenu(&ui, CLAY_ID("FileMenu"));
//       }
//   }
//
// Only one menu is open at a time. Clicking a title toggles it; once any menu is
// open, moving the pointer onto another title switches to it. A menu closes when
// an item is chosen, when the pointer presses outside it, or on Escape.
// MenuItem returns true on the frame it is chosen (and closes the menu).
bool ClayWidgets_BeginMenu(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title);
void ClayWidgets_EndMenu(ClayWidgets_Context *ctx, Clay_ElementId id);
bool ClayWidgets_MenuItem(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label);
void ClayWidgets_MenuSeparator(ClayWidgets_Context *ctx);

// Right-click context menu. Uses the same MenuItem / MenuSeparator rows as the
// menu bar, but the panel floats at a stored cursor position instead of under a
// title. Typical use:
//
//   // after declaring some region element with id CLAY_ID("Canvas"):
//   if (ClayWidgets_RightClicked(&ui, CLAY_ID("Canvas"))) {
//       ClayWidgets_OpenContextMenu(&ui, CLAY_ID("CanvasMenu"), ui.input.mouseX, ui.input.mouseY);
//   }
//   if (ClayWidgets_BeginContextMenu(&ui, CLAY_ID("CanvasMenu"))) {
//       if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxCut"),  CLAY_STRING("Cut")))  { ... }
//       if (ClayWidgets_MenuItem(&ui, CLAY_ID("CtxCopy"), CLAY_STRING("Copy"))) { ... }
//       ClayWidgets_EndContextMenu(&ui, CLAY_ID("CanvasMenu"));
//   }
//
// The menu closes when an item is chosen, on a left press outside it, or on
// Escape.
bool ClayWidgets_RightClicked(ClayWidgets_Context *ctx, Clay_ElementId id);
void ClayWidgets_OpenContextMenu(ClayWidgets_Context *ctx, Clay_ElementId menuId, float x, float y);
bool ClayWidgets_BeginContextMenu(ClayWidgets_Context *ctx, Clay_ElementId menuId);
void ClayWidgets_EndContextMenu(ClayWidgets_Context *ctx, Clay_ElementId menuId);

#ifdef CLAY_WIDGETS_IMPLEMENTATION

static Clay_ElementId ClayWidgets__MenuDropdownId(Clay_ElementId id) {
    return Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsMenuDropdown"), id.id);
}

static void ClayWidgets__MenuNavigation(ClayWidgets_Context *ctx, uint32_t id) {
    ctx->menuOpening = ctx->menuNavigationId != id;
    if (ctx->menuOpening) {
        ctx->menuNavigationId = id;
        ctx->menuItemCount = 0;
    } else if (ctx->menuItemCount > 0 && (ctx->input.keyDown || ctx->input.keyUp || ctx->input.keyHome || ctx->input.keyEnd)) {
        int32_t index = -1;
        for (int32_t i = 0; i < ctx->menuItemCount; ++i) if (ctx->menuItems[i] == ctx->focusedId) index = i;
        if (ctx->input.keyHome) index = 0;
        else if (ctx->input.keyEnd) index = ctx->menuItemCount - 1;
        else index = (index + (ctx->input.keyUp ? ctx->menuItemCount - 1 : 1)) % ctx->menuItemCount;
        ctx->focusedId = ctx->menuItems[index < 0 ? ctx->menuItemCount - 1 : index];
    }
    ctx->menuItemCount = 0;
}

bool ClayWidgets_BeginMenu(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String title) {
    if (!ctx) {
        return false;
    }

    Clay_ElementId dropdownId = ClayWidgets__MenuDropdownId(id);
    if (ctx->menuTriggerCount < 32) ctx->menuTriggers[ctx->menuTriggerCount++] = id.id;
    if (ctx->openMenuId == id.id && ctx->menuTriggerCountPrev > 1 && (ctx->input.keyLeft || ctx->input.keyRight)) {
        for (int32_t i=0;i<ctx->menuTriggerCountPrev;++i) if (ctx->menuTriggers[i]==id.id) {
            int32_t next = (i + (ctx->input.keyLeft ? ctx->menuTriggerCountPrev - 1 : 1)) % ctx->menuTriggerCountPrev;
            ctx->openMenuId = ctx->menuTriggers[next]; ctx->focusedId = ctx->openMenuId;
            ctx->input.keyLeft = ctx->input.keyRight = false; break;
        }
    }
    bool overTrigger = !ctx->disabledDepth && Clay_PointerOver(id);
    if (overTrigger) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool isOpen = (ctx->openMenuId == id.id);
    bool focused = ClayWidgets__RegisterFocusable(ctx, id, overTrigger);
    if (focused && (ClayWidgets__ActivateFocused(ctx, id) || ctx->input.keyDown)) {
        ctx->openMenuId = id.id;
    }

    // Click toggles this menu; while a *different* menu is open, hovering this
    // title switches to it (classic menu-bar behavior).
    if (ClayWidgets__ConsumeClick(ctx, overTrigger)) {
        ctx->openMenuId = isOpen ? 0 : id.id;
    } else if (ctx->openMenuId != 0 && ctx->openMenuId != id.id && overTrigger) {
        ctx->openMenuId = id.id;
    }
    isOpen = (ctx->openMenuId == id.id);

    // Dismiss on a press outside the title and its panel, or on Escape.
    if (isOpen) {
        if (ctx->input.pointerPressed && !overTrigger && !Clay_PointerOver(dropdownId)) {
            ctx->openMenuId = 0;
            isOpen = false;
        } else if (ctx->input.keyEscape) {
            ctx->openMenuId = 0;
            isOpen = false;
            ctx->focusedId = id.id;
            ctx->input.keyEscape = false;
        }
    }

    Clay_Color triggerBg = (isOpen || overTrigger) ? ctx->theme.hoverColor : ClayWidgets__FadeToClear(ctx->theme.hoverColor);

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(0, 0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = {
                .left = ctx->theme.spacing.md,
                .right = ctx->theme.spacing.md,
                .top = ctx->theme.spacing.sm,
                .bottom = ctx->theme.spacing.sm,
            },
            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = triggerBg,
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
        .transition = ClayWidgets__ColorTransition(ctx),
    }) {
        CLAY_TEXT(title, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .wrapMode = CLAY_TEXT_WRAP_NONE,
        });
    }

    if (!isOpen) {
        if (ctx->menuNavigationId == id.id) ctx->menuNavigationId = 0;
        return false;
    }
    if (!ClayWidgets__PushOverlay(ctx, 300)) return false;
    ClayWidgets__MenuNavigation(ctx, id.id);

    // Floating item panel, anchored under the title's bottom-left corner.
    float r = (float)ctx->theme.radiusSm;
    Clay_ElementData triggerBox = Clay_GetElementData(id), dropBox = Clay_GetElementData(dropdownId);
    float width = dropBox.found ? dropBox.boundingBox.width : 180;
    float height = dropBox.found ? dropBox.boundingBox.height : 180;
    bool up = triggerBox.boundingBox.y + triggerBox.boundingBox.height + height > ctx->layoutDimensions.height
        && triggerBox.boundingBox.y > ctx->layoutDimensions.height / 2;
    float xOffset = fminf(0, ctx->layoutDimensions.width - triggerBox.boundingBox.x - width);
    float maxHeight = fmaxf(1, up ? triggerBox.boundingBox.y - 4 : ctx->layoutDimensions.height - triggerBox.boundingBox.y - triggerBox.boundingBox.height - 4);
    ClayWidgets__BeginScrollElement(dropdownId, CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(180, ctx->layoutDimensions.width), .height = CLAY_SIZING_FIT(0, maxHeight) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.xs),
            .childGap = 0,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = (Clay_CornerRadius){ r, r, r, r },
        .floating = {
            .offset = { .x = xOffset, .y = up ? -4.0f : 4.0f },
            .parentId = id.id,
            .zIndex = ClayWidgets__OverlayZ(ctx, 0),
            .attachPoints = {
                .element = up ? CLAY_ATTACH_POINT_LEFT_BOTTOM : CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = up ? CLAY_ATTACH_POINT_LEFT_TOP : CLAY_ATTACH_POINT_LEFT_BOTTOM,
            },
            .attachTo = CLAY_ATTACH_TO_ELEMENT_WITH_ID,
        },
        .clip = { .vertical = true },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    });

    return true;
}

void ClayWidgets_EndMenu(ClayWidgets_Context *ctx, Clay_ElementId id) {
    (void)id;
    if (!ctx) {
        return;
    }
    ClayWidgets__EndElement(); // dropdown panel
    ClayWidgets__PopOverlay(ctx);
}

bool ClayWidgets_MenuItem(ClayWidgets_Context *ctx, Clay_ElementId id, Clay_String label) {
    if (!ctx) {
        return false;
    }

    bool over = Clay_PointerOver(id);
    if (over) {
        ClayWidgets__SetCursor(ctx, CLAY_WIDGETS_CURSOR_POINTER);
    }
    bool clicked = ClayWidgets__ConsumeClick(ctx, over);
    if (!ctx->disabledDepth) {
        if (ctx->menuOpening && ctx->menuItemCount == 0) ctx->focusedId = id.id;
        ClayWidgets__RegisterFocusable(ctx, id, over);
        if (ctx->menuItemCount < CLAY_WIDGETS_MAX_FOCUSABLES) ctx->menuItems[ctx->menuItemCount++] = id.id;
        if (!ctx->menuOpening && ClayWidgets__ActivateFocused(ctx, id)) clicked = true;
    }
    if (clicked) {
        // Choosing an item dismisses whichever menu it lives in (bar or context).
        uint32_t restoreFocus = ctx->openContextMenuId ? ctx->contextMenuReturnFocus : ctx->openMenuId;
        ctx->openMenuId = 0;
        ctx->openContextMenuId = 0;
        ctx->focusedId = restoreFocus;
        ctx->menuNavigationId = 0;
    }
    if (ctx->focusedId == id.id) {
        Clay_ElementId owner = {0}; owner.id = ctx->menuNavigationId;
        Clay_ElementId panel = ctx->openContextMenuId
            ? Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsContextMenuPanel"),owner.id) : ClayWidgets__MenuDropdownId(owner);
        ClayWidgets__ScrollIntoView(ctx,id,panel);
    }

    CLAY(id, {
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0, 0) },
            .padding = {
                .left = ctx->theme.spacing.sm,
                .right = ctx->theme.spacing.lg,
                .top = ctx->theme.spacing.sm,
                .bottom = ctx->theme.spacing.sm,
            },
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER },
        },
        .backgroundColor = over ? ctx->theme.hoverColor : ClayWidgets__FadeToClear(ctx->theme.hoverColor),
        .cornerRadius = CLAY_CORNER_RADIUS((float)ctx->theme.radiusSm),
        .transition = ClayWidgets__ColorTransition(ctx),
    }) {
        CLAY_TEXT(label, {
            .textColor = ctx->theme.textColor,
            .fontId = ctx->theme.fontBody,
            .fontSize = ctx->theme.fontSizeBody,
            .wrapMode = CLAY_TEXT_WRAP_NONE,
        });
    }

    ClayWidgets__Describe(ctx,id,CLAY_WIDGETS_ROLE_MENU_ITEM,label,false,false);
    return clicked;
}

void ClayWidgets_MenuSeparator(ClayWidgets_Context *ctx) {
    if (!ctx) {
        return;
    }
    CLAY_AUTO_ID({
        .layout = {
            .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(1) },
            .padding = { .left = ctx->theme.spacing.xs, .right = ctx->theme.spacing.xs },
        },
    }) {
        CLAY_AUTO_ID({
            .layout = { .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(1) } },
            .backgroundColor = ctx->theme.borderColor,
        }) {}
    }
}

bool ClayWidgets_RightClicked(ClayWidgets_Context *ctx, Clay_ElementId id) {
    return ctx && ctx->input.pointerRightPressed && Clay_PointerOver(id);
}

void ClayWidgets_OpenContextMenu(ClayWidgets_Context *ctx, Clay_ElementId menuId, float x, float y) {
    if (!ctx) {
        return;
    }
    // Nudge the anchor in from the right/bottom edges so a menu opened near the
    // window border still fits. Height is unknown here, so we reserve a rough
    // band; width uses the same minimum the panel lays out at.
    const float estWidth = 190.0f;
    const float estHeight = 180.0f;
    float maxX = ctx->layoutDimensions.width - estWidth;
    float maxY = ctx->layoutDimensions.height - estHeight;
    if (maxX < 0.0f) maxX = 0.0f;
    if (maxY < 0.0f) maxY = 0.0f;
    ctx->contextMenuX = ClayWidgets__ClampF32(x, 0.0f, maxX);
    ctx->contextMenuY = ClayWidgets__ClampF32(y, 0.0f, maxY);
    ctx->openContextMenuId = menuId.id;
    ctx->contextMenuReturnFocus = ctx->focusedId;
}

bool ClayWidgets_BeginContextMenu(ClayWidgets_Context *ctx, Clay_ElementId menuId) {
    if (!ctx || ctx->openContextMenuId != menuId.id) {
        return false;
    }

    Clay_ElementId panelId = Clay_GetElementIdWithIndex(CLAY_STRING("ClayWidgetsContextMenuPanel"), menuId.id);

    // Dismiss on Escape or a left press that lands outside the panel. Item
    // selection closes via MenuItem; right-clicks are the caller's business
    // (they reopen the menu elsewhere).
    if (ctx->input.keyEscape) {
        ctx->openContextMenuId = 0;
        ctx->focusedId = ctx->contextMenuReturnFocus;
        ctx->menuNavigationId = 0;
        ctx->input.keyEscape = false;
        return false;
    }
    if (ctx->input.pointerPressed && !Clay_PointerOver(panelId)) {
        ctx->openContextMenuId = 0;
        return false;
    }

    float r = (float)ctx->theme.radiusSm;
    if (!ClayWidgets__PushOverlay(ctx, 400)) return false;
    ClayWidgets__MenuNavigation(ctx, menuId.id);
    Clay_ElementData previous = Clay_GetElementData(panelId);
    if (previous.found) {
        ctx->contextMenuX = ClayWidgets__Clamp(ctx->contextMenuX,0,fmaxf(0,ctx->layoutDimensions.width-previous.boundingBox.width));
        ctx->contextMenuY = ClayWidgets__Clamp(ctx->contextMenuY,0,fmaxf(0,ctx->layoutDimensions.height-previous.boundingBox.height));
    }
    ClayWidgets__BeginScrollElement(panelId, CLAY__INIT(Clay_ElementDeclaration){
        .layout = {
            .sizing = { .width = CLAY_SIZING_FIT(180, ctx->layoutDimensions.width), .height = CLAY_SIZING_FIT(0, fmaxf(1,ctx->layoutDimensions.height-ctx->contextMenuY)) },
            .padding = CLAY_PADDING_ALL(ctx->theme.spacing.xs),
            .childGap = 0,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = ctx->theme.surfaceAltColor,
        .cornerRadius = (Clay_CornerRadius){ r, r, r, r },
        .floating = {
            .offset = { .x = ctx->contextMenuX, .y = ctx->contextMenuY },
            .zIndex = ClayWidgets__OverlayZ(ctx, 0),
            .attachPoints = {
                .element = CLAY_ATTACH_POINT_LEFT_TOP,
                .parent = CLAY_ATTACH_POINT_LEFT_TOP,
            },
            .attachTo = CLAY_ATTACH_TO_ROOT,
        },
        .clip = { .vertical = true },
        .border = {
            .color = ctx->theme.borderColor,
            .width = { .left = 1, .right = 1, .top = 1, .bottom = 1 },
        },
    });

    return true;
}

void ClayWidgets_EndContextMenu(ClayWidgets_Context *ctx, Clay_ElementId menuId) {
    (void)menuId;
    if (!ctx) {
        return;
    }
    ClayWidgets__EndElement(); // context menu panel
    ClayWidgets__PopOverlay(ctx);
}

#endif

#endif
