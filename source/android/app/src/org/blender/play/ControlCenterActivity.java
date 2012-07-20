package org.blender.play;

import java.io.BufferedInputStream;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

import android.widget.*;
import android.view.*;
import android.view.View.*;

public class ControlCenterActivity extends Activity {

	private Button b_startgame;
	private Button b_selectgame;
	private TextView t_gamename;
	
	
	private String gamepath = null;
	
	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
	    super.onCreate(savedInstanceState);
	    
	    setContentView(R.layout.main);

	    
	    
	    b_startgame = (Button)findViewById(R.id.startbutton);
	    b_selectgame = (Button)findViewById(R.id.gameselecterbutton);
	    t_gamename = (TextView)findViewById(R.id.gamenametext);
	    
	    
	    b_startgame.setOnClickListener(new OnClickListener() {           
            @Override
            public void onClick(View v) {

            	Intent intent = new Intent(ControlCenterActivity.this, GhostActivity.class);
            	intent.setType("*/*");
            	intent.setData(Uri.parse("file://" + gamepath));
            	
            	startActivity(intent); 
            }
        });
	    
	    
	    
	    
	    b_selectgame.setOnClickListener(new OnClickListener() {           
            @Override
            public void onClick(View v) {

            	Intent intent = new Intent(Intent.ACTION_GET_CONTENT); 
            	intent.setType("*/*");
            	
            	intent.addCategory(Intent.CATEGORY_OPENABLE);
            try {	
            	startActivityForResult(Intent.createChooser(intent, "Select a File to Upload"), 0);
            	}
            catch (android.content.ActivityNotFoundException e)
            {
            	
            }
            
            }
        });
	    
	    updatePlayable();
	    
	    // TODO Auto-generated method stub
	}

	void updatePlayable()
	{
		if(gamepath != null)
			b_startgame.setEnabled(true);
		else
			b_startgame.setEnabled(false);
	}
	
	
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case 0:      
            if (resultCode == RESULT_OK) {  
                Uri uri = data.getData();
                String tpath = uri.getPath();
                String tname = tpath.substring(tpath.lastIndexOf("/")+1);		
                		
                if(tpath.substring(tpath.lastIndexOf(".")+1).toLowerCase().equals("blend"))
                {
                	gamepath = tpath;
                	t_gamename.setText(tname.substring(0, tname.lastIndexOf(".")));
                	updatePlayable();
                	
                }
                else
                {
                	Toast.makeText(getApplicationContext(), "File \"" + tname + "\" doesn't have extensio \".blend\".", Toast.LENGTH_LONG).show();
                
                }
            }           
            break;
        }
    super.onActivityResult(requestCode, resultCode, data);
    }
	
	void InstallFile(String installpath, String basepath)
	{
        try
        {
	        ZipInputStream zip = new ZipInputStream(
	        						new BufferedInputStream(
	        								new FileInputStream(installpath)));
	        try {
	        	byte[] buff = new byte[4096];
	        	
	            ZipEntry entry;
	            while ((entry = zip.getNextEntry()) != null) {
	                
	                String filename = entry.getName();
	                
	                if(filename != null)
	                {
	                	FileOutputStream outfile = new FileOutputStream(basepath + filename);
	                	try{	  
	                		Log.i("Blender", "Installing " + filename);
	                		
			                int count;
			                while ((count = zip.read(buff)) != -1) {
			                	outfile.write(buff, 0, count);
			                };
		                
	                	} finally {
	                		outfile.close();
	        	        }
	                }
	            }
	        } finally {
	        	zip.close();
	        }
        
        
        }
        catch (Exception e) 
        {
        	Log.w("Blender", "Error reading file: " + e);
        }
		
		
		
	}
    
	@Override
	public void onStart() {
		super.onStart();
        Uri intentdata = getIntent().getData();
        if(intentdata != null)
        {
        	String path =  intentdata.getEncodedPath();
        	if(path != null)
        	{
        		Log.i("rrrr","Started " + path);
        		
        	    String basedir = getBaseContext().getFilesDir().getPath();
        	    basedir = basedir.substring(0, basedir.lastIndexOf("/")+1);
        	    
        	    InstallFile(path, basedir);
        		
        	}
        	else Log.i("rrrr","No path");
        } else Log.i("rrrr","No data");
    	

        

	}
}
