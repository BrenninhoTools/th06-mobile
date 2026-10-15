#!/bin/bash
APK=android/app/build/outputs/apk/release/app-release.apk
PKG=com.brenninhotools.th06
FILTER='AndroidRuntime|SDL|libc|DEBUG|crash_dump|th06|ActivityManager|ActivityTaskManager|linker|System.err|nativeloader|Zygote|RuntimeInit'

adb install -r -g "$APK"
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow

adb logcat -c
adb shell am start -W -n $PKG/.SetupActivity
sleep 12
adb shell pidof $PKG > setup-pid.txt || true
adb logcat -d -v threadtime | grep -E "$FILTER" | grep -vE "ActivityManager: (Start proc|Killing).*(com\.google|android\.)" | tail -n 150 > logcat-setup.txt
echo "pid after setup: $(cat setup-pid.txt)" >> logcat-setup.txt

adb shell am force-stop $PKG
DATA=/sdcard/Android/data/$PKG/files
adb shell mkdir -p $DATA
head -c 65536 /dev/urandom > fake.dat
adb push fake.dat "$DATA/th06_CM.DAT"

adb logcat -c
adb shell am start -W -n $PKG/.TH06Activity
sleep 15
adb logcat -d -v threadtime | grep -E "$FILTER" | tail -n 250 > logcat-game.txt
echo "pid after game: $(adb shell pidof $PKG)" >> logcat-game.txt
adb shell ls -la $DATA >> logcat-game.txt
adb shell ls -la /sdcard/.Touhou6/crash >> logcat-game.txt 2>&1
adb exec-out screencap -p > screen.png || true
true
