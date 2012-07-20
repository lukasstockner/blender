
#ifndef __AEVENT_H__
#define __AEVENT_H__


#ifdef __cplusplus
extern "C" {
#endif

enum aEventType {
	ET_UNKNOWN = 0,
	ET_MOUSE,
	ET_KEY,
	ET_WINDOW,
	ET_WINDOWSIZE,
	ET_APP
} ;


typedef struct aEventBase
{
	int aeventype;
	int time;
} aEventBase;

typedef struct aEventMouse
{
	aEventBase eb;
	float coord[2];
	int mouseevent;
	char oops;

} aEventMouse;

typedef struct aEventKey
{
	aEventBase eb;
	int key;
	int keyevent;


} aEventKey;

enum aEventWindow_type {
	ET_WS_UNKNOWN = 0,
	ET_WS_FOCUS,
	ET_WS_DEFOCUS,
	ET_WS_UPDATE
} ;

typedef struct aEventWindow
{
	aEventBase eb;
	int type;


} aEventWindow;

typedef struct aEventWindowSize
{
	aEventBase eb;
	int pos[2];
	int size[2];


} aEventWindowSize;


enum aEventApp_action {
	ET_APP_UNKNOWN = 0,
	ET_APP_CLOSE
} ;
typedef struct aEventApp
{
	aEventBase eb;
	int action;
} aEventApp;

typedef union eEventAllTypes
{
	aEventBase eb;
	aEventMouse Mouse;
	aEventKey Key;
	aEventWindow Window;
	aEventWindowSize WindowSize;
	aEventApp app;

} eEventAllTypes;

size_t eEventGetSize(int type);

typedef struct aEventQueue
{
	char * begin;
	char * endsafe;
	char * readp;
	char * writep;


} aEventQueue;


void aEventQueueInit(aEventQueue * q, size_t size);

void aEventQueueAdd(aEventQueue * q, char * event, size_t size);

int aEventQueueCheck(aEventQueue * q);

int aEventGQueueCheck(void);

int aEventQueueRead(aEventQueue * q, char * event);

int aEventGQueueRead(eEventAllTypes * event);

void aEventQueuexFree(aEventQueue * q);

void aSwapBuffers(void);

#ifdef __cplusplus
}
#endif

#endif /* __AEVENT_H__ */
