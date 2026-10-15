package com.brenninhotools.th06;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.provider.DocumentsContract;
import android.provider.Settings;
import android.system.Os;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.Charset;
import java.util.ArrayDeque;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class SetupActivity extends Activity {
    private static final int REQUEST_FOLDER = 6;
    private static final int REQUEST_ALL_FILES = 7;
    private static final int REQUEST_STORAGE = 8;
    private static final int REQUEST_ZIP = 9;
    private static final String PREF_ASKED_STORAGE = "askedStorage";
    private static final String BUNDLED_ASSET_DIR = "gamedata";
    private static final int MAX_SEARCH_DEPTH = 3;

    private TextView status;
    private Button folderButton;
    private Button zipButton;
    private volatile boolean copying;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        CrashWatcherService.start(this, Process.myPid());
        if (!requestStorageAccess()) {
            continueStartup();
        }
    }

    private boolean requestStorageAccess() {
        if (TouhouStorage.hasAllFilesAccess(this)) {
            return false;
        }
        SharedPreferences prefs = getSharedPreferences("th06", MODE_PRIVATE);
        if (prefs.getBoolean(PREF_ASKED_STORAGE, false)) {
            return false;
        }
        prefs.edit().putBoolean(PREF_ASKED_STORAGE, true).apply();

        if (Build.VERSION.SDK_INT >= 30) {
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, REQUEST_ALL_FILES);
            } catch (Exception e) {
                try {
                    startActivityForResult(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION),
                            REQUEST_ALL_FILES);
                } catch (Exception ignored) {
                    return false;
                }
            }
            return true;
        }
        if (Build.VERSION.SDK_INT >= 23) {
            requestPermissions(new String[] {
                android.Manifest.permission.READ_EXTERNAL_STORAGE,
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE
            }, REQUEST_STORAGE);
            return true;
        }
        return false;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_STORAGE) {
            continueStartup();
        }
    }

    private void continueStartup() {
        TouhouStorage.crashDir(this);
        File gameDir = findGameDir();
        if (gameDir != null) {
            launchGame(gameDir);
            return;
        }
        buildUi();
        if (hasBundledData()) {
            importBundled();
        }
    }

    private File appDataDir() {
        File dir = getExternalFilesDir(null);
        if (dir == null) {
            dir = getFilesDir();
        }
        dir.mkdirs();
        return dir;
    }

    private File importTarget() {
        if (TouhouStorage.hasAllFilesAccess(this)) {
            File root = TouhouStorage.externalRoot();
            if ((root.isDirectory() || root.mkdirs()) && root.canWrite()) {
                return root;
            }
        }
        return appDataDir();
    }

    private File findGameDir() {
        if (TouhouStorage.hasAllFilesAccess(this) && hasGameData(TouhouStorage.externalRoot())) {
            return TouhouStorage.externalRoot();
        }
        if (hasGameData(appDataDir())) {
            return appDataDir();
        }
        return null;
    }

    private static boolean isCmDat(String name) {
        return name != null && name.toUpperCase(Locale.ROOT).endsWith("CM.DAT");
    }

    private static boolean hasGameData(File dir) {
        File[] files = dir.listFiles();
        if (files == null) {
            return false;
        }
        for (File file : files) {
            if (file.isFile() && isCmDat(file.getName())) {
                return true;
            }
        }
        return false;
    }

    private void buildUi() {
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setBackgroundColor(Color.BLACK);
        int pad = (int) (24 * getResources().getDisplayMetrics().density);
        layout.setPadding(pad, pad, pad, pad);

        TextView title = new TextView(this);
        title.setText("東方紅魔郷 ~ the Embodiment of Scarlet Devil");
        title.setTextColor(Color.WHITE);
        title.setTextSize(20);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        status = new TextView(this);
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(15);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, pad, 0, pad);
        layout.addView(status);
        showIdleText();

        folderButton = new Button(this);
        folderButton.setText("Select game folder / Selecionar pasta");
        folderButton.setOnClickListener(v -> openFolderPicker());
        layout.addView(folderButton);

        zipButton = new Button(this);
        zipButton.setText("Select .zip / Selecionar .zip");
        zipButton.setOnClickListener(v -> openZipPicker());
        layout.addView(zipButton);

        setContentView(layout);
    }

    private void showIdleText() {
        status.setText("Game files not found. Pick your EoSD 1.02h folder or a .zip of it, "
                + "or copy the files to:\n" + importTarget().getAbsolutePath() + "\n\n"
                + "Arquivos do jogo não encontrados. Escolha a pasta do EoSD 1.02h ou um .zip dela, "
                + "ou copie os arquivos para a pasta acima.");
    }

    private void setBusy(boolean busy) {
        copying = busy;
        folderButton.setEnabled(!busy);
        zipButton.setEnabled(!busy);
    }

    private void openFolderPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivityForResult(intent, REQUEST_FOLDER);
        } catch (Exception e) {
            status.setText("No folder picker available. Copy the files manually to:\n"
                    + importTarget().getAbsolutePath());
        }
    }

    private void openZipPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
            "application/zip", "application/x-zip-compressed", "application/octet-stream"
        });
        try {
            startActivityForResult(intent, REQUEST_ZIP);
        } catch (Exception e) {
            status.setText("No file picker available on this device.");
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_ALL_FILES) {
            continueStartup();
            return;
        }
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        Uri uri = data.getData();
        if (requestCode == REQUEST_FOLDER) {
            runImport(() -> importFolder(uri));
        } else if (requestCode == REQUEST_ZIP) {
            runImport(() -> importZip(uri));
        }
    }

    private interface ImportTask {
        void run() throws Exception;
    }

    private void runImport(ImportTask task) {
        setBusy(true);
        status.setText("Copying... / Copiando...");
        new Thread(() -> {
            String error = null;
            try {
                task.run();
            } catch (Exception e) {
                error = e.toString();
            }
            final String result = error;
            runOnUiThread(() -> onImportFinished(result));
        }).start();
    }

    private void onImportFinished(String error) {
        setBusy(false);
        File gameDir = findGameDir();
        if (error == null && gameDir != null) {
            launchGame(gameDir);
            return;
        }
        status.setText((error != null ? "Import failed: " + error + "\n\n" : "")
                + "No *CM.DAT file was found.\nNenhum arquivo *CM.DAT foi encontrado.");
    }

    private void setProgress(String name) {
        runOnUiThread(() -> status.setText("Copying / Copiando:\n" + name));
    }

    private void importFolder(Uri tree) throws Exception {
        ContentResolver resolver = getContentResolver();
        String rootId = DocumentsContract.getTreeDocumentId(tree);
        String gameId = findGameDocument(resolver, tree, rootId, 0);
        copyTree(resolver, tree, gameId != null ? gameId : rootId, importTarget());
    }

    private String findGameDocument(ContentResolver resolver, Uri tree, String documentId, int depth) {
        if (depth > MAX_SEARCH_DEPTH) {
            return null;
        }
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId);
        String[] projection = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        ArrayDeque<String> dirs = new ArrayDeque<>();
        try (Cursor cursor = resolver.query(children, projection, null, null, null)) {
            if (cursor == null) {
                return null;
            }
            while (cursor.moveToNext()) {
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(cursor.getString(2))) {
                    dirs.add(cursor.getString(0));
                } else if (isCmDat(cursor.getString(1))) {
                    return documentId;
                }
            }
        } catch (Exception e) {
            return null;
        }
        for (String dir : dirs) {
            String found = findGameDocument(resolver, tree, dir, depth + 1);
            if (found != null) {
                return found;
            }
        }
        return null;
    }

    private void copyTree(ContentResolver resolver, Uri tree, String documentId, File target) throws Exception {
        target.mkdirs();
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, documentId);
        String[] projection = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try (Cursor cursor = resolver.query(children, projection, null, null, null)) {
            if (cursor == null) {
                return;
            }
            while (cursor.moveToNext()) {
                String childId = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                if (name == null || name.isEmpty()) {
                    continue;
                }
                File out = new File(target, name);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                    copyTree(resolver, tree, childId, out);
                    continue;
                }
                if (isSkipped(name)) {
                    continue;
                }
                setProgress(name);
                Uri file = DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                try (InputStream in = resolver.openInputStream(file)) {
                    if (in != null) {
                        writeStream(in, out);
                    }
                }
            }
        }
    }

    private static boolean isSkipped(String name) {
        String lower = name.toLowerCase(Locale.ROOT);
        return lower.endsWith(".exe") || lower.endsWith(".dll") || lower.endsWith(".lnk");
    }

    private void importZip(Uri uri) throws Exception {
        Charset charset = zipCharset(uri);
        String prefix = findZipPrefix(uri, charset);
        if (prefix == null) {
            throw new IllegalStateException("No *CM.DAT inside the zip");
        }
        File target = importTarget();
        String canonicalTarget = target.getCanonicalPath() + File.separator;
        try (ZipInputStream zip = openZip(uri, charset)) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                String name = entry.getName().replace('\\', '/');
                if (entry.isDirectory() || !name.startsWith(prefix)) {
                    continue;
                }
                String relative = name.substring(prefix.length());
                if (relative.isEmpty() || isSkipped(relative)) {
                    continue;
                }
                File out = new File(target, relative);
                if (!out.getCanonicalPath().startsWith(canonicalTarget)) {
                    continue;
                }
                File parent = out.getParentFile();
                if (parent != null) {
                    parent.mkdirs();
                }
                setProgress(relative);
                writeStream(zip, out);
            }
        }
    }

    private ZipInputStream openZip(Uri uri, Charset charset) throws Exception {
        InputStream in = getContentResolver().openInputStream(uri);
        if (in == null) {
            throw new IllegalStateException("Cannot open the zip");
        }
        if (charset != null && Build.VERSION.SDK_INT >= 24) {
            return new ZipInputStream(in, charset);
        }
        return new ZipInputStream(in);
    }

    private Charset zipCharset(Uri uri) {
        try (ZipInputStream zip = openZip(uri, null)) {
            while (zip.getNextEntry() != null) {
            }
            return null;
        } catch (Exception e) {
            if (Build.VERSION.SDK_INT >= 24 && Charset.isSupported("Shift_JIS")) {
                return Charset.forName("Shift_JIS");
            }
            return null;
        }
    }

    private String findZipPrefix(Uri uri, Charset charset) throws Exception {
        try (ZipInputStream zip = openZip(uri, charset)) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                String name = entry.getName().replace('\\', '/');
                int slash = name.lastIndexOf('/');
                String base = slash >= 0 ? name.substring(slash + 1) : name;
                if (!entry.isDirectory() && isCmDat(base)) {
                    return slash >= 0 ? name.substring(0, slash + 1) : "";
                }
            }
        }
        return null;
    }

    private boolean hasBundledData() {
        try {
            String[] files = getAssets().list(BUNDLED_ASSET_DIR);
            if (files == null) {
                return false;
            }
            for (String file : files) {
                if (isCmDat(file)) {
                    return true;
                }
            }
        } catch (Exception ignored) {
        }
        return false;
    }

    private void importBundled() {
        runImport(() -> copyAssets(getAssets(), BUNDLED_ASSET_DIR, importTarget()));
    }

    private void copyAssets(AssetManager assets, String path, File target) throws Exception {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            setProgress(target.getName());
            try (InputStream in = assets.open(path)) {
                writeStream(in, target);
            }
            return;
        }
        target.mkdirs();
        for (String child : children) {
            copyAssets(assets, path + "/" + child, new File(target, child));
        }
    }

    private static void writeStream(InputStream in, File out) throws Exception {
        File temp = new File(out.getPath() + ".part");
        try (OutputStream os = new FileOutputStream(temp)) {
            byte[] buffer = new byte[1 << 16];
            int read;
            while ((read = in.read(buffer)) > 0) {
                os.write(buffer, 0, read);
            }
        }
        if (out.exists()) {
            out.delete();
        }
        if (!temp.renameTo(out)) {
            throw new IllegalStateException("Cannot write " + out.getPath());
        }
    }

    @Override
    public void onBackPressed() {
        if (!copying) {
            super.onBackPressed();
        }
    }

    private void launchGame(File gameDir) {
        try {
            Os.setenv("TH06_DATA_DIR", gameDir.getAbsolutePath(), true);
        } catch (Exception ignored) {
        }
        startActivity(new Intent(this, TH06Activity.class));
        finish();
    }
}
