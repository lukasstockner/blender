package org.blender.play;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

public class CopyIntern extends Activity {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
	}
	
	/* So we can terminate nicely at the end */
	void MainFunc()
	{
	    Bundle extras = getIntent().getExtras();
	    if (extras == null) {
	    		Log.e("Blender copy", "No extras.");
	    	
	    		return;
	    }
	    
	    
	    String from = extras.getString("from");
	    String to = extras.getString("to");
	    
	    if(from == null)
	    {
	    	Log.e("Blender copy", "No \"from\" parameter was found."); 	
	    }
	    if(to == null)
	    {
	    	Log.e("Blender copy", "No \"to\" parameter was found."); 	
	    }
	    
	    if(from == null || to == null)
	    {
	    	return;
	    }
	    
	    /* there should be a better way of getting default dir*/
	    
	    String basedir = getBaseContext().getFilesDir().getPath();
	    basedir = basedir.substring(0, basedir.lastIndexOf("/")+1);
	    
	    BlenderNativeAPI.FileCopyFromTo(from, basedir + to);
	    
	}
	
	@Override
	public void onStart() {
		super.onStart();
		MainFunc();

		BlenderNativeAPI.exit(0);
	    
	}

}
