package androidx.activity;

import android.app.Activity;

public class ComponentActivity extends Activity {
    public final Object activityResultRegistry = new Object();
    private final OnBackPressedDispatcher onBackPressedDispatcher =
        new OnBackPressedDispatcher();

    public OnBackPressedDispatcher getOnBackPressedDispatcher() {
        return onBackPressedDispatcher;
    }
}
