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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __DEPSGRAPH_UTIL_THREAD_H__
#define __DEPSGRAPH_UTIL_THREAD_H__

#include "BLI_threads.h"

class Thread {
public:
	typedef void (*run_cb_t)(int id);
	
	Thread(run_cb_t run_cb_, int id_)
	{
		id = id_;
		run_cb = run_cb_;
		joined = false;
		pthread_create(&pthread_id, NULL, run, (void*)this);
	}

	~Thread()
	{
		if(!joined)
			join();
	}

	static void *run(void *arg)
	{
		Thread *t = (Thread *)arg;
		t->run_cb(t->id);
		return NULL;
	}

	bool join()
	{
		joined = true;
		return pthread_join(pthread_id, NULL) == 0;
	}

protected:
	pthread_t pthread_id;
	int id;
	run_cb_t run_cb;
	bool joined;
};

#endif /* __DEPSGRAPH_UTIL_THREAD_H__ */
