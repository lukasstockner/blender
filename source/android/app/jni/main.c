/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

//BEGIN_INCLUDE(all)
#include <jni.h>
#include <errno.h>

#include <sys/stat.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <android/log.h>


//#include "linkgl.h"

#include <stdio.h>
#include <stdlib.h>

#include <dlfcn.h>

#include <dirent.h>

//#include <jconfig.h>
//#include <jpeglib.h>

//#include <ft2build.h>
//#include FT_FREETYPE_H

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "blender", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "blender", __VA_ARGS__))
//#ifdef DEBUG
#define printd(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, "blender", __VA_ARGS__))
//#else
//#define printd(...)
//#endif
#define printf(...) ((void)__android_log_print(ANDROID_LOG_INFO, "printf", __VA_ARGS__))

#include <pthread.h>


extern int foo(void);

#include "aEvent.h"

#include <unistd.h>
#include <assert.h>

typedef int bool;
#define false (0)
#define true (1)

aEventQueue mq;

void * ReadAEventData( void * parp )
{
	bool anyProcessed = false;
	while(1)
	{
		do{
			eEventAllTypes event;

			if(!aEventQueueCheck(&mq))
			{
				usleep(2*1000);
			}

			while(aEventQueueRead(&mq, (char*)&event))
			{
				switch (event.eb.aeventype)
				{

				case ET_KEY:
					{
						printf("Key Press %c", event.Key.key);
						break;
					}
				case ET_MOUSE:
					{

						printf("Mouse: %f %f", event.Mouse.coord[0], event.Mouse.coord[1]);
						break;
					}

				default:
					assert(0);


				}

				anyProcessed = true;
			}



		} while(anyProcessed);

		usleep(2000*1000);
	}

}






int copyfile(const char * src, const char * dst)
{
    FILE * fsrc = fopen(src, "rb");
    FILE * fdst;
    int r =1;
    
    printd("%s to  %s",src,dst);
    if(fsrc)
    {
	fdst = fopen(dst, "wb");

	if(fdst){
	    char buff[1024];
	    size_t bread;

	    while(!feof(fsrc))
	    {
		bread = fread(buff,1,1024,fsrc);
		fwrite(buff,1,bread,fdst);
	    }

	    fclose(fsrc);
    } else r=0;

	fclose(fsrc);
    } else r=0;

    printd("%s %s %i",src,dst,r);


    return r;
}
const char * libloaded = "/data/data/org.blender.app/lib";
const char * libstatic = "/data/data/org.blender.app/lib/lib";
const char * libsdcard = "/sdcard/nucleusbridge/lib";
const char * libloadernew= "/data/data/org.blender.play/lib";


const char * libloadeddir = "/data/data/org.blender.app/";
const char * libsdcarddir = "/sdcard/nucleusbridge/lib/";

static char * getpathlib(const char * base, const char * suffix, int version, int mustexist)
{
    size_t basesize = strlen(base);
    size_t suffixsize = strlen(suffix);
    char * path;
    struct stat fst;

    if(version>=100 || version<0)
        version = 0;
    path = malloc(basesize+suffixsize+4+(version?3:0));
    memcpy(path, base, basesize);
    memcpy(path+basesize, suffix, suffixsize);
    memcpy(path+basesize+suffixsize, ".so", 4);
    if(version)
    {
        path[basesize+suffixsize+3]='.';
        sprintf(path+basesize+suffixsize+4, "%d", version);
    }

    printd(path);
    if(mustexist && stat(path,&fst)==-1)
    {
        free(path);
        path = NULL;

    }

    return path;
}

static char * mergepath(const char * base, const char * suffix)
{
    size_t basesize = strlen(base);
    size_t suffixsize = strlen(suffix);
    
    char * path = malloc(basesize+suffixsize+2);
    
    memcpy(path, base, basesize);
    memcpy(path+basesize, suffix, suffixsize+1);
  return path;
}




