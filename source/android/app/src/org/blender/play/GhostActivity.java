package org.blender.play;



import android.app.Activity;
import android.os.Bundle;
import android.view.*;
import android.content.*;
import android.util.*;
import android.net.*;
import javax.microedition.khronos.egl.*;





public class GhostActivity extends Activity
{
    /** Called when the activity is first created. */
	private GhostSurface mainSurface;
	
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        
        
        Uri data = getIntent().getData();
        if(data != null){
      
        Log.i("Blender","Started");
        mainSurface = new GhostSurface(data.getPath(), getApplication());
        setContentView(mainSurface);
        SurfaceHolder holder = mainSurface.getHolder();
        Log.v("Blender", "Surface valid: " + Boolean.toString(holder.getSurface().isValid()));
        //mainSurface.initSurface();
     //setContentView(R.layout.main);
        }
	    
    }
    
    public void onStart()
    {
    	super.onStart();
    	
	

    }
    
    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
    	Log.i("My", "Testa "+event.getAction()+" " + +event.getYPrecision());
    	return true;
    }
    
    @Override
    public void onPause()
    {
    	super.onPause();
    	BlenderNativeAPI.actionClose();
    	
    	finish();    	
    }
    
}



class GhostSurface extends SurfaceView implements SurfaceHolder.Callback {

	private EGLDisplay egldisplay = null;
	private EGLSurface surface = null;
	private EGL10 eglc;
	private EGLContext mEGLContext;
	
	private String filepath;
	
	
	@Override
	public boolean onTouchEvent(MotionEvent event)
	{ 

		BlenderNativeAPI.eventTouch(event.getAction(), event.getX(), event.getY());
	return true;
	}
	
	public GhostSurface(String filepath, Context context)
	{
		super(context);
		
		this.filepath = filepath;
		
		getHolder().addCallback(this); 
		getHolder().setType(SurfaceHolder.SURFACE_TYPE_GPU);
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
	}
	
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i("Blender", "Surface Created, Starting Blender");
        holder.setType(SurfaceHolder.SURFACE_TYPE_GPU);
        //this.initSurface();
        BlenderNativeAPI.SetScreen(this);
        BlenderNativeAPI.StartBlender(filepath);
        
    	BlenderNativeAPI.eventWindowsResize(this.getWidth(), this.getHeight());
    	BlenderNativeAPI.eventWindowsFocus();
    	BlenderNativeAPI.eventWindowsUpdate();
    }
	
    
    public void surfaceChanged(SurfaceHolder holder,
            int format, int width, int height) {
		Log.i("Blender", "New size " + width + "x" + height);
	    
		BlenderNativeAPI.eventWindowsResize(width, height);
		BlenderNativeAPI.eventWindowsFocus();
		BlenderNativeAPI.eventWindowsUpdate();
	}
    
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i("Blender", "Surface Destroyed");
    }
    
    public void SwapBuffers() {
        eglc = (EGL10)EGLContext.getEGL();
        
        eglc.eglWaitNative(EGL10.EGL_CORE_NATIVE_ENGINE, null);
        eglc.eglWaitGL();
        if(!eglc.eglSwapBuffers(egldisplay, surface)) 
        {
        	Log.v("Blender", "SwappBuffers Error:" + eglc.eglGetError());
        }

    }
	
	public void initSurface()
	{
		 eglc = (EGL10)EGLContext.getEGL();
		 
		 egldisplay = eglc.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
		 
		 int[] ver = {0, 0};
		 eglc.eglInitialize(egldisplay, ver);
		 
		 int[][] configSpec = { 
				 {
				    EGL10.EGL_RED_SIZE, 8,
				    EGL10.EGL_BLUE_SIZE, 8,
				    EGL10.EGL_GREEN_SIZE, 8,
				    EGL10.EGL_DEPTH_SIZE,   16,
				    EGL10.EGL_ALPHA_SIZE, 8,
		            EGL10.EGL_RENDERABLE_TYPE,  4,
		            EGL10.EGL_NONE
				 },
				 {
				    EGL10.EGL_RED_SIZE, 6,
				    EGL10.EGL_BLUE_SIZE, 6,
				    EGL10.EGL_GREEN_SIZE, 6,
				    EGL10.EGL_DEPTH_SIZE,   16,
				    EGL10.EGL_ALPHA_SIZE, 8,
		            EGL10.EGL_RENDERABLE_TYPE,  4,
		            EGL10.EGL_NONE
				},				 
				 {
				    EGL10.EGL_RED_SIZE, 5,
				    EGL10.EGL_BLUE_SIZE, 6,
				    EGL10.EGL_GREEN_SIZE, 5,
				    EGL10.EGL_DEPTH_SIZE,   16,
				    EGL10.EGL_ALPHA_SIZE, 8,
		            EGL10.EGL_RENDERABLE_TYPE,  4,
		            EGL10.EGL_NONE
				},
				 {
				    EGL10.EGL_RED_SIZE, 5,
				    EGL10.EGL_BLUE_SIZE, 6,
				    EGL10.EGL_GREEN_SIZE, 5,
				    EGL10.EGL_DEPTH_SIZE,   16,
		            EGL10.EGL_RENDERABLE_TYPE,  4,
		            EGL10.EGL_NONE
				},	
				 {
				    EGL10.EGL_RED_SIZE, 5,
				    EGL10.EGL_BLUE_SIZE, 6,
				    EGL10.EGL_GREEN_SIZE, 5,
		            EGL10.EGL_RENDERABLE_TYPE,  4,
		            EGL10.EGL_NONE
				},	
				 {
		            EGL10.EGL_RENDERABLE_TYPE,  4,
		            EGL10.EGL_NONE
				},	
		 };
		 
		 
		 EGLConfig[] configs = new EGLConfig[1];
         int[] num_config = new int[1];
         
         int i;
         for(i = 0; i < configSpec.length; i++)
         {   if (eglc.eglChooseConfig(egldisplay, configSpec[i], configs, 1, num_config) && num_config[0] != 0) {
	             Log.i("Blender", "Using config #" + i);
	             break;
	         }
         }
         if(i == configSpec.length)
         {
        	 Log.e("Blender", "No config was found");
        	 return;
        	 
         }
         
         EGLConfig config = configs[0];
         
         try {
        	 int EGL_CONTEXT_CLIENT_VERSION=0x3098;
        	 int contextAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE };
        	 mEGLContext = eglc.eglCreateContext(egldisplay, config, EGL10.EGL_NO_CONTEXT, contextAttrs);
        	 if (mEGLContext == EGL10.EGL_NO_CONTEXT) {
                 Log.e("Blender", "eglCreateContext failed");
                 return ;
             }
             surface = eglc.eglCreateWindowSurface(egldisplay, config, this.getHolder(), null);

             eglc.eglMakeCurrent(egldisplay, surface, surface, mEGLContext);
         
         }
         catch(Exception e) {
             Log.v("Blender", "Error when initilizing the screen:\n " + e);
         }
				 
	}
	
	
}