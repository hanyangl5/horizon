package com.horizon.engine;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

public abstract class HorizonActivity extends Activity implements SurfaceHolder.Callback {

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

        nativeOnCreate(this, getAssets());
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