static void loadallfiles(const char * dirfrom, const char *dirto)
{
  
  DIR * dir;
  struct dirent *fileobj;
  if(!(dir = opendir(dirfrom)))
  {
   printf("No %s folder\n", dirfrom);
   return;
  }
  
  while((fileobj=readdir(dir)))
  {
    char *filefrom, *fileto;
    struct stat filestat;
    if(fileobj->d_name[0] == '.')
      continue;
    
    filefrom = mergepath(dirfrom, fileobj->d_name);
    fileto = mergepath(dirto, fileobj->d_name);
    copyfile(filefrom, fileto);
    printf("%s -> %s\n", filefrom, fileto);
    
    remove(filefrom);
    
    free(filefrom);
    free(fileto);
    
  }
  
  
  
  closedir(dir);
}


void * loadownlib(const char * libname, int version)
{

    char * libpath;
    void * libh = 0;

    printd("Loading lib: %s",libname);



	if(libpath=getpathlib(libloadernew, libname, version, 1))
	{
		libh = dlopen(libpath,RTLD_GLOBAL);
	} else
    if(libpath=getpathlib(libstatic, libname, version, 1))
    {
        libh = dlopen(libpath,RTLD_GLOBAL);
    } else
    if(libpath=getpathlib(libloaded, libname, version, 1))
    {
        libh = dlopen(libpath,RTLD_GLOBAL);
    }


    if(libh==0)
    {
        const char * errstr;
        printd("Couldn't find library %s",libname);
        if(libpath)
        {
            errstr = dlerror();
            printd(errstr?errstr:"Unidentified Error");

        }

    } else
    {

        printd("Loaded libary's path: %s", libpath);
    }


    if(libpath)
        free(libpath);

    return libh;

}

void * loadownlibexact(const char * libpath)
{

	void * libh = 0;

	printd("Loading lib: %s",libpath);



	if(libpath)
	{
		libh = dlopen(libpath,RTLD_GLOBAL);
	}



	if(libh==0)
	{
		const char * errstr;
		printd("Couldn't find library %s",libpath);

			errstr = dlerror();
			printd(errstr?errstr:"Unidentified Error");

	}


	return libh;

}


int pushlibrary(const char * libname)
{
    char * srclib;
    char * dstlib;
    int r ;

    printd("pushing3");

    if(!(srclib = getpathlib(libsdcard, libname, 0, 1)))
    {
        printd("failed");
        return 0;
    }

    dstlib = getpathlib(libloaded, libname, 0, 0);

    r = copyfile(srclib, dstlib);

    free(srclib);
    free(dstlib);


    return r;
}


int startlibmainfunc(void * libh, int argc, char *argv[])
{
	int (*mainfunc)(int, char**);
//cccccchhhhange
    if(mainfunc = dlsym(libh, "main"))
    {
        printf("%i\n", mainfunc(argc,argv));
        return 1;
    }
    exit(0);
    return 0;
}

struct JavaBox {
	JavaVM *jvm;

	jclass jcGhostSurface;
	jmethodID midSwapBuffers;
jmethodID midinitSurface;

	jobject mainwin; /* should be stored */
} jb;


JNIEXPORT jint JNICALL Java_org_blender_play_BlenderNativeAPI_FileCopyFromTo (JNIEnv * env, jclass class, jstring jfrom, jstring jto)
{
	const char * from = (*env)->GetStringUTFChars(env, jfrom, 0);
	const char * to = (*env)->GetStringUTFChars(env, jto, 0);

	jint r = copyfile(from, to);


	(*env)->ReleaseStringUTFChars(env, jfrom, from);
	(*env)->ReleaseStringUTFChars(env, jto, to);

	return r;

}

JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_ExecuteLib(JNIEnv * env, jclass class, jstring jexec, jstring jparam1, jstring jparam2)
{
	const char * exec =		(*env)->GetStringUTFChars(env, jexec, 0);
	const char * param1 =	(*env)->GetStringUTFChars(env, jparam1, 0);
	const char * param2 = jparam2 ? (*env)->GetStringUTFChars(env, jparam2, 0) : NULL;
	const char *cmd[3] = {exec, param1, param2};


	LOGW("jparam2 %p", jparam2);

	void * lh = loadownlibexact(exec);
	if(lh)
	{
		LOGW("cmd %s %s %s", cmd[0], cmd[1], cmd[2]?cmd[2]:"");
		startlibmainfunc(lh, param2 ? 3 : 2,cmd);
	}

	if(param2)
		(*env)->ReleaseStringUTFChars(env, jparam2, param2);
	(*env)->ReleaseStringUTFChars(env, jparam1, param1);
	(*env)->ReleaseStringUTFChars(env, jexec, exec);


}

JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_eventTouch(JNIEnv * env, jclass class, jint type, jfloat x, jfloat y)
{
	aEventMouse mouse;
	LOGW("%i: (%f,\t%f)\n ", (int) type, (float) x, (float) y);

	if(type != 2)
	{
	  mouse.eb.aeventype = ET_MOUSE;

	  mouse.coord[0] = (float)x;
	  mouse.coord[1] = (float)y;

	  mouse.mouseevent = 2;

	  aEventQueueAdd(&mq, (char*)&mouse, sizeof(mouse));	  
	  
	}
	
	mouse.eb.aeventype = ET_MOUSE;

	mouse.coord[0] = (float)x;
	mouse.coord[1] = (float)y;

	mouse.mouseevent = type;

	aEventQueueAdd(&mq, (char*)&mouse, sizeof(mouse));



}



JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_eventWindowsUpdate(JNIEnv * env, jclass class)
{
	aEventWindow win;
	LOGW("Redraw Win");

	win.eb.aeventype = ET_WINDOW;

	win.type = ET_WS_UPDATE;

	aEventQueueAdd(&mq, (char*)&win, sizeof(win));

}

JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_eventWindowsFocus(JNIEnv * env, jclass class)
{
	aEventWindow win;
	LOGW("Focus Win");

	win.eb.aeventype = ET_WINDOW;

	win.type = ET_WS_FOCUS;

	aEventQueueAdd(&mq, (char*)&win, sizeof(win));

}




JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_eventWindowsDefocus(JNIEnv * env, jclass class)
{
	aEventWindow win;
	LOGW("Deocus Win");

	win.eb.aeventype = ET_WINDOW;

	win.type = ET_WS_DEFOCUS;

	aEventQueueAdd(&mq, (char*)&win, sizeof(win));

}

JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_eventWindowsResize(JNIEnv * env, jclass class, jint x, jint y)
{
	aEventWindowSize win;
	LOGW("Resize Win");

	win.eb.aeventype = ET_WINDOWSIZE;

	win.pos[0] = 0;
	win.pos[1] = 0;

	win.size[0] = x;
	win.size[1] = y;

	LOGW("Resize Win %ix%i", win.size[0], win.size[1]);

	aEventQueueAdd(&mq, (char*)&win, sizeof(win));

}


JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_actionClose(JNIEnv * env, jclass class)
{
	aEventApp app;

	LOGW("Close");

	app.eb.aeventype = ET_APP;

	app.action = ET_APP_CLOSE;

	aEventQueueAdd(&mq, (char*)&app, sizeof(app));


}

JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_SetScreen
  (JNIEnv * env, jclass class, jobject win)
  {


	//LOGW("Hiopea* ");
 jb.mainwin = (*env)->NewGlobalRef(env, win);
// LOGW("Hiopea2 ");
  jb.jcGhostSurface = (*env)->NewGlobalRef(env, (*env)->GetObjectClass(env, jb.mainwin));
 // LOGW("Hiopea3 ");
  jb.midSwapBuffers = (*env)->GetMethodID(env, jb.jcGhostSurface, "SwapBuffers", "()V");//(*env)->NewGlobalRef(env, (*env)->GetMethodID(env, jb.jcGhostSurface, "SwapBuffers", "()V"));
  //LOGW("Hiopea4 ");
  LOGW("cl %i %i", (int)jb.midSwapBuffers, (int)jb.jcGhostSurface);
  //glClearColor(0.5f, 0.0f, 0.5f, 1.0f);
  //glClear(GL_COLOR_BUFFER_BIT);
  }
  
