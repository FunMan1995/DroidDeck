package com.droiddeck.launcher.gpu;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import com.droiddeck.launcher.R;
import com.droiddeck.launcher.core.FileUtils;
import com.droiddeck.launcher.core.TarZst;

import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public class LinuxVulkanDriverManager {
    private static final String TAG = "LinuxVulkanDriver";
    public static final String DIR_NAME = "linux_vulkan_drivers";
    public static final String LIB_NAME = "libvulkan_freedreno.so";
    public static final String ICD_NAME = "icd.json";
    public static final String META_NAME = "meta.json";
    /** Reserved id of the bundled Vortek forwarder (brief 003); imports never get it. */
    public static final String VORTEK_ID = "vortek";
    public static final String VORTEK_LIB = "libvulkan_vortek.so";
    public static final String VORTEK_ASSET = "graphics_driver/vortek-client.tzst";
    static final String VORTEK_API = "1.3.0";
    private static final String STAMP_NAME = ".asset-stamp";

    private final Context context;
    private final File rootDir;

    public LinuxVulkanDriverManager(Context context) {
        this.context = context;
        this.rootDir = new File(context.getFilesDir(), DIR_NAME);
        if (!rootDir.exists()) rootDir.mkdirs();
    }

    public File getDriverDir(String id) {
        return new File(rootDir, id);
    }

    public boolean isInstalled(String id) {
        if (id == null || id.isEmpty() || id.contains("/") || id.contains("..")) return false;
        File dir = getDriverDir(id);
        return new File(dir, libraryName(id)).isFile() && new File(dir, ICD_NAME).isFile();
    }

    static String libraryName(String id) {
        return VORTEK_ID.equals(id) ? VORTEK_LIB : LIB_NAME;
    }

    public String getIcdPath(String id) {
        return isInstalled(id) ? new File(getDriverDir(id), ICD_NAME).getAbsolutePath() : null;
    }

    public List<String> enumerateInstalledDrivers() {
        ArrayList<String> ids = new ArrayList<>();
        File[] dirs = rootDir.listFiles();
        if (dirs == null) return ids;
        for (File d : dirs) {
            // The bundled forwarder is selected by SessionPrefs.maliForwarder, not offered as an import.
            if (d.isDirectory() && !VORTEK_ID.equals(d.getName()) && isInstalled(d.getName())) ids.add(d.getName());
        }
        Collections.sort(ids);
        return ids;
    }

    private JSONObject readMeta(String id) {
        try {
            File meta = new File(getDriverDir(id), META_NAME);
            if (!meta.isFile()) return null;
            return new JSONObject(FileUtils.readString(meta));
        } catch (Exception e) {
            return null;
        }
    }

    public String getDriverName(String id) {
        JSONObject m = readMeta(id);
        String name = m != null ? m.optString("name", "") : "";
        return name.isEmpty() ? id : name;
    }

    public String getDriverVersion(String id) {
        JSONObject m = readMeta(id);
        return m != null ? m.optString("driverVersion", "") : "";
    }

    public String getMinGlibc(String id) {
        JSONObject m = readMeta(id);
        return m != null ? m.optString("minGlibc", "") : "";
    }

    public void removeDriver(String id) {
        if (id == null || id.isEmpty() || id.contains("/") || id.contains("..")) return;
        Log.d(TAG, "removing imported Linux Vulkan driver " + id);
        FileUtils.delete(getDriverDir(id));
    }

    public String installDriver(Uri zipUri, String displayName) throws IOException {
        File tmpDir = newStagingDir();
        boolean keep = false;
        try {
            String soName = null;
            JSONObject zipMeta = null;
            try (InputStream is = context.getContentResolver().openInputStream(zipUri);
                 ZipInputStream zis = new ZipInputStream(is)) {
                if (is == null) throw new IOException("cannot open " + zipUri);
                ZipEntry entry;
                while ((entry = zis.getNextEntry()) != null) {
                    if (entry.isDirectory()) continue;
                    String base = new File(entry.getName()).getName();
                    if (base.isEmpty()) continue;
                    if (base.startsWith("libvulkan_freedreno") && base.endsWith(".so")) {
                        if (soName != null) Log.w(TAG, "zip has several libvulkan_freedreno*.so; using the first (" + soName + ")");
                        else {
                            Files.copy(zis, new File(tmpDir, LIB_NAME).toPath(), StandardCopyOption.REPLACE_EXISTING);
                            soName = base;
                        }
                    } else if (base.equals(META_NAME)) {
                        try {
                            zipMeta = new JSONObject(new String(readAll(zis), StandardCharsets.UTF_8));
                        } catch (Exception e) {
                            Log.w(TAG, "meta.json unreadable, ignoring: " + e.getMessage());
                        }
                    }
                }
            }
            String id = adopt(tmpDir, soName, zipMeta, displayName);
            keep = true;
            return id;
        } finally {
            if (!keep) FileUtils.delete(tmpDir);
        }
    }

    File newStagingDir() throws IOException {
        File tmpDir = new File(rootDir, ".tmp-" + System.nanoTime());
        FileUtils.delete(tmpDir);
        if (!tmpDir.mkdirs()) throw new IOException("cannot create " + tmpDir);
        return tmpDir;
    }

    String adopt(File tmpDir, String soName, JSONObject zipMeta, String displayName) throws IOException {
        try {
            if (soName == null) {
                throw new IllegalArgumentException(context.getString(R.string.lvd_no_library));
            }
            File so = new File(tmpDir, LIB_NAME);
            if (!isAarch64Elf(so)) {
                throw new IllegalArgumentException(context.getString(R.string.lvd_not_aarch64, soName));
            }
            if (!containsAscii(so, "libc.so.6")) {
                throw new IllegalArgumentException(context.getString(R.string.lvd_not_glibc, soName));
            }
            String kind = zipMeta != null ? zipMeta.optString("kind", "") : "";
            if (!kind.isEmpty() && !"linux-vulkan-icd".equals(kind)) {
                Log.w(TAG, "meta.json says kind=" + kind + ", importing anyway (the binary is glibc)");
            }

            String name = zipMeta != null ? zipMeta.optString("name", "") : "";
            String version = zipMeta != null ? zipMeta.optString("driverVersion", "") : "";
            String minGlibc = zipMeta != null ? zipMeta.optString("minGlibc", "") : "";
            if (name.isEmpty()) {
                name = displayName != null ? displayName : "linux-driver";
                if (name.toLowerCase().endsWith(".zip")) name = name.substring(0, name.length() - 4);
            }
            String id = uniqueId(sanitizeId(name));
            File dir = getDriverDir(id);

            String api = zipMeta != null ? zipMeta.optString("apiVersion", "") : "";
            writeIcd(tmpDir, new File(dir, LIB_NAME), icdApiVersion(api));

            JSONObject meta = new JSONObject();
            meta.put("schemaVersion", 1);
            meta.put("kind", "linux-vulkan-icd");
            meta.put("name", name);
            meta.put("driverVersion", version);
            meta.put("libc", "glibc");
            meta.put("minGlibc", minGlibc);
            meta.put("sourceLibraryName", soName);
            meta.put("importedAt", System.currentTimeMillis());
            if (!FileUtils.writeString(new File(tmpDir, META_NAME), meta.toString(2))) throw new IOException("cannot write meta.json");

            if (!tmpDir.renameTo(dir)) throw new IOException("cannot move into " + dir);
            Log.i(TAG, "imported Linux Vulkan driver " + id + " (" + soName + ", minGlibc=" + minGlibc + ") -> " + dir);
            return id;
        } catch (org.json.JSONException e) {
            throw new IOException("manifest write failed: " + e.getMessage());
        }
    }

    private String uniqueId(String base) {
        return uniqueId(base, id -> getDriverDir(id).exists());
    }

    /** Never returns {@link #VORTEK_ID}, even when that directory does not exist yet. */
    static String uniqueId(String base, java.util.function.Predicate<String> taken) {
        if (VORTEK_ID.equals(base)) base = base + "-imported";
        String id = base;
        int n = 2;
        while (VORTEK_ID.equals(id) || taken.test(id)) id = base + "-" + (n++);
        return id;
    }

    /** api_version for an imported ICD: meta.json "apiVersion" when it is a 1.x.y, else the old default. */
    static String icdApiVersion(String fromMeta) {
        return fromMeta != null && fromMeta.matches("1\\.[0-4]\\.\\d+") ? fromMeta : "1.1.274";
    }

    private static void writeIcd(File dir, File library, String apiVersion) throws IOException, org.json.JSONException {
        JSONObject icd = new JSONObject();
        icd.put("file_format_version", "1.0.0");
        JSONObject icdBody = new JSONObject();
        icdBody.put("library_path", library.getAbsolutePath());
        icdBody.put("api_version", apiVersion);
        icd.put("ICD", icdBody);
        if (!FileUtils.writeString(new File(dir, ICD_NAME), icd.toString(2))) throw new IOException("cannot write icd.json");
    }

    /**
     * Installs the bundled Vortek client (built from source, tools/vortek) into
     * getDriverDir(VORTEK_ID) once per APK update and writes its icd.json (api 1.3.0).
     */
    public synchronized File ensureVortek() throws IOException {
        File dir = getDriverDir(VORTEK_ID);
        String stamp = assetStamp();
        File stampFile = new File(dir, STAMP_NAME);
        if (isInstalled(VORTEK_ID) && stampFile.isFile() && stamp.equals(FileUtils.readString(stampFile))) return dir;
        File tmpDir = newStagingDir();
        boolean keep = false;
        try {
            if (!TarZst.extractAsset(context, VORTEK_ASSET, tmpDir)) throw new IOException("cannot unpack " + VORTEK_ASSET);
            File so = new File(tmpDir, VORTEK_LIB);
            if (!isAarch64Elf(so)) throw new IOException(VORTEK_ASSET + " has no aarch64 " + VORTEK_LIB);
            writeIcd(tmpDir, new File(dir, VORTEK_LIB), VORTEK_API);
            JSONObject meta = new JSONObject();
            meta.put("schemaVersion", 1);
            meta.put("kind", "linux-vulkan-icd");
            meta.put("name", "Vortek (Mali forwarder)");
            meta.put("libc", "glibc");
            meta.put("bundled", true);
            if (!FileUtils.writeString(new File(tmpDir, META_NAME), meta.toString(2))) throw new IOException("cannot write meta.json");
            if (!FileUtils.writeString(new File(tmpDir, STAMP_NAME), stamp)) throw new IOException("cannot write stamp");
            FileUtils.delete(dir);
            if (!tmpDir.renameTo(dir)) throw new IOException("cannot move into " + dir);
            keep = true;
            Log.i(TAG, "installed bundled Vortek client -> " + dir);
            return dir;
        } catch (org.json.JSONException e) {
            throw new IOException("manifest write failed: " + e.getMessage());
        } finally {
            if (!keep) FileUtils.delete(tmpDir);
        }
    }

    private String assetStamp() {
        try {
            return String.valueOf(context.getPackageManager().getPackageInfo(context.getPackageName(), 0).lastUpdateTime);
        } catch (Exception e) {
            return "0";
        }
    }

    static String sanitizeId(String name) {
        String s = name.trim().replaceAll("[^A-Za-z0-9._-]+", "_").replaceAll("^[._]+", "");
        if (s.length() > 64) s = s.substring(0, 64);
        return s.isEmpty() ? "linux-driver" : s;
    }

    static boolean isAarch64Elf(File f) {
        try (FileInputStream in = new FileInputStream(f)) {
            byte[] h = new byte[20];
            int n = 0;
            while (n < h.length) {
                int r = in.read(h, n, h.length - n);
                if (r < 0) break;
                n += r;
            }
            if (n < 20) return false;
            if (h[0] != 0x7f || h[1] != 'E' || h[2] != 'L' || h[3] != 'F') return false;
            if (h[4] != 2 || h[5] != 1) return false;
            int machine = (h[18] & 0xff) | ((h[19] & 0xff) << 8);
            return machine == 0xB7;
        } catch (IOException e) {
            return false;
        }
    }

    private static byte[] readAll(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int r;
        while ((r = in.read(buf)) > 0) bos.write(buf, 0, r);
        return bos.toByteArray();
    }

    static boolean containsAscii(File f, String needle) {
        byte[] nb = needle.getBytes(StandardCharsets.US_ASCII);
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[1 << 16];
            int carry = 0;
            int r;
            while ((r = in.read(buf, carry, buf.length - carry)) > 0) {
                int len = carry + r;
                for (int i = 0; i + nb.length <= len; i++) {
                    int j = 0;
                    while (j < nb.length && buf[i + j] == nb[j]) j++;
                    if (j == nb.length) return true;
                }
                carry = Math.min(nb.length - 1, len);
                System.arraycopy(buf, len - carry, buf, 0, carry);
            }
        } catch (IOException e) {
            return false;
        }
        return false;
    }
}
