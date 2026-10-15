package com.brenninhotools.th06;

import android.content.Context;
import android.os.Build;
import android.os.Environment;

import java.io.File;

public final class TouhouStorage {
    public static final String ROOT_NAME = ".Touhou6";
    public static final String CRASH_NAME = "crash";

    private TouhouStorage() {
    }

    public static boolean hasAllFilesAccess(Context context) {
        if (Build.VERSION.SDK_INT >= 30) {
            return Environment.isExternalStorageManager();
        }
        if (Build.VERSION.SDK_INT >= 23) {
            return context.checkSelfPermission(android.Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    == android.content.pm.PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    public static File externalRoot() {
        return new File(Environment.getExternalStorageDirectory(), ROOT_NAME);
    }

    public static File rootDir(Context context) {
        File root = externalRoot();
        if (hasAllFilesAccess(context) && (root.isDirectory() || root.mkdirs()) && root.canWrite()) {
            return root;
        }
        File fallback = context.getExternalFilesDir(null);
        if (fallback == null) {
            fallback = context.getFilesDir();
        }
        File dir = new File(fallback, ROOT_NAME);
        dir.mkdirs();
        return dir;
    }

    public static File crashDir(Context context) {
        File dir = new File(rootDir(context), CRASH_NAME);
        dir.mkdirs();
        return dir;
    }
}
