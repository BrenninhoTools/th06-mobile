#!/bin/bash
APK=android/app/build/outputs/apk/release/app-release.apk
PKG=com.brenninhotools.th06
FILTER=' (AndroidRuntime|SDL|SDL/APP|SDL/ERROR|libc|DEBUG|crash_dump64|th06|System.err|linker) *:'

adb install -r -g "$APK"
adb shell appops set $PKG MANAGE_EXTERNAL_STORAGE allow

adb logcat -c
adb logcat -b crash -c
adb shell am start -W -n $PKG/.SetupActivity
sleep 12
adb logcat -d -b crash -v threadtime > logcat-crash-setup.txt
adb logcat -d -v threadtime | grep -E "$FILTER" | grep -v "Access denied finding property" | tail -n 120 > logcat-setup.txt
echo "pid after setup: $(adb shell pidof $PKG)" >> logcat-setup.txt

adb shell am force-stop $PKG
DATA=/sdcard/.Touhou6
adb shell mkdir -p $DATA
head -c 65536 /dev/urandom > fake.dat
adb push fake.dat "$DATA/th06_CM.DAT"

adb logcat -c
adb logcat -b crash -c
adb shell am start -W -n $PKG/.SetupActivity
sleep 15
adb logcat -d -b crash -v threadtime > logcat-crash-game.txt
adb logcat -d -v threadtime | grep -E "$FILTER" | grep -v "Access denied finding property" | tail -n 150 > logcat-game.txt
echo "pid after game: $(adb shell pidof $PKG)" >> logcat-game.txt
adb shell ls -la $DATA >> logcat-game.txt
adb shell ls -la /sdcard/.Touhou6/crash >> logcat-game.txt 2>&1
adb shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity" >> logcat-game.txt
true
