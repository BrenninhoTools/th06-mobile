#include "TouchControls.hpp"
#include "Controller.hpp"

#include <SDL2/SDL_ttf.h>
#include <cmath>

namespace
{
enum SlotKind
{
    SLOT_FREE,
    SLOT_STICK,
    SLOT_BUTTON,
};

struct Slot
{
    SDL_FingerID id;
    SlotKind kind;
    i32 button;
};

struct ButtonDef
{
    TouhouButton button;
    f32 offRight;
    f32 offY;
    bool fromTop;
    f32 radius;
    f32 r;
    f32 g;
    f32 b;
    const char *label;
};

const ButtonDef c_Buttons[TouchControls::LABEL_COUNT] = {
    {TH_BUTTON_SHOOT, 0.15f, 0.24f, false, 0.105f, 0.95f, 0.35f, 0.35f, "SHOT"},
    {TH_BUTTON_BOMB, 0.34f, 0.13f, false, 0.080f, 0.35f, 0.60f, 0.95f, "BOMB"},
    {TH_BUTTON_FOCUS, 0.15f, 0.49f, false, 0.080f, 0.95f, 0.85f, 0.35f, "SLOW"},
    {TH_BUTTON_MENU, 0.08f, 0.09f, true, 0.060f, 0.80f, 0.80f, 0.80f, "MENU"},
    {TH_BUTTON_SKIP, 0.24f, 0.09f, true, 0.060f, 0.80f, 0.80f, 0.80f, "SKIP"},
};

const i32 c_MaxSlots = 10;
const f32 c_StickRadius = 0.13f;
const f32 c_StickDeadZone = 0.025f;
const f32 c_HitScale = 1.3f;

Slot g_Slots[c_MaxSlots];
i32 g_Width = 1;
i32 g_Height = 1;
bool g_Visible = true;
bool g_StickActive = false;
f32 g_StickCenterX;
f32 g_StickCenterY;
f32 g_StickX;
f32 g_StickY;
bool g_ButtonHeld[TouchControls::LABEL_COUNT];

f32 Unit()
{
    return (f32)(g_Height < g_Width ? g_Height : g_Width);
}

void ButtonCenter(i32 index, f32 *x, f32 *y)
{
    const ButtonDef &def = c_Buttons[index];
    f32 unit = Unit();
    *x = g_Width - def.offRight * unit;
    *y = def.fromTop ? def.offY * unit : g_Height - def.offY * unit;
}

void DefaultStickCenter(f32 *x, f32 *y)
{
    f32 unit = Unit();
    *x = 0.21f * unit;
    *y = g_Height - 0.24f * unit;
}

i32 HitButton(f32 x, f32 y)
{
    f32 unit = Unit();
    i32 best = -1;
    f32 bestDist = 0.0f;

    for (i32 i = 0; i < TouchControls::LABEL_COUNT; i++)
    {
        f32 cx;
        f32 cy;
        ButtonCenter(i, &cx, &cy);
        f32 dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
        f32 limit = c_Buttons[i].radius * unit * c_HitScale;
        if (dist <= limit && (best < 0 || dist < bestDist))
        {
            best = i;
            bestDist = dist;
        }
    }
    return best;
}

Slot *FindSlot(SDL_FingerID id)
{
    for (i32 i = 0; i < c_MaxSlots; i++)
    {
        if (g_Slots[i].kind != SLOT_FREE && g_Slots[i].id == id)
        {
            return &g_Slots[i];
        }
    }
    return NULL;
}

Slot *AllocSlot(SDL_FingerID id)
{
    for (i32 i = 0; i < c_MaxSlots; i++)
    {
        if (g_Slots[i].kind == SLOT_FREE)
        {
            g_Slots[i].id = id;
            return &g_Slots[i];
        }
    }
    return NULL;
}

void RecountButtons()
{
    for (i32 i = 0; i < TouchControls::LABEL_COUNT; i++)
    {
        g_ButtonHeld[i] = false;
    }
    g_StickActive = false;
    for (i32 i = 0; i < c_MaxSlots; i++)
    {
        if (g_Slots[i].kind == SLOT_BUTTON && g_Slots[i].button >= 0)
        {
            g_ButtonHeld[g_Slots[i].button] = true;
        }
        else if (g_Slots[i].kind == SLOT_STICK)
        {
            g_StickActive = true;
        }
    }
}

void ResetAll()
{
    for (i32 i = 0; i < c_MaxSlots; i++)
    {
        g_Slots[i].kind = SLOT_FREE;
    }
    RecountButtons();
}

void FingerDown(SDL_FingerID id, f32 x, f32 y)
{
    g_Visible = true;
    Slot *slot = FindSlot(id);
    if (slot == NULL)
    {
        slot = AllocSlot(id);
    }
    if (slot == NULL)
    {
        return;
    }

    i32 button = HitButton(x, y);
    if (button >= 0)
    {
        slot->kind = SLOT_BUTTON;
        slot->button = button;
    }
    else if (x < g_Width * 0.5f && !g_StickActive)
    {
        slot->kind = SLOT_STICK;
        slot->button = -1;
        g_StickCenterX = x;
        g_StickCenterY = y;
        g_StickX = x;
        g_StickY = y;
    }
    else
    {
        slot->kind = SLOT_BUTTON;
        slot->button = -1;
    }
    RecountButtons();
}

void FingerMotion(SDL_FingerID id, f32 x, f32 y)
{
    Slot *slot = FindSlot(id);
    if (slot == NULL)
    {
        return;
    }
    if (slot->kind == SLOT_STICK)
    {
        f32 maxDist = c_StickRadius * Unit();
        f32 dx = x - g_StickCenterX;
        f32 dy = y - g_StickCenterY;
        f32 dist = std::sqrt(dx * dx + dy * dy);
        if (dist > maxDist * 1.6f)
        {
            f32 pull = dist - maxDist * 1.6f;
            g_StickCenterX += dx / dist * pull;
            g_StickCenterY += dy / dist * pull;
        }
        g_StickX = x;
        g_StickY = y;
    }
    else
    {
        slot->button = HitButton(x, y);
        RecountButtons();
    }
}

void FingerUp(SDL_FingerID id)
{
    Slot *slot = FindSlot(id);
    if (slot == NULL)
    {
        return;
    }
    slot->kind = SLOT_FREE;
    RecountButtons();
}

u16 StickButtons()
{
    if (!g_StickActive)
    {
        return 0;
    }
    f32 unit = Unit();
    f32 dx = g_StickX - g_StickCenterX;
    f32 dy = g_StickY - g_StickCenterY;
    f32 dist = std::sqrt(dx * dx + dy * dy);
    if (dist < c_StickDeadZone * unit)
    {
        return 0;
    }
    u16 buttons = 0;
    f32 nx = dx / dist;
    f32 ny = dy / dist;
    const f32 diagonal = 0.38f;
    if (nx > diagonal)
    {
        buttons |= TH_BUTTON_RIGHT;
    }
    else if (nx < -diagonal)
    {
        buttons |= TH_BUTTON_LEFT;
    }
    if (ny > diagonal)
    {
        buttons |= TH_BUTTON_DOWN;
    }
    else if (ny < -diagonal)
    {
        buttons |= TH_BUTTON_UP;
    }
    return buttons;
}
} // namespace

