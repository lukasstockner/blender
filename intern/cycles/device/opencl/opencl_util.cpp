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

/* lookup something in the cache. If this returns NULL, slot_locker
 * will be holding a lock for the cache. slot_locker should refer to a
 * default constructed thread_scoped_lock */
template<typename T>
T OpenCLCache::get_something(cl_platform_id platform,
                       cl_device_id device,
                       T Slot::*member,
                       thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);

	OpenCLCache& self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);

	pair<CacheMap::iterator,bool> ins = self.cache.insert(
		CacheMap::value_type(PlatformDevicePair(platform, device), Slot()));

	Slot &slot = ins.first->second;

	/* create slot lock only while holding cache lock */
	if(!slot.mutex)
		slot.mutex = new thread_mutex;

	/* need to unlock cache before locking slot, to allow store to complete */
	cache_lock.unlock();

	/* lock the slot */
	slot_locker = thread_scoped_lock(*slot.mutex);

	/* If the thing isn't cached */
	if(slot.*member == NULL) {
		/* return with the caller's lock holder holding the slot lock */
		return NULL;
	}

	/* the item was already cached, release the slot lock */
	slot_locker.unlock();

	return slot.*member;
}

template cl_context OpenCLCache::get_something<cl_context>(cl_platform_id platform,
                                                                  cl_device_id device,
                                                                  cl_context Slot::*member,
                                                                  thread_scoped_lock& slot_locker);
template cl_program OpenCLCache::get_something<cl_program>(cl_platform_id platform,
                                                                  cl_device_id device,
                                                                  cl_program Slot::*member,
                                                                  thread_scoped_lock& slot_locker);

/* store something in the cache. you MUST have tried to get the item before storing to it */
template<typename T>
void OpenCLCache::store_something(cl_platform_id platform,
                                         cl_device_id device,
                                         T thing,
                                         T Slot::*member,
                                         thread_scoped_lock& slot_locker)
{
	assert(platform != NULL);
	assert(device != NULL);
	assert(thing != NULL);

	OpenCLCache &self = global_instance();

	thread_scoped_lock cache_lock(self.cache_lock);
	CacheMap::iterator i = self.cache.find(PlatformDevicePair(platform, device));
	cache_lock.unlock();

	Slot &slot = i->second;

	/* sanity check */
	assert(i != self.cache.end());
	assert(slot.*member == NULL);

	slot.*member = thing;

	/* unlock the slot */
	slot_locker.unlock();
}

template void OpenCLCache::store_something<cl_context>(cl_platform_id platform,
                                                              cl_device_id device,
                                                              cl_context thing,
                                                              cl_context Slot::*member,
                                                              thread_scoped_lock& slot_locker);
template void OpenCLCache::store_something<cl_program>(cl_platform_id platform,
                                                              cl_device_id device,
                                                              cl_program thing,
                                                              cl_program Slot::*member,
                                                              thread_scoped_lock& slot_locker);

/* see get_something comment */
cl_context OpenCLCache::get_context(cl_platform_id platform,
                                           cl_device_id device,
                                           thread_scoped_lock& slot_locker)
{
	cl_context context = get_something<cl_context>(platform,
	                                               device,
	                                               &Slot::context,
	                                               slot_locker);

	if(!context)
		return NULL;

	/* caller is going to release it when done with it, so retain it */
	cl_int ciErr = clRetainContext(context);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;

	return context;
}

/* see get_something comment */
cl_program OpenCLCache::get_program(cl_platform_id platform,
                                           cl_device_id device,
                                           ProgramName program_name,
                                           thread_scoped_lock& slot_locker)
{
	cl_program program = NULL;

	switch(program_name) {
		case OCL_DEV_BASE_PROGRAM:
			/* Get program related to OpenCLDeviceBase */
			program = get_something<cl_program>(platform,
			                                    device,
			                                    &Slot::ocl_dev_base_program,
			                                    slot_locker);
			break;
		case OCL_DEV_MEGAKERNEL_PROGRAM:
			/* Get program related to megakernel */
			program = get_something<cl_program>(platform,
			                                    device,
			                                    &Slot::ocl_dev_megakernel_program,
			                                    slot_locker);
			break;
	default:
		assert(!"Invalid program name");
	}

	if(!program)
		return NULL;

	/* caller is going to release it when done with it, so retain it */
	cl_int ciErr = clRetainProgram(program);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;

	return program;
}

/* see store_something comment */
void OpenCLCache::store_context(cl_platform_id platform,
                                       cl_device_id device,
                                       cl_context context,
                                       thread_scoped_lock& slot_locker)
{
	store_something<cl_context>(platform,
	                            device,
	                            context,
	                            &Slot::context,
	                            slot_locker);

	/* increment reference count in OpenCL.
	 * The caller is going to release the object when done with it. */
	cl_int ciErr = clRetainContext(context);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;
}

/* see store_something comment */
void OpenCLCache::store_program(cl_platform_id platform,
                                       cl_device_id device,
                                       cl_program program,
                                       ProgramName program_name,
                                       thread_scoped_lock& slot_locker)
{
	switch(program_name) {
		case OCL_DEV_BASE_PROGRAM:
			store_something<cl_program>(platform,
			                            device,
			                            program,
			                            &Slot::ocl_dev_base_program,
			                            slot_locker);
			break;
		case OCL_DEV_MEGAKERNEL_PROGRAM:
			store_something<cl_program>(platform,
			                            device,
			                            program,
			                            &Slot::ocl_dev_megakernel_program,
			                            slot_locker);
			break;
		default:
			assert(!"Invalid program name\n");
			return;
	}

	/* Increment reference count in OpenCL.
	 * The caller is going to release the object when done with it.
	 */
	cl_int ciErr = clRetainProgram(program);
	assert(ciErr == CL_SUCCESS);
	(void)ciErr;
}

/* Discard all cached contexts and programs.  */
void OpenCLCache::flush()
{
	OpenCLCache &self = global_instance();
	thread_scoped_lock cache_lock(self.cache_lock);

	foreach(CacheMap::value_type &item, self.cache) {
		if(item.second.ocl_dev_base_program != NULL)
			clReleaseProgram(item.second.ocl_dev_base_program);
		if(item.second.ocl_dev_megakernel_program != NULL)
			clReleaseProgram(item.second.ocl_dev_megakernel_program);
		if(item.second.context != NULL)
			clReleaseContext(item.second.context);
	}

	self.cache.clear();
}

CCL_NAMESPACE_END

#endif
