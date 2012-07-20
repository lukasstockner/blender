package org.blender.play;

import java.io.File;

import android.app.Activity;
import android.os.Bundle;
import android.util.*;

public class makesActivity extends Activity {

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
	}
	
	void MainFunc()
	{
		Bundle extras = getIntent().getExtras();
		if (extras == null) {
				Log.e("Blender makes", "No extras");	
				return;
		}
		
		
		String strexec = extras.getString("exec");
		String strpar1 = extras.getString("par1");
		String strpar2 = extras.getString("par2");
		
		if (strexec == null) {
			Log.e("Blender makes", "No exec string was passed");
		} 
		
		if (strpar1 == null) {
			Log.e("Blender makes", "No par1 string was passed. You need to have at least one parameter for makesdna/rna");
		} 
		
		if(strexec == null || strpar1 == null)
			return;
		
		   
		String strexecintern = strexec.substring(strexec.lastIndexOf("/")+1);
		
		Log.e("Blender makes", strexecintern);
		
		File fileexec = new File(strexec);
		
		if(!fileexec.exists())
		{
			Log.e("Blender makes","File " + strexec + " does not exis");
			return;
		}
		
		
		
		String BaseDir = getBaseContext().getFilesDir().getPath();// getPackageResourcePath().getFilesDir().getPath();
		BaseDir = BaseDir.substring(0, BaseDir.lastIndexOf("/")+1);
		
		if(1!=BlenderNativeAPI.FileCopyFromTo(strexec, BaseDir+strexecintern))
		{
			Log.e("Blender makes","Failed to copy "+ strexec +" to " + strexecintern);
			return;
		}
	   
		BlenderNativeAPI.ExecuteLib(BaseDir + strexecintern, strpar1, strpar2);
	}
	
	@Override
	public void onStart() {
		super.onStart();

		MainFunc();
		BlenderNativeAPI.exit(0);		
	}

}