void TouchControls::HandleEvent(const SDL_Event &e)
{
    switch (e.type)
    {
    case SDL_FINGERDOWN:
        FingerDown(e.tfinger.fingerId, e.tfinger.x * g_Width, e.tfinger.y * g_Height);
        break;
    case SDL_FINGERMOTION:
        FingerMotion(e.tfinger.fingerId, e.tfinger.x * g_Width, e.tfinger.y * g_Height);
        break;
    case SDL_FINGERUP:
        FingerUp(e.tfinger.fingerId);
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        g_Visible = false;
        ResetAll();
        break;
    case SDL_APP_WILLENTERBACKGROUND:
    case SDL_WINDOWEVENT:
        if (e.type == SDL_APP_WILLENTERBACKGROUND || e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
        {
            ResetAll();
        }
        break;
    default:
        break;
    }
}

void TouchControls::SetScreenSize(i32 width, i32 height)
{
    g_Width = width > 0 ? width : 1;
    g_Height = height > 0 ? height : 1;
}

u16 TouchControls::GetButtons()
{
    u16 buttons = StickButtons();
    for (i32 i = 0; i < LABEL_COUNT; i++)
    {
        if (g_ButtonHeld[i])
        {
            buttons |= c_Buttons[i].button;
        }
    }
    return buttons;
}

bool TouchControls::IsVisible()
{
    return g_Visible;
}

i32 TouchControls::BuildOverlay(OverlayItem *items, i32 maxItems)
{
    i32 count = 0;
    f32 unit = Unit();

    if (!g_Visible)
    {
        return 0;
    }

    f32 baseX;
    f32 baseY;
    f32 knobX;
    f32 knobY;
    if (g_StickActive)
    {
        baseX = g_StickCenterX;
        baseY = g_StickCenterY;
        f32 dx = g_StickX - baseX;
        f32 dy = g_StickY - baseY;
        f32 dist = std::sqrt(dx * dx + dy * dy);
        f32 maxDist = c_StickRadius * unit;
        if (dist > maxDist)
        {
            dx = dx / dist * maxDist;
            dy = dy / dist * maxDist;
        }
        knobX = baseX + dx;
        knobY = baseY + dy;
    }
    else
    {
        DefaultStickCenter(&baseX, &baseY);
        knobX = baseX;
        knobY = baseY;
    }

    if (count + 2 <= maxItems)
    {
        items[count++] = {baseX, baseY, c_StickRadius * unit, 1.0f, 1.0f, 1.0f, g_StickActive ? 0.35f : 0.20f,
                          OVERLAY_RING, -1};
        items[count++] = {knobX, knobY, 0.055f * unit, 1.0f, 1.0f, 1.0f, g_StickActive ? 0.55f : 0.30f,
                          OVERLAY_CIRCLE, -1};
    }

    for (i32 i = 0; i < LABEL_COUNT && count < maxItems; i++)
    {
        f32 cx;
        f32 cy;
        ButtonCenter(i, &cx, &cy);
        const ButtonDef &def = c_Buttons[i];
        items[count++] = {cx, cy, def.radius * unit, def.r, def.g, def.b, g_ButtonHeld[i] ? 0.65f : 0.32f,
                          OVERLAY_CIRCLE, i};
    }

    return count;
}

SDL_Surface *TouchControls::CreateLabelSurface(i32 label)
{
    static TTF_Font *font = NULL;
    static bool tried = false;

    if (label < 0 || label >= LABEL_COUNT)
    {
        return NULL;
    }

    if (!tried)
    {
        tried = true;
        if (TTF_WasInit() == 0)
        {
            TTF_Init();
        }
        font = TTF_OpenFont("NotoSans-Regular.ttf", 48);
    }

    if (font == NULL)
    {
        return NULL;
    }

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *text = TTF_RenderUTF8_Blended(font, c_Buttons[label].label, white);
    if (text == NULL)
    {
        return NULL;
    }
    SDL_Surface *converted = SDL_ConvertSurfaceFormat(text, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(text);
    return converted;
}
