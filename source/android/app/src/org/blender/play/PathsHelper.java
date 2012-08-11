package org.blender.play;

import java.io.*;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.content.Context;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.*;
import android.util.Log;

public class PathsHelper {

	static String BaseAppDir;
	static {
		BaseAppDir = null;
		
		
	}
	
	
	static public String getBaseAppDir(Context context)
	{
		if(BaseAppDir == null)
		{
			BaseAppDir = context.getFilesDir().getPath();
			BaseAppDir = BaseAppDir.substring(0, BaseAppDir.lastIndexOf("/")+1);
			
			
		}
		
		return BaseAppDir;
		
	}
	
	static public String getFileName(String path)
	{
		
		
		return path.substring(path.lastIndexOf("/")+1);
	}
	
	public static void InstallFile(InputStream inputStream, String basepath)
	{
        try
        {
	        ZipInputStream zip = new ZipInputStream(
	        						new BufferedInputStream(
	        								inputStream));
	        try {
	        	byte[] buff = new byte[4096];
	        	
	            ZipEntry entry;
	            while ((entry = zip.getNextEntry()) != null) {
	                
	                String filename = entry.getName();
	                
	                if(filename != null)
	                {
	                	if(entry.isDirectory())
	                	{
	                		if(!new File(basepath + filename).mkdir())
	                			Log.e("Blender", "Cannot create "+filename+" dir.");
	                		
	                		
	                	} else
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
	
	
	
	
	
	
	static public boolean installIfNeeded(Context context)
	{
		try {
			PackageInfo pack = context.getPackageManager().getPackageInfo(context.getApplicationContext().getPackageName(), 0);
			String apkver =pack.versionName;
			
			String curver = "0.0";
			
			try{
				FileInputStream fin = new FileInputStream(getBaseAppDir(context)+"version.txt");
				BufferedInputStream bin = new BufferedInputStream(fin);
				DataInputStream din = new DataInputStream(bin);
				
				if(din.available()>0)
				{
					curver = din.readLine();
				}
				
				din.close();
				bin.close();
				fin.close();
				
				
			} catch (Exception e){//Catch exception if any
				  
			}
		
		
			if(!curver.equals(apkver))
			{
				try {
					InstallFile(context.getAssets().open("internalfiles.zip"), getBaseAppDir(context));
				} catch (IOException e1) {

				}
				
				
			 try{
					FileWriter fout = new FileWriter(getBaseAppDir(context)+"version.txt");
					BufferedWriter out = new BufferedWriter(fout);
					out.write(apkver);
					out.close();
				}catch (Exception e)
				{


				}
				
				return true;
			}
			

			
			
		
		} catch (NameNotFoundException e) {
			
		}
		
		return false;	
	}
	
}
