#include "Platform.hpp"
#include "FileSystem.hpp"
#include "GameWindow.hpp"
#include "SoundPlayer.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

static std::string g_DataDir;
static bool g_Active = true;

static const char *c_ReadmeName = "PUT_GAME_FILES_HERE.txt";
static const char *c_ReadmeText =
    "Copy every file from your Touhou 6 (Embodiment of Scarlet Devil 1.02h) folder into this folder.\n"
    "Required: " TH_CM_DAT_FILE ", " TH_ED_DAT_FILE ", " TH_IN_DAT_FILE ", " TH_MD_DAT_FILE ", " TH_ST_DAT_FILE
    ", " TH_TL_DAT_FILE "\n"
    "Optional: bgm/*.wav for music, msgothic.ttc or NotoSansJP-Regular.ttf for text.\n";

static int SDLCALL LifecycleWatch(void *userdata, SDL_Event *e)
{
    (void)userdata;
    Platform::HandleEvent(*e);
    return 0;
}

static void ComputeDataDir()
{
#if defined(__ANDROID__)
    const char *dir = SDL_AndroidGetExternalStoragePath();
    if (dir == NULL)
    {
        dir = SDL_AndroidGetInternalStoragePath();
    }
    g_DataDir = dir != NULL ? dir : "";
#elif TH06_MOBILE
    const char *home = std::getenv("HOME");
    g_DataDir = home != NULL ? std::string(home) + "/Documents" : "";
#else
    g_DataDir.clear();
#endif
}

void Platform::ApplyHints()
{
#if TH06_MOBILE
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "1");
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE_PAUSEAUDIO, "1");
    SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
    SDL_SetHint(SDL_HINT_IDLE_TIMER_DISABLED, "1");
    SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "playback");
#elif TH06_TOUCH_UI
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
#endif
}

bool Platform::Init()
{
    ComputeDataDir();

    if (g_DataDir.empty())
    {
        return true;
    }

#ifndef _WIN32
    mkdir(g_DataDir.c_str(), 0755);
    if (chdir(g_DataDir.c_str()) != 0)
    {
        return false;
    }
    mkdir("bgm", 0755);
    mkdir("replay", 0755);
#endif

    FILE *readme = FileSystem::FopenUTF8(c_ReadmeName, "rb");
    if (readme != NULL)
    {
        std::fclose(readme);
    }
    else
    {
        FileSystem::WriteDataToFile(c_ReadmeName, c_ReadmeText, std::strlen(c_ReadmeText));
    }
    return true;
}

const std::string &Platform::DataDir()
{
    return g_DataDir;
}

std::string Platform::DataPath(const char *name)
{
    if (g_DataDir.empty() || name == NULL || name[0] == '/')
    {
        return name != NULL ? name : "";
    }
    std::string path = g_DataDir;
    path += '/';
    for (const char *c = name; *c != '\0'; c++)
    {
        path += *c == '\\' ? '/' : *c;
    }
    return path;
}

bool Platform::HasGameData()
{
    FILE *f = FileSystem::FopenUTF8(TH_CM_DAT_FILE, "rb");
    if (f == NULL)
    {
        return false;
    }
    std::fclose(f);
    return true;
}

void Platform::ShowMissingDataMessage()
{
    std::string msg = "Game data not found.\n\nCopy the Touhou 6 (EoSD 1.02h) files into:\n";
    msg += g_DataDir.empty() ? "the game folder" : g_DataDir;
    msg += "\n\nSee ";
    msg += c_ReadmeName;
    msg += " in that folder.";
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "th06", msg.c_str(), NULL);
}

void Platform::HandleEvent(const SDL_Event &e)
{
    switch (e.type)
    {
    case SDL_APP_WILLENTERBACKGROUND:
    case SDL_APP_DIDENTERBACKGROUND:
        if (g_Active)
        {
            g_Active = false;
            g_GameWindow.lastActiveAppValue = 0;
            if (g_SoundPlayer.audioDev != 0)
            {
                SDL_PauseAudioDevice(g_SoundPlayer.audioDev, 1);
            }
        }
        break;
    case SDL_APP_WILLENTERFOREGROUND:
    case SDL_APP_DIDENTERFOREGROUND:
        if (!g_Active)
        {
            g_Active = true;
            g_GameWindow.lastActiveAppValue = 1;
            g_LastFrameTime = SDL_GetTicks();
            g_Supervisor.lastFrameTime = SDL_GetTicks();
            if (g_SoundPlayer.audioDev != 0)
            {
                SDL_PauseAudioDevice(g_SoundPlayer.audioDev, 0);
            }
        }
        break;
    default:
        break;
    }
}

void Platform::AttachLifecycle()
{
    SDL_DelEventWatch(LifecycleWatch, NULL);
    SDL_AddEventWatch(LifecycleWatch, NULL);
}

bool Platform::IsActive()
{
    return g_Active;
}
