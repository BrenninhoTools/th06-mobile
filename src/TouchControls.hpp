#pragma once

#include "inttypes.hpp"

#include <SDL2/SDL.h>

namespace TouchControls
{
enum OverlayShape
{
    OVERLAY_CIRCLE,
    OVERLAY_RING,
};

struct OverlayItem
{
    f32 x;
    f32 y;
    f32 radius;
    f32 r;
    f32 g;
    f32 b;
    f32 a;
    OverlayShape shape;
    i32 label;
};

enum
{
    MAX_OVERLAY_ITEMS = 16,
    LABEL_COUNT = 5,
};

void HandleEvent(const SDL_Event &e);
void SetScreenSize(i32 width, i32 height);
u16 GetButtons();
bool IsVisible();
i32 BuildOverlay(OverlayItem *items, i32 maxItems);
SDL_Surface *CreateLabelSurface(i32 label);
} // namespace TouchControls
