package android.content;

import android.content.pm.PackageManager;

public class Context {
    public static final int MODE_PRIVATE = 0;
    public static final String CONNECTIVITY_SERVICE = "connectivity";

    public Object getSystemService(String name) {
        return null;
    }

    public PackageManager getPackageManager() {
        return new PackageManager();
    }

    public SharedPreferences getSharedPreferences(String n, int m) {
        return new SharedPreferences();
    }
}
