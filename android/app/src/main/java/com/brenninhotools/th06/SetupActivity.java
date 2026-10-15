package com.brenninhotools.th06;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.provider.Settings;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

public class SetupActivity extends Activity {
    private static final int REQUEST_FOLDER = 6;
    private static final int REQUEST_ALL_FILES = 7;
    private static final int REQUEST_STORAGE = 8;
    private static final String PREF_ASKED_STORAGE = "askedStorage";

    private TextView status;
    private Button pickButton;
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
        if (hasGameData(getDataDir())) {
            launchGame();
            return;
        }
        buildUi();
    }

    private File getDataDir() {
        File dir = getExternalFilesDir(null);
        if (dir == null) {
            dir = getFilesDir();
        }
        dir.mkdirs();
        return dir;
    }

    private static boolean hasGameData(File dir) {
        File[] files = dir.listFiles();
        if (files == null) {
            return false;
        }
        for (File file : files) {
            if (file.isFile() && file.getName().toUpperCase(Locale.ROOT).endsWith("CM.DAT")) {
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
        status.setText("Game files not found.\n\nSelect the folder that contains 紅魔郷CM.DAT and the other .DAT files "
                + "(and the bgm folder, if you have it). They will be copied into the app.\n\n"
                + "Arquivos do jogo não encontrados. Selecione a pasta com o 紅魔郷CM.DAT e os outros .DAT.");
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(15);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, pad, 0, pad);
        layout.addView(status);

        pickButton = new Button(this);
        pickButton.setText("Select game folder / Selecionar pasta");
        pickButton.setOnClickListener(v -> openPicker());
        layout.addView(pickButton);

        setContentView(layout);
    }

    private void openPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivityForResult(intent, REQUEST_FOLDER);
        } catch (Exception e) {
            status.setText("No folder picker available on this device.\n\nCopy the files manually to:\n"
                    + getDataDir().getAbsolutePath());
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_ALL_FILES) {
            continueStartup();
            return;
        }
        if (requestCode != REQUEST_FOLDER || resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        Uri tree = data.getData();
        copying = true;
        pickButton.setEnabled(false);
        status.setText("Copying... / Copiando...");
        new Thread(() -> {
            String error = null;
            try {
                String rootId = DocumentsContract.getTreeDocumentId(tree);
                copyTree(getContentResolver(), tree, rootId, getDataDir());
            } catch (Exception e) {
                error = e.toString();
            }
            final String result = error;
            runOnUiThread(() -> onCopyFinished(result));
        }).start();
    }

    private void onCopyFinished(String error) {
        copying = false;
        pickButton.setEnabled(true);
        if (error == null && hasGameData(getDataDir())) {
            launchGame();
            return;
        }
        status.setText((error != null ? "Copy failed: " + error + "\n\n" : "")
                + "No *CM.DAT file was found in the selected folder.\n"
                + "Nenhum arquivo *CM.DAT foi encontrado na pasta escolhida.");
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
                String lower = name.toLowerCase(Locale.ROOT);
                if (lower.endsWith(".exe") || lower.endsWith(".dll")) {
                    continue;
                }
                final String shown = name;
                runOnUiThread(() -> status.setText("Copying / Copiando: " + shown));
                Uri file = DocumentsContract.buildDocumentUriUsingTree(tree, childId);
                try (InputStream in = resolver.openInputStream(file); OutputStream os = new FileOutputStream(out)) {
                    if (in == null) {
                        continue;
                    }
                    byte[] buffer = new byte[1 << 16];
                    int read;
                    while ((read = in.read(buffer)) > 0) {
                        os.write(buffer, 0, read);
                    }
                }
            }
        }
    }

    @Override
    public void onBackPressed() {
        if (!copying) {
            super.onBackPressed();
        }
    }

    private void launchGame() {
        startActivity(new Intent(this, TH06Activity.class));
        finish();
    }
}
