package android.app;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.os.Bundle;
import android.view.Window;

public class Activity extends Context {
    private final Window window = new Window();

    public Intent getIntent() {
        return new Intent();
    }

    @Override
    public PackageManager getPackageManager() {
        return new PackageManager();
    }

    @Override
    public SharedPreferences getSharedPreferences(String n, int m) {
        return new SharedPreferences();
    }

    @Override
    public Object getSystemService(String name) {
        if (CONNECTIVITY_SERVICE.equals(name)) {
            return new ConnectivityManager();
        }
        return null;
    }

    protected void onCreate(Bundle b) {}

    protected void onDestroy() {}

    public void finish() {}

    public boolean isFinishing() { return false; }

    public void runOnUiThread(Runnable action) { action.run(); }

    public void setContentView(Object view) {}

    public void overridePendingTransition(int enterAnim, int exitAnim) {}

    public Window getWindow() { return window; }
}
