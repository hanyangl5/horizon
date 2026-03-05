package com.horizon.engine;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

public abstract class HorizonActivity extends Activity implements SurfaceHolder.Callback {

    private static final String TAG = "HorizonActivity";
    private static final int REQ_STORAGE_PERMISSION = 1001;
    private static final int REQ_MANAGE_EXTERNAL = 1002;

    private boolean nativeCreated = false;

    protected abstract String getNativeLibraryName();

    static native void nativeOnCreate(Activity activity, AssetManager assetManager);
    static native void nativeOnDestroy();
    static native void nativeOnPause();
    static native void nativeOnResume();
    static native void nativeOnSurfaceCreated(Surface surface);
    static native void nativeOnSurfaceDestroyed();
    static native void nativeOnSurfaceChanged(int width, int height);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        System.loadLibrary(getNativeLibraryName());

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        SurfaceView surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        setContentView(surfaceView);

        if (ensureStoragePermission()) {
            startNativeCreate();
        }
    }

    private void startNativeCreate() {
        if (nativeCreated) {
            return;
        }
        nativeOnCreate(this, getAssets());
        nativeCreated = true;
    }

    private boolean ensureStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (Environment.isExternalStorageManager()) {
                return true;
            }
            try {
                Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
                startActivityForResult(intent, REQ_MANAGE_EXTERNAL);
            } catch (Exception e) {
                Log.e(TAG, "Failed to launch MANAGE_EXTERNAL_STORAGE settings", e);
                Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                startActivityForResult(intent, REQ_MANAGE_EXTERNAL);
            }
            return false;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            if (checkSelfPermission(android.Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{android.Manifest.permission.READ_EXTERNAL_STORAGE},
                    REQ_STORAGE_PERMISSION);
                return false;
            }
        }

        return true;
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_STORAGE_PERMISSION) {
            boolean granted = grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED;
            if (!granted) {
                Log.e(TAG, "READ_EXTERNAL_STORAGE permission denied");
            }
            startNativeCreate();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_MANAGE_EXTERNAL) {
            if (!Environment.isExternalStorageManager()) {
                Log.e(TAG, "MANAGE_EXTERNAL_STORAGE not granted");
            }
            startNativeCreate();
        }
    }

    @Override
    protected void onDestroy() {
        nativeOnDestroy();
        super.onDestroy();
    }

    @Override
    protected void onPause() {
        nativeOnPause();
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeOnResume();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        nativeOnSurfaceCreated(holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeOnSurfaceDestroyed();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        nativeOnSurfaceChanged(width, height);
    }
}
