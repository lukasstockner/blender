#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aEvent.h"

#include <assert.h>


#define SIZETOBOUND(a) ((a+3) & ~3)



size_t eEventGetSize(int type)
{
	switch(type)
	{
		case ET_MOUSE:	return sizeof(aEventMouse);
		case ET_KEY:	return sizeof(aEventKey);
		case ET_WINDOW:	return sizeof(aEventWindow);
		case ET_WINDOWSIZE:	return sizeof(aEventWindowSize);
		case ET_APP:	return sizeof(aEventApp);
		default: assert(0);

	}
}




void aEventQueueInit(aEventQueue * q, size_t size)
{
	int safebound = SIZETOBOUND(sizeof(eEventAllTypes));
	size = SIZETOBOUND(size);
	
	q->begin = (char*)malloc(size+safebound);
	q->endsafe = q->begin + size;

	q->readp = q->writep = q->begin;



}


static int sameSide(char * p1, char * p2, char * cp)
{
	if(p1 < cp && p2 < cp)
		return 1;
	if(p1 >= cp && p2 > cp)
		return 1;

	return 0;
}

void aEventQueueAdd(aEventQueue * q, char * event, size_t size)
{
	size_t boundedsize = SIZETOBOUND(size);

	if(q->writep >= q->endsafe)
		q->writep = q->begin;

	if(!sameSide(q->writep, q->writep+boundedsize, q->readp))
	{
		//printf("Event Dropped\n");
		return;
		assert(0);
	}

	memcpy(q->writep, event, size);

	
	q->writep+=boundedsize;


}

int aEventQueueCheck(aEventQueue * q)
{
	if(q->readp != q->writep)
		return 1;

	return 0;
}

int aEventQueueRead(aEventQueue * q, char * event)
{
	if(aEventQueueCheck(q))
	{
		aEventBase * base = (aEventBase *)q->readp;
		size_t size = eEventGetSize(base->aeventype);

		memcpy(event, base, size);

		size = SIZETOBOUND(size);

		q->readp += size;

		if(q->readp >= q->endsafe)
			q->readp = q->begin;

		return 1;
	} else
	{
		return 0;
	}



}

aEventQueue mq;


void aEventQueuexFree(aEventQueue * q)
{
	free(q->begin);


}


int aEventGQueueCheck(void)
{
	return aEventQueueCheck(&mq);
}

int aEventGQueueRead(eEventAllTypes * event)
{
	return aEventQueueRead(&mq, (char*) event);
}

