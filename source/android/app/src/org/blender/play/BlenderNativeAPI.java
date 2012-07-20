package org.blender.play;

class BlenderNativeAPI
{
	static
    {
      
		
		System.loadLibrary("main");
    }
	
	
	public static native void SetScreen(GhostSurface win);
	public static native void Swap();
	public static native void StartBlender(String filepath);
	
	public static native void eventWindowsUpdate();
	public static native void eventWindowsResize(int x, int y);
	public static native void eventWindowsFocus();
	public static native void eventWindowsDefocus();
	
	public static native void actionClose();
	
	public static native void eventTouch(int type, float x, float y);	
	
	public static native int FileCopyFromTo(String from, String to);
	
	public static native void ExecuteLib(String execpath, String param1, String param2);
	public static native void exit(int exitcode);
	
	
}