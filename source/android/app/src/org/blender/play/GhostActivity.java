package org.blender.play;



import java.util.*;

import android.app.Activity;
import android.os.Bundle;
import android.view.*;
import android.content.*;
import android.hardware.*;
import android.util.*;
import android.net.*;
import javax.microedition.khronos.egl.*;





public class GhostActivity extends Activity implements SensorEventListener
{
    /** Called when the activity is first created. */
	private GhostSurface mainSurface;
	
	private SensorManager sensors;
	private Sensor accelerometer;
	private Sensor gyroscope;
	private Sensor magneticfield;
	
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        
        
        
        Uri data = getIntent().getData();
        if(data != null || true){
      
        Log.i("Blender","Started");
        mainSurface = new GhostSurface(data != null ? data.getPath() : "/sdcard/test.blend", getApplication());
        setContentView(mainSurface);
        SurfaceHolder holder = mainSurface.getHolder();
        Log.v("Blender", "Surface valid: " + Boolean.toString(holder.getSurface().isValid()));
       // holder.setFixedSize(240, 400);
        
        //mainSurface.initSurface();
     //setContentView(R.layout.main);
        
        sensors = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        
        accelerometer = sensors.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        gyroscope = sensors.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        magneticfield = sensors.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD);
        
        BlenderNativeAPI.SetASystem(this);
        //sensors.registerListener(this, accelerometer, SensorManager.SENSOR_DELAY_GAME);
        
        //sensors.registerListener(this, gyroscope, SensorManager.SENSOR_DELAY_GAME);        
        
        
        }
	    
    }
    
    public int export_getSensorsAvailability(int type)
    {
    	switch(type)
    	{
    		case 1:	return accelerometer == null ? 0 : 1;
    		case 2:	return gyroscope == null ? 0 : 1;
    		case 3: return magneticfield == null ? 1 : 0;
    	}
    	return 0;  	
    }
    
    public int export_setSensorsState(int type, int enable)
    {
    	Sensor cs = null;
    	switch(type)
    	{
    		case 1:	cs = accelerometer ; break;
    		case 2:	cs =  gyroscope; break;
    		case 3: cs =  magneticfield; break;
    		default: return 0;
    	}
    	
    	if(cs==null)
    		return 0;
    	
    	if(enable != 0)
    	{
    		return sensors.registerListener(this, cs, SensorManager.SENSOR_DELAY_GAME) ? 1 : 0;
    		
    	} else
    	{
    		sensors.unregisterListener(this, cs);
    		return 1;
    		
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
    	BlenderNativeAPI.exit(0);
    }
        
    

    
    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) 
    {

    }
    
    @Override
    public void onSensorChanged(SensorEvent event) 
    {
    	switch(event.sensor.getType())
    	{
    		case Sensor.TYPE_ACCELEROMETER:
    			BlenderNativeAPI.eventSensor3D(0, event.values[0], event.values[1], event.values[2]);
    			break;
    		case Sensor.TYPE_GYROSCOPE:
    			BlenderNativeAPI.eventSensor3D(1, event.values[0], event.values[1], event.values[2]);
    			break;	
    		case Sensor.TYPE_MAGNETIC_FIELD:
    			BlenderNativeAPI.eventSensor3D(2, event.values[0], event.values[1], event.values[2]);
    			break;      	
    	
    	}
    	
    }
}



class GhostSurface extends SurfaceView implements SurfaceHolder.Callback{

	private EGLDisplay egldisplay = null;
	private EGLSurface surface = null;
	private EGL10 eglc;
	private EGLContext mEGLContext;
	
	private String filepath;
	
	long lasttime;
	
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
		
		lasttime = Calendar.getInstance().getTimeInMillis();
		
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
    	
    	export_getWindowSize();
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
    	long ctime = Calendar.getInstance().getTimeInMillis();
    	
    	
    	Log.i("Blender", "FPS " + String.format("%.2f", 1000.0/(ctime-lasttime)) + " Time: "+(ctime-lasttime));
    	lasttime = ctime;
        eglc = (EGL10)EGLContext.getEGL();
        
        eglc.eglWaitNative(EGL10.EGL_CORE_NATIVE_ENGINE, null);
        eglc.eglWaitGL();
        if(!eglc.eglSwapBuffers(egldisplay, surface)) 
        {
        	Log.v("Blender", "SwappBuffers Error:" + eglc.eglGetError());
        }

    }
    
    public int export_getWindowSize()
    {
    	return (this.getWidth() << 16) | this.getHeight();
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