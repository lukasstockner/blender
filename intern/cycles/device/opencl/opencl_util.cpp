/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef WITH_OPENCL

#include "opencl.h"

CCL_NAMESPACE_BEGIN

cl_context OpenCLCache::get_context(cl_platform_id platform,
                                    cl_device_id device,
                                    thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);

	OpenCLCache& self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	pair<CacheMap::iterator,bool> ins = self.cache.insert(
		CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

	Slot &slot = ins.first->second;

	/* create slot lock only while holding cache lock */
	if(!slot.context_mutex)
		slot.context_mutex = new thread_mutex;

	/* need to unlock cache before locking slot, to allow store to complete */
	cache_lock.unlock();

	/* lock the slot */
	slot_locker = thread_scoped_lock(*slot.context_mutex);

	/* If the thing isn't cached */
	if(slot.context == NULL) {
		/* return with the caller's lock holder holding the slot lock */
		return NULL;
	}

	/* the item was already cached, release the slot lock */
	slot_locker.unlock();

	cl_int ciErr = clRetainContext(slot.context);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;

	return slot.context;
}

cl_program OpenCLCache::get_program(cl_platform_id platform,
                                    cl_device_id device,
                                    ustring key,
                                    thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);

	OpenCLCache& self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	pair<CacheMap::iterator,bool> ins = self.cache.insert(
		CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

	Slot &slot = ins.first->second;

	pair<Slot::EntryMap::iterator,bool> ins2 = slot.programs.insert(
		Slot::EntryMap::value_type(key, Slot::ProgramEntry()));

	Slot::ProgramEntry &entry = ins2.first->second;

	/* create slot lock only while holding cache lock */
	if(!entry.mutex)
		entry.mutex = new thread_mutex;

	/* need to unlock cache before locking slot, to allow store to complete */
	cache_lock.unlock();

	/* lock the slot */
	slot_locker = thread_scoped_lock(*entry.mutex);

	/* If the thing isn't cached */
	if(entry.program == NULL) {
		/* return with the caller's lock holder holding the slot lock */
		return NULL;
	}

	/* the item was already cached, release the slot lock */
	slot_locker.unlock();

	cl_int ciErr = clRetainProgram(entry.program);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;

	return entry.program;
}

void OpenCLCache::store_context(cl_platform_id platform,
                                cl_device_id device,
                                cl_context context,
                                thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);
	assert(device != NULL);
	assert(context != NULL);

	OpenCLCache &self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);
	CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
	cache_lock.unlock();

	Slot &slot = i->second;

	/* sanity check */
	assert(i != self.cache.end());
	assert(slot.context == NULL);

	slot.context = context;

	/* unlock the slot */
	slot_locker.unlock();

	/* increment reference count in OpenCL.
	 * The caller is going to release the object when done with it. */
	cl_int ciErr = clRetainContext(context);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;
}

void OpenCLCache::store_program(cl_platform_id platform,
                                cl_device_id device,
                                cl_program program,
                                ustring key,
                                thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);
	assert(device != NULL);
	assert(program != NULL);

	OpenCLCache &self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
	assert(i != self.cache.end());
	Slot &slot = i->second;

	Slot::EntryMap::iterator i2 = slot.programs.find(key);
	assert(i2 != slot.programs.end());
	Slot::ProgramEntry &entry = i2->second;

	assert(entry.program == NULL);

	cache_lock.unlock();

	entry.program = program;

	/* unlock the slot */
	slot_locker.unlock();

	/* Increment reference count in OpenCL.
	 * The caller is going to release the object when done with it.
	 */
	cl_int ciErr = clRetainProgram(program);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;
}

CCL_NAMESPACE_END

#endif
