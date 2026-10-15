# th06-mobile

Android and iOS port of Touhou Koumakyou ~ the Embodiment of Scarlet Devil (1.02h), built on the
[GensokyoClub/th06](https://github.com/GensokyoClub/th06) decompilation and its SDL2 + OpenGL ES `portable` branch.

This repository does **not** contain any game assets. You need your own copy of the game.

## Features

- SDL2 + OpenGL ES 2.0 renderer, rendered at 640x480 into an offscreen framebuffer and scaled to the screen with letterboxing
- On-screen touch controls: floating joystick on the left, SHOT / BOMB / SLOW on the right, MENU / SKIP at the top
- Bluetooth / USB game controller support with hot-plugging (touch controls hide while a controller is used)
- Android back button opens the pause menu
- App pauses rendering and audio when sent to the background

## Installing the game files

Copy every file from your EoSD 1.02h folder (`紅魔郷CM.DAT`, `紅魔郷ED.DAT`, `紅魔郷IN.DAT`, `紅魔郷MD.DAT`,
`紅魔郷ST.DAT`, `紅魔郷TL.DAT`, and the `bgm` folder if you have it) into the game data folder:

| Platform | Data folder |
| --- | --- |
| Android | `Android/data/com.brenninhotools.th06/files/` |
| iOS | the app's folder in the **Files** app (On My iPhone → th06) |

Launch the app once first so the folder is created. A `PUT_GAME_FILES_HERE.txt` file is written there.

Notes:

- Music uses the WAV soundtrack (`bgm/th06_*.wav`). MIDI playback is not available on mobile.
- Text uses `msgothic.ttc` or `NotoSansJP-Regular.ttf` from the data folder if present, otherwise a bundled
  or system Japanese font.
- Japanese file names are required. English-patched file names are not supported.

## Building

All mobile builds use CMake and download SDL2, SDL2_image and SDL2_ttf automatically.

### Android

Requirements: JDK 17, Android SDK with NDK, Gradle 8.9+.

```sh
cd android
gradle assembleRelease -Pth06Abis=arm64-v8a
```

The APK is written to `android/app/build/outputs/apk/release/`. Optional Gradle properties:
`th06Abis` (comma separated ABIs), `th06NdkVersion`, `th06CmakeVersion`.

### iOS

Requirements: macOS with Xcode and Ninja.

```sh
cmake -S . -B build-ios -G Ninja -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios --target th06
```

The resulting `th06.app` is unsigned. Put it in a `Payload` folder and zip it as `.ipa` to sideload it
(AltStore, Sideloadly, TrollStore).

### Desktop

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Uses the system SDL2, SDL2_image and SDL2_ttf (or pass `-DTH06_VENDORED_SDL=ON`).
`-DTH06_FORCE_MOBILE_UI=ON` enables the mobile renderer and touch controls on desktop for testing
(mouse clicks act as touches). The original `premake5.lua` from the portable branch is kept as well.

## CI

`.github/workflows/mobile.yml` builds an arm64 Android APK and an unsigned arm64 iOS IPA on every push to `master`.
Both jobs run in parallel and use ccache.

## Credits

- [GensokyoClub/th06](https://github.com/GensokyoClub/th06) for the decompilation and the portable SDL2 port
- [SDL](https://libsdl.org) for the platform layer
