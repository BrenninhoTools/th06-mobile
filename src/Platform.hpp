#pragma once

#include <SDL2/SDL.h>
#include <string>

#if defined(__ANDROID__) || (defined(__APPLE__) && TARGET_OS_IPHONE)
#define TH06_MOBILE 1
#else
#define TH06_MOBILE 0
#endif

#if TH06_MOBILE || defined(TH06_FORCE_MOBILE_UI)
#define TH06_TOUCH_UI 1
#else
#define TH06_TOUCH_UI 0
#endif

namespace Platform
{
bool Init();
void ApplyHints();
const std::string &DataDir();
std::string DataPath(const char *name);
bool HasGameData();
void ShowMissingDataMessage();
void HandleEvent(const SDL_Event &e);
void AttachLifecycle();
bool IsActive();
} // namespace Platform
