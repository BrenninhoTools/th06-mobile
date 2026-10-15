package com.brenninhotools.th06;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.os.IBinder;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class CrashWatcherService extends Service {
    public static final String EXTRA_PID = "pid";

    private static final int HISTORY_LINES = 400;
    private static final long CAPTURE_MILLIS = 3000;
    private static final String[] CRASH_MARKERS = {
        "FATAL EXCEPTION",
        "Fatal signal",
        "*** *** *** *** *** ***",
        "Abort message:",
        "java.lang.UnsatisfiedLinkError",
        "Process crashed",
    };

    private final Object lock = new Object();
    private final ArrayDeque<String> history = new ArrayDeque<>();
    private final List<String> capture = new ArrayList<>();
    private Thread readerThread;
    private Thread monitorThread;
    private Process logcat;
    private volatile int targetPid;
    private volatile boolean running;
    private long captureDeadline;
    private String crashReason;
    private boolean crashWritten;

    public static void start(Context context, int pid) {
        Intent intent = new Intent(context, CrashWatcherService.class);
        intent.putExtra(EXTRA_PID, pid);
        try {
            context.startService(intent);
        } catch (Exception ignored) {
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        int pid = intent != null ? intent.getIntExtra(EXTRA_PID, 0) : 0;
        if (pid == 0) {
            stopSelf();
            return START_NOT_STICKY;
        }
        if (running && pid == targetPid) {
            return START_NOT_STICKY;
        }
        stopWatching();
        targetPid = pid;
        crashWritten = false;
        crashReason = null;
        startWatching();
        return START_NOT_STICKY;
    }

    @Override
    public void onDestroy() {
        stopWatching();
        super.onDestroy();
    }

    private void startWatching() {
        running = true;
        readerThread = new Thread(this::readLogcat, "th06-logcat");
        readerThread.start();
        monitorThread = new Thread(this::monitor, "th06-crash-monitor");
        monitorThread.start();
    }

    private void stopWatching() {
        running = false;
        if (logcat != null) {
            logcat.destroy();
            logcat = null;
        }
        if (readerThread != null) {
            readerThread.interrupt();
            readerThread = null;
        }
        if (monitorThread != null) {
            monitorThread.interrupt();
            monitorThread = null;
        }
    }

    private void readLogcat() {
        try {
            logcat = Runtime.getRuntime().exec(new String[] {"logcat", "-v", "threadtime", "-T", "1"});
            BufferedReader reader = new BufferedReader(
                    new InputStreamReader(logcat.getInputStream(), StandardCharsets.UTF_8));
            String line;
            while (running && (line = reader.readLine()) != null) {
                onLine(line);
            }
        } catch (Exception ignored) {
        }
    }

    private void onLine(String line) {
        synchronized (lock) {
            if (crashReason != null && !crashWritten) {
                capture.add(line);
                return;
            }
            history.addLast(line);
            while (history.size() > HISTORY_LINES) {
                history.removeFirst();
            }
            if (crashWritten) {
                return;
            }
            for (String marker : CRASH_MARKERS) {
                if (line.contains(marker) && belongsToTarget(line, marker)) {
                    crashReason = marker;
                    captureDeadline = System.currentTimeMillis() + CAPTURE_MILLIS;
                    return;
                }
            }
        }
    }

    private boolean belongsToTarget(String line, String marker) {
        if (marker.startsWith("***") || marker.equals("Abort message:")) {
            return true;
        }
        String pid = " " + targetPid + " ";
        return line.contains(pid) || line.contains("pid " + targetPid) || line.contains("PID: " + targetPid);
    }

    private void monitor() {
        long deadSince = 0;
        while (running) {
            try {
                Thread.sleep(250);
            } catch (InterruptedException e) {
                return;
            }

            boolean shouldWrite;
            synchronized (lock) {
                shouldWrite = crashReason != null && !crashWritten
                        && System.currentTimeMillis() >= captureDeadline;
            }
            if (shouldWrite) {
                writeCrash();
                stopSelf();
                return;
            }

            boolean alive = new File("/proc/" + targetPid).exists();
            if (!alive) {
                if (deadSince == 0) {
                    deadSince = System.currentTimeMillis();
                } else if (System.currentTimeMillis() - deadSince > CAPTURE_MILLIS + 2000) {
                    synchronized (lock) {
                        if (crashReason != null && !crashWritten) {
                            shouldWrite = true;
                        }
                    }
                    if (shouldWrite) {
                        writeCrash();
                    }
                    stopSelf();
                    return;
                }
            } else {
                deadSince = 0;
            }
        }
    }

    private void writeCrash() {
        List<String> before;
        List<String> after;
        String reason;
        synchronized (lock) {
            if (crashWritten) {
                return;
            }
            crashWritten = true;
            before = new ArrayList<>(history);
            after = new ArrayList<>(capture);
            reason = crashReason;
        }

        String stamp = new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.ROOT).format(new Date());
        File file = new File(TouhouStorage.crashDir(this), "crash_" + stamp + ".txt");

        try (Writer writer = new OutputStreamWriter(new FileOutputStream(file), StandardCharsets.UTF_8)) {
            writer.write("th06-mobile crash report\n");
            writer.write("Time: " + stamp + "\n");
            writer.write("Reason: " + reason + "\n");
            writer.write("PID: " + targetPid + "\n");
            writer.write("App: " + appVersion() + "\n");
            writer.write("Device: " + Build.MANUFACTURER + " " + Build.MODEL + "\n");
            writer.write("Android: " + Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")\n");
            writer.write("ABIs: " + String.join(", ", Build.SUPPORTED_ABIS) + "\n");
            writer.write("\n===== logcat =====\n");
            for (String line : before) {
                writer.write(line);
                writer.write('\n');
            }
            for (String line : after) {
                writer.write(line);
                writer.write('\n');
            }
        } catch (Exception ignored) {
        }
    }

    private String appVersion() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            return info.packageName + " " + info.versionName;
        } catch (Exception e) {
            return getPackageName();
        }
    }
}
