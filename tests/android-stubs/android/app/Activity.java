package android.app;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.os.Bundle;

public class Activity extends Context {
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
}
