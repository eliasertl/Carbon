package io.github.eliasertl.carbon.gallery;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.view.WindowManager;

import androidx.activity.OnBackPressedCallback;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsControllerCompat;

import com.google.androidgamesdk.GameActivity;

/**
 * The Gallery's activity. GameActivity runs the native code (AndroidMain.cpp) on a thread of its own and hands it
 * the surface, touches, keys, insets and the soft keyboard; this class only sets up the window.
 */
public class MainActivity extends GameActivity {
    /**
     * Back (the gesture or the button) goes to the native code while it has somewhere to go back to: a page of
     * the Gallery on a phone returns to the list of pages. Otherwise the system handles it and leaves the app.
     */
    private final OnBackPressedCallback mBackCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            nativeOnBack();
        }
    };

    private native void nativeOnBack();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Edge to edge: the interface reaches under the status bar, the navigation bar and into the display
        // cutout, and Carbon keeps its controls inside the safe area the native code reports.
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        getWindow().setAttributes(attributes);
        // Before Android 15, which draws edge to edge by itself, the bars are made transparent here.
        if (Build.VERSION.SDK_INT < 35) {
            makeSystemBarsTransparent();
        }
        super.onCreate(savedInstanceState);
        getOnBackPressedDispatcher().addCallback(this, mBackCallback);
    }

    @SuppressWarnings("deprecation")
    private void makeSystemBarsTransparent() {
        getWindow().setStatusBarColor(0);
        getWindow().setNavigationBarColor(0);
    }

    /**
     * Called by the native code when the interface's appearance changes: dark icons in the system bars over a
     * light interface, light ones over a dark interface.
     */
    public void setLightSystemBars(boolean light) {
        runOnUiThread(() -> {
            WindowInsetsControllerCompat controller =
                    WindowCompat.getInsetsController(getWindow(), getWindow().getDecorView());
            controller.setAppearanceLightStatusBars(light);
            controller.setAppearanceLightNavigationBars(light);
        });
    }

    /** Called by the native code when it starts or stops having somewhere to go back to. */
    public void setBackHandled(boolean handled) {
        runOnUiThread(() -> mBackCallback.setEnabled(handled));
    }

    /** The clipboard's text, or an empty string; called by the native code when Carbon pastes. */
    public String getClipboardText() {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = clipboard != null ? clipboard.getPrimaryClip() : null;
        if (clip == null || clip.getItemCount() == 0) {
            return "";
        }
        CharSequence text = clip.getItemAt(0).coerceToText(this);
        return text != null ? text.toString() : "";
    }

    /** Replaces the clipboard's text; called by the native code when Carbon copies or cuts. */
    public void setClipboardText(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(ClipData.newPlainText("Carbon", text));
        }
    }
}
