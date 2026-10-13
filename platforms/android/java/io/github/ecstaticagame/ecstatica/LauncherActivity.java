package io.github.ecstaticagame.ecstatica;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/*
 * Game library: imports each game's folder (or a zip of it) into the app's
 * external files directory, imports an optional .sf2 soundfont for music,
 * and starts GameActivity with the game to run. The engine reads
 * <files>/e1 or <files>/e2, so copying the data there by hand (USB, adb push)
 * works too.
 */
public class LauncherActivity extends Activity {
    private static final String[] GAMES = { "e1", "e2" };
    private static final String[] TITLES = { "Ecstatica", "Ecstatica II" };
    private static final int REQ_FOLDER = 1, REQ_ZIP = 2, REQ_SF2 = 3;

    private File filesDir;
    private TextView status;
    private final List<Button> buttons = new ArrayList<>();
    private final TextView[] gameState = new TextView[2];
    private final Button[] playButton = new Button[2];
    private final Button[] removeButton = new Button[2];
    private TextView sf2State;
    private Button sf2Remove;
    private String pendingGame;
    private volatile boolean busy;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        filesDir = getExternalFilesDir(null);
        if (filesDir == null) filesDir = getFilesDir();
        setContentView(buildUi());
        refresh();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!busy) refresh();
    }

    /* ── UI ─────────────────────────────────────────────── */

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private TextView text(String s, float size, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextSize(size);
        t.setTextColor(Color.rgb(0xe8, 0xe2, 0xd6));
        if (bold) t.setTypeface(Typeface.DEFAULT_BOLD);
        return t;
    }

    private Button button(String label, View.OnClickListener l) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setOnClickListener(l);
        buttons.add(b);
        return b;
    }

    private LinearLayout row() {
        LinearLayout r = new LinearLayout(this);
        r.setOrientation(LinearLayout.HORIZONTAL);
        r.setGravity(Gravity.CENTER_VERTICAL);
        return r;
    }

    private View buildUi() {
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(dp(24), dp(24), dp(24), dp(24));
        col.setBackgroundColor(Color.BLACK);

        col.addView(text("Ecstatica", 30, true));
        col.addView(text("You need your own copy of the game. Import the folder that holds "
                + "CODE/ECSTATIC.FAN, or a zip of it.", 14, false));

        for (int i = 0; i < GAMES.length; i++) {
            final String game = GAMES[i];
            TextView title = text(TITLES[i], 20, true);
            title.setPadding(0, dp(20), 0, 0);
            col.addView(title);
            gameState[i] = text("", 14, false);
            col.addView(gameState[i]);
            LinearLayout r = row();
            playButton[i] = button("Play", v -> play(game));
            r.addView(playButton[i]);
            r.addView(button("Import folder", v -> pick(game, REQ_FOLDER)));
            r.addView(button("Import zip", v -> pick(game, REQ_ZIP)));
            removeButton[i] = button("Remove", v -> remove(game));
            r.addView(removeButton[i]);
            col.addView(r);
        }

        TextView music = text("Music (optional)", 20, true);
        music.setPadding(0, dp(20), 0, 0);
        col.addView(music);
        sf2State = text("", 14, false);
        col.addView(sf2State);
        LinearLayout r = row();
        r.addView(button("Import .sf2 soundfont", v -> pick(null, REQ_SF2)));
        sf2Remove = button("Remove", v -> {
            File sf = soundfont();
            if (sf != null) sf.delete();
            refresh();
        });
        r.addView(sf2Remove);
        col.addView(r);

        status = text("", 14, false);
        status.setPadding(0, dp(16), 0, 0);
        col.addView(status);

        TextView notes = text("Ecstatica 1 needs the DOS release root with its W folder inside, "
                + "for the 640x480 graphics. Installers, DIRECTX, ISHIELD, .EXE and .DLL files are "
                + "skipped. You can also copy the folders by hand to:\n"
                + filesDir.getAbsolutePath() + "/e1 and /e2\n\n"
                + "Saved games are kept in each game's folder; Remove deletes them too. "
                + "Music needs a General MIDI soundfont (.sf2), such as GeneralUser GS.", 13, false);
        notes.setTextColor(Color.rgb(0x9a, 0x92, 0x84));
        notes.setPadding(0, dp(16), 0, 0);
        col.addView(notes);

        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(Color.BLACK);
        scroll.addView(col);
        return scroll;
    }

    private void setBusy(boolean b) {
        busy = b;
        for (Button btn : buttons) btn.setEnabled(!b);
        if (!b) refresh();
    }

    private void refresh() {
        for (int i = 0; i < GAMES.length; i++) {
            boolean ok = installed(GAMES[i]);
            gameState[i].setText(ok ? "Installed." : "Not imported yet.");
            playButton[i].setEnabled(ok && !busy);
            removeButton[i].setEnabled(ok && !busy);
        }
        File sf = soundfont();
        sf2State.setText(sf != null ? "Using " + sf.getName() + "." : "No soundfont: music is silent.");
        sf2Remove.setEnabled(sf != null && !busy);
    }

    private void setStatus(final String s) {
        runOnUiThread(() -> status.setText(s));
    }

    /* ── Library ────────────────────────────────────────── */

    private static File childCI(File dir, String name) {
        String[] names = dir.list();
        if (names == null) return null;
        for (String n : names)
            if (n.equalsIgnoreCase(name)) return new File(dir, n);
        return null;
    }

    private boolean installed(String game) {
        File code = childCI(new File(filesDir, game), "CODE");
        return code != null && childCI(code, "ECSTATIC.FAN") != null;
    }

    private File soundfont() {
        File[] files = filesDir.listFiles();
        if (files == null) return null;
        for (File f : files)
            if (f.isFile() && f.getName().toLowerCase(Locale.ROOT).endsWith(".sf2")) return f;
        return null;
    }

    private static void deleteTree(File f) {
        File[] kids = f.listFiles();
        if (kids != null) for (File k : kids) deleteTree(k);
        f.delete();
    }

    private void remove(String game) {
        deleteTree(new File(filesDir, game));
        refresh();
    }

    private void play(String game) {
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra("game", game);
        startActivity(i);
    }

    /* ── Import ─────────────────────────────────────────── */

    private void pick(String game, int req) {
        pendingGame = game;
        Intent i;
        if (req == REQ_FOLDER) {
            i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        } else {
            i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType(req == REQ_ZIP ? "application/zip" : "*/*");
        }
        startActivityForResult(i, req);
    }

    @Override
    protected void onActivityResult(int req, int result, Intent data) {
        super.onActivityResult(req, result, data);
        if (result != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        final String game = pendingGame;
        setBusy(true);
        new Thread(() -> {
            String err = null;
            try {
                if (req == REQ_FOLDER) importTree(game, uri);
                else if (req == REQ_ZIP) importZip(game, uri);
                else importSoundfont(uri);
            } catch (Exception e) {
                err = e.getMessage() != null ? e.getMessage() : e.toString();
            }
            final String msg = err == null ? "Import finished." : "Import failed: " + err;
            runOnUiThread(() -> {
                status.setText(msg);
                setBusy(false);
            });
        }).start();
    }

    private static final String[] SKIP_DIRS = { "directx", "ishield", "__macosx" };
    private static final String[] SKIP_EXT = { ".exe", ".dll", ".ply" };

    private static boolean wanted(List<String> parts) {
        for (String p : parts) {
            String low = p.toLowerCase(Locale.ROOT);
            if (low.startsWith(".")) return false;
            for (String d : SKIP_DIRS) if (low.equals(d)) return false;
        }
        String name = parts.get(parts.size() - 1).toLowerCase(Locale.ROOT);
        for (String e : SKIP_EXT) if (name.endsWith(e)) return false;
        return true;
    }

    private File freshGameDir(String game) throws IOException {
        File dir = new File(filesDir, game);
        deleteTree(dir);
        if (!dir.mkdirs()) throw new IOException("cannot create " + dir);
        return dir;
    }

    private static long copy(InputStream in, File dst) throws IOException {
        File parent = dst.getParentFile();
        if (parent != null) parent.mkdirs();
        long n = 0;
        try (OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[1 << 16];
            int r;
            while ((r = in.read(buf)) > 0) {
                out.write(buf, 0, r);
                n += r;
            }
        }
        return n;
    }

    /* A document in a picked tree. */
    private static final class Doc {
        final String id, name;
        final boolean dir;
        final long size;
        Doc(String id, String name, boolean dir, long size) {
            this.id = id; this.name = name; this.dir = dir; this.size = size;
        }
    }

    private List<Doc> children(Uri tree, String parentId) {
        List<Doc> out = new ArrayList<>();
        Uri kids = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentId);
        String[] cols = { Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME,
                          Document.COLUMN_MIME_TYPE, Document.COLUMN_SIZE };
        try (Cursor c = getContentResolver().query(kids, cols, null, null, null)) {
            while (c != null && c.moveToNext())
                out.add(new Doc(c.getString(0), c.getString(1),
                                Document.MIME_TYPE_DIR.equals(c.getString(2)), c.getLong(3)));
        }
        return out;
    }

    private Doc childCI(List<Doc> docs, String name, boolean dir) {
        for (Doc d : docs) if (d.dir == dir && d.name.equalsIgnoreCase(name)) return d;
        return null;
    }

    /* The game root holds CODE/ECSTATIC.FAN; look up to two levels down. */
    private String findRoot(Uri tree, String id, int depth) {
        List<Doc> kids = children(tree, id);
        Doc code = childCI(kids, "CODE", true);
        if (code != null && childCI(children(tree, code.id), "ECSTATIC.FAN", false) != null) return id;
        if (depth == 0) return null;
        for (Doc d : kids) {
            if (!d.dir) continue;
            String r = findRoot(tree, d.id, depth - 1);
            if (r != null) return r;
        }
        return null;
    }

    private void listTree(Uri tree, String id, List<String> path, List<Object[]> out) {
        for (Doc d : children(tree, id)) {
            List<String> p = new ArrayList<>(path);
            p.add(d.name);
            if (!wanted(p)) continue;
            if (d.dir) listTree(tree, d.id, p, out);
            else out.add(new Object[] { p, d });
        }
    }

    @SuppressWarnings("unchecked")
    private void importTree(String game, Uri tree) throws IOException {
        setStatus("Looking for the game…");
        String rootId = findRoot(tree, DocumentsContract.getTreeDocumentId(tree), 2);
        if (rootId == null) throw new IOException("no CODE/ECSTATIC.FAN in that folder");
        List<Object[]> files = new ArrayList<>();
        listTree(tree, rootId, new ArrayList<>(), files);
        long total = 0, done = 0;
        for (Object[] f : files) total += ((Doc) f[1]).size;

        File dir = freshGameDir(game);
        ContentResolver cr = getContentResolver();
        for (Object[] f : files) {
            List<String> parts = (List<String>) f[0];
            Doc d = (Doc) f[1];
            Uri u = DocumentsContract.buildDocumentUriUsingTree(tree, d.id);
            try (InputStream in = cr.openInputStream(u)) {
                if (in == null) throw new IOException("cannot read " + d.name);
                done += copy(in, new File(dir, String.join("/", parts)));
            }
            setStatus(String.format(Locale.ROOT, "Importing… %d%%", total > 0 ? done * 100 / total : 100));
        }
    }

    /* Two passes over the stream: find the root, then copy below it. */
    private void importZip(String game, Uri uri) throws IOException {
        setStatus("Reading the zip…");
        ContentResolver cr = getContentResolver();
        String prefix = null;
        try (ZipInputStream zin = new ZipInputStream(cr.openInputStream(uri))) {
            for (ZipEntry e; (e = zin.getNextEntry()) != null; ) {
                String n = e.getName().replace('\\', '/');
                String low = n.toLowerCase(Locale.ROOT);
                if (low.equals("code/ecstatic.fan") || low.endsWith("/code/ecstatic.fan")) {
                    String p = n.substring(0, n.length() - "code/ecstatic.fan".length());
                    if (prefix == null || p.length() < prefix.length()) prefix = p;
                }
            }
        }
        if (prefix == null) throw new IOException("no CODE/ECSTATIC.FAN in the zip");

        File dir = freshGameDir(game);
        int count = 0;
        try (ZipInputStream zin = new ZipInputStream(cr.openInputStream(uri))) {
            for (ZipEntry e; (e = zin.getNextEntry()) != null; ) {
                String n = e.getName().replace('\\', '/');
                if (e.isDirectory() || !n.startsWith(prefix)) continue;
                String rel = n.substring(prefix.length());
                List<String> parts = new ArrayList<>();
                for (String p : rel.split("/")) if (!p.isEmpty()) parts.add(p);
                if (parts.isEmpty() || !wanted(parts) || parts.contains("..")) continue;
                copy(zin, new File(dir, String.join("/", parts)));
                if (++count % 50 == 0) setStatus("Importing… " + count + " files");
            }
        }
    }

    private void importSoundfont(Uri uri) throws IOException {
        String name = "soundfont.sf2";
        try (Cursor c = getContentResolver().query(uri, new String[] { OpenableColumns.DISPLAY_NAME },
                                                  null, null, null)) {
            if (c != null && c.moveToFirst() && c.getString(0) != null) name = c.getString(0);
        }
        if (!name.toLowerCase(Locale.ROOT).endsWith(".sf2"))
            throw new IOException("not an .sf2 soundfont");
        File old = soundfont();
        if (old != null) old.delete();
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IOException("cannot read " + name);
            copy(in, new File(filesDir, name.replaceAll("[^A-Za-z0-9._-]", "_")));
        }
    }
}