JNIEnv* jGetEnv(void)
{
	JNIEnv* env;
	jint r = (*jb.jvm)->GetEnv(jb.jvm, (void**)&env, JNI_VERSION_1_2);
	if (r != JNI_OK)
	{
		if(r == JNI_EDETACHED)
			LOGW("Detached Thread");
		else if(r == JNI_EVERSION)
			LOGW("Not supported");
		}
	
	
	return env;
}
  

void aSwapBuffers(void)
{
	JNIEnv* env = jGetEnv();
LOGW("Start Swap ");
	(*env)->CallVoidMethod(env, jb.mainwin, jb.midSwapBuffers);
	LOGW("Swapped");
}
  
JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_Swap(JNIEnv * env, jclass class)
{
	aSwapBuffers();

}

void * StartBlender( void * parp )
{
	char * copyfilepath = parp;
	JNIEnv* env = NULL;
	(*jb.jvm)->AttachCurrentThread(jb.jvm, (void**) &env, NULL);
	LOGW("One moment");
	{

//env = jGetEnv();

LOGW("env %p",*env);

jb.midinitSurface = (*env)->GetMethodID(env, jb.jcGhostSurface, "initSurface", "()V");//(*env)->NewGlobalRef(env, (*env)->GetMethodID(env, jb.jcGhostSurface, "initSurface", "()V"));

LOGW("midinitSurface2 %p",jb.midinitSurface);

(*env)->CallVoidMethod(env, jb.mainwin, jb.midinitSurface);
	}

	setenv("PYTHONPATH","/mnt/sdcard/com.googlecode.python3forandroid/extras/python3:/data/data/com.googlecode.python3forandroid/files/python3/lib/python3.2/lib-dynload",1);
	setenv("PYTHONHOME","/data/data/com.googlecode.python3forandroid/files/python3",1);

	loadownlib("jpeg", 8);
	loadownlib("freetype", 6);
	loadownlib("z", 1);
	loadownlib("png15", 15);
	loadownlib("GLarm", 0);
	loadownlib("GLUarm", 0);
	//loadownlib("SDL", 0);


		if(!dlopen("/data/data/org.blender.play/libpython3.2m.so.1.0",RTLD_GLOBAL))
		{
			const char *errstr = dlerror();
			printd(errstr?errstr:"Unidentified Error");

		}
LOGW(copyfilepath);
	if(1)
	{
	int i;
	void *blendlibh;
	//char *cmd[] = {"blender","-h"};
	char *cmd[] = {"blenderplayer" , copyfilepath};//,  "/sdcard/test.blend", "-o", "//render_",  "-F" ,"JPEG", "-x" ,"1" ,"-f" ,"1"};//"-b",



	blendlibh = loadownlib("blenderplayer",0);


	startlibmainfunc(blendlibh,2,cmd);//11
	free(copyfilepath);





	}
	//exit(0);


}

JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_exit(JNIEnv * env, jclass class, jint exitcode)
{
	//exit(exitcode);
	kill(getpid(), SIGKILL);
}


JNIEXPORT void JNICALL Java_org_blender_play_BlenderNativeAPI_StartBlender(JNIEnv * env, jclass class, jstring jfilepath)
{
	const char * filepath = (*env)->GetStringUTFChars(env, jfilepath, 0);
	char * copyfilepath = strdup(filepath);
	(*env)->ReleaseStringUTFChars(env, jfilepath, filepath);


	pthread_t thread;
	pthread_create(&thread, 0, StartBlender, copyfilepath);

}


jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
	pthread_t thread;
	LOGW("loaded");
	jb.jvm = vm;


	aEventQueueInit(&mq, 1024*4);
	//pthread_create(&thread, 0, ReadAEventData, 0);

	freopen("/sdcard/out.txt","w",stdout);
	freopen("/sdcard/err.txt","w",stderr);



return JNI_VERSION_1_2;
}

//END_INCLUDE(all)
