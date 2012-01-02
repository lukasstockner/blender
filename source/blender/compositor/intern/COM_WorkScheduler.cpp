/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include <list>
#include "COM_WorkScheduler.h"
#include "PIL_time.h"
#include "BLI_threads.h"
#include "COM_CPUDevice.h"
#include "COM_OpenCLDevice.h"
#include "OCL_opencl.h"
#include "stdio.h"
#include "COM_OpenCLKernels.cl.cpp"

#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
#elif COM_CURRENT_THREADING_MODEL == COM_TM_NOTHREAD
#elif COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
#elif COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
#else
#error WorkScheduler: No threading model configured
#endif


/// @brief mutex for cpuwork
static ThreadMutex cpumutex;
/// @brief mutem for gpuwork
static ThreadMutex gpumutex;
/// @brief global state of the WorkScheduler.
static WorkSchedulerState state;
/// @brief work that are scheduled for a CPUDevice. the work has not been picked up by a CPUDevice
static list<WorkPackage*> cpuwork;
/// @brief work that are scheduled for a OpenCLDevice. the work has not been picked up by a OpenCLDevice
static list<WorkPackage*> gpuwork;
/// @brief list of all CPUDevices. for every hardware thread an instance of CPUDevice is created
static vector<CPUDevice*> cpudevices;
/// @brief list of all OpenCLDevices. for every OpenCL GPU device an instance of OpenCLDevice is created
static vector<OpenCLDevice*> gpudevices;
static bool openclActive;
#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
/// @brief list of all thread for every CPUDevice in cpudevices a thread exists
static ListBase cputhreads;
/// @brief list of all thread for every OpenCLDevice in gpudevices a thread exists
static ListBase gputhreads;
#endif

#if COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
ThreadedWorker *cpuworker;
#endif

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
/// @brief list of all thread for every CPUDevice in cpudevices a thread exists
static ListBase cputhreads;
static ThreadQueue * cpuqueue;
#endif

#if COM_OPENCL_ENABLED
static cl_context context;
static cl_program program;
#endif

#if COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
void* worker_execute_cpu(void* data) {
	CPUDevice device;
	WorkPackage * package = (WorkPackage*)data;
	device.execute(package);
	delete package;
	return NULL;
}
#endif
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
void* WorkScheduler::thread_execute_cpu(void* data) {
	bool continueLoop = true;
	Device* device = (Device*)data;
	while (continueLoop) {
		WorkPackage* work = (WorkPackage*)BLI_thread_queue_pop(cpuqueue);
		if (work) {
		   device->execute(work);
		   delete work;
		}
		PIL_sleep_ms(10);

		if (WorkScheduler::isStopping()) {
			continueLoop = false;
		}
	}
	return NULL;
}

bool WorkScheduler::isStopping() {return state == COM_WSS_STOPPING;}

#endif
#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
void* WorkScheduler::thread_execute_cpu(void* data) {
	bool continueLoop = true;
	Device* device = (Device*)data;
	while (continueLoop) {
		WorkPackage* work = WorkScheduler::getCPUWork();
		if (work) {
		   device->execute(work);
		   delete work;
		}
		PIL_sleep_ms(100);

		if (WorkScheduler::isStopping()) {
			continueLoop = false;
		}
	}
	return NULL;
}

void* WorkScheduler::thread_execute_gpu(void* data) {
	bool continueLoop = true;
	Device* device = (Device*)data;
	while (continueLoop) {
		WorkPackage* work = WorkScheduler::getGPUWork();
		if (work) {
		   device->execute(work);
		   delete work;
		}
		PIL_sleep_ms(100);

		if (WorkScheduler::isStopping()) {
			continueLoop = false;
		}
	}
	return NULL;
}

bool WorkScheduler::isStopping() {return state == COM_WSS_STOPPING;}

WorkPackage* WorkScheduler::getCPUWork() {
	WorkPackage* result = NULL;
	BLI_mutex_lock(&cpumutex);

	if (cpuwork.size()>0) {
	   result = cpuwork.front();
	   cpuwork.pop_front();
	}
	BLI_mutex_unlock(&cpumutex);
	return result;
}

WorkPackage* WorkScheduler::getGPUWork() {
	WorkPackage* result = NULL;
	BLI_mutex_lock(&gpumutex);

	if (gpuwork.size()>0) {
	   result = gpuwork.front();
	   gpuwork.pop_front();
	}
	BLI_mutex_unlock(&gpumutex);
	return result;
}

#endif


void WorkScheduler::schedule(ExecutionGroup *group, int chunkNumber) {
	WorkPackage* package = new WorkPackage(group, chunkNumber);
#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
	if (group->isOpenCL() && openclActive){
		BLI_mutex_lock(&gpumutex);
		gpuwork.push_back(package);
		BLI_mutex_unlock(&gpumutex);
	} else{
		BLI_mutex_lock(&cpumutex);
		cpuwork.push_back(package);
		BLI_mutex_unlock(&cpumutex);
	}
#elif COM_CURRENT_THREADING_MODEL == COM_TM_NOTHREAD
	CPUDevice device;
	device.execute(package);
	delete package;
#elif COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
	BLI_insert_work(cpuworker, package);
#elif COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	BLI_thread_queue_push(cpuqueue, package);
#endif
}

void WorkScheduler::start(CompositorContext &context) {
#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
	unsigned int index;
	cpuwork.clear();
	BLI_init_threads(&cputhreads, thread_execute_cpu, cpudevices.size());
	for (index = 0 ; index < cpudevices.size() ; index ++) {
		Device* device = cpudevices[index];
		BLI_insert_thread(&cputhreads, device);
	}
	if (context.getHasActiveOpenCLDevices()) {
		gpuwork.clear();
		BLI_init_threads(&gputhreads, thread_execute_gpu, gpudevices.size());
		for (index = 0 ; index < gpudevices.size() ; index ++) {
			Device* device = gpudevices[index];
			BLI_insert_thread(&gputhreads, device);
		}
		openclActive = true;
	} else {
		openclActive = false;
	}
#endif
#if COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
	cpuworker = BLI_create_worker(worker_execute_cpu, cpudevices.size(), 0);
#endif
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	unsigned int index;
	cpuqueue = BLI_thread_queue_init();
	BLI_thread_queue_nowait(cpuqueue);
	BLI_init_threads(&cputhreads, thread_execute_cpu, cpudevices.size());
	for (index = 0 ; index < cpudevices.size() ; index ++) {
		Device* device = cpudevices[index];
		BLI_insert_thread(&cputhreads, device);
	}
#endif
	
	state = COM_WSS_STARTED;
}

void WorkScheduler::stop() {
	state = COM_WSS_STOPPING;
#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
	BLI_mutex_lock(&cpumutex);
	while (cpuwork.size()>0) {
	   WorkPackage * result = cpuwork.front();
	   cpuwork.pop_front();
	   delete result;
	}
	BLI_mutex_unlock(&cpumutex);
	BLI_end_threads(&cputhreads);

	BLI_mutex_lock(&gpumutex);
	while (gpuwork.size()>0) {
	   WorkPackage * result = gpuwork.front();
	   gpuwork.pop_front();
	   delete result;
	}
	BLI_mutex_unlock(&gpumutex);
	BLI_end_threads(&gputhreads);
#endif
#if COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
	BLI_destroy_worker(cpuworker);
#endif
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	BLI_mutex_lock(&cpumutex);
	while (cpuwork.size()>0) {
	   WorkPackage * result = cpuwork.front();
	   cpuwork.pop_front();
	   delete result;
	}
	BLI_mutex_unlock(&cpumutex);
	BLI_end_threads(&cputhreads);
	
	BLI_thread_queue_free(cpuqueue);
#endif
	state = COM_WSS_STOPPED;
}

void WorkScheduler::finish() {
	while (!cpuwork.empty() && !gpuwork.empty()) {
		PIL_sleep_ms(10);
	}
}

bool WorkScheduler::hasGPUDevices() {
	return gpudevices.size()>0;
}

extern void clContextError(const char *errinfo, const void *private_info, size_t cb, void *user_data) {
	printf("OPENCL error: %s\n", errinfo);
}

void WorkScheduler::initialize() {
	state = COM_WSS_UNKNOWN;
	BLI_mutex_init(&cpumutex);
	BLI_mutex_init(&gpumutex);

#if COM_CURRENT_THREADING_MODEL == COM_TM_PTHREAD
	int numberOfCPUThreads = BLI_system_thread_count();

	for (int index = 0 ; index < numberOfCPUThreads ; index ++) {
		CPUDevice *device = new CPUDevice();
		device->initialize();
		cpudevices.push_back(device);
	}
#elif COM_CURRENT_THREADING_MODEL == COM_TM_WORKER
	int numberOfCPUThreads = BLI_system_thread_count();

	for (int index = 0 ; index < numberOfCPUThreads ; index ++) {
		CPUDevice *device = new CPUDevice();
		device->initialize();
		cpudevices.push_back(device);
	}
#elif COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	int numberOfCPUThreads = BLI_system_thread_count();

	for (int index = 0 ; index < numberOfCPUThreads ; index ++) {
		CPUDevice *device = new CPUDevice();
		device->initialize();
		cpudevices.push_back(device);
	}
#endif
#if COM_OPENCL_ENABLED
	context = NULL;
	program = NULL;
	if (clCreateContextFromType) {
		cl_uint numberOfPlatforms;
		cl_int error;
		error = clGetPlatformIDs(0, 0, &numberOfPlatforms);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
		printf("%d number of platforms\n", numberOfPlatforms);
		cl_platform_id platforms[numberOfPlatforms];
		error = clGetPlatformIDs(numberOfPlatforms, platforms, 0);
		unsigned int indexPlatform;
		cl_uint totalNumberOfDevices = 0;
		for (indexPlatform = 0 ; indexPlatform < numberOfPlatforms ; indexPlatform ++) {
			cl_platform_id platform = platforms[indexPlatform];
			cl_uint numberOfDevices;
			clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, 0, &numberOfDevices);
			totalNumberOfDevices += numberOfDevices;
		}

		cl_device_id cldevices[totalNumberOfDevices];
		unsigned int numberOfDevicesReceived = 0;
		for (indexPlatform = 0 ; indexPlatform < numberOfPlatforms ; indexPlatform ++) {
			cl_platform_id platform = platforms[indexPlatform];
			cl_uint numberOfDevices;
			clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, 0, &numberOfDevices);
			clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numberOfDevices, cldevices+numberOfDevicesReceived*sizeof (cl_device_id), 0);
			numberOfDevicesReceived += numberOfDevices;
		}
		context = clCreateContext(NULL, totalNumberOfDevices, cldevices, clContextError, NULL, &error);
		if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
		program = clCreateProgramWithSource(context, 1, &sourcecode, 0, &error);
		error = clBuildProgram(program, totalNumberOfDevices, cldevices, 0, 0, 0);
		if (error != CL_SUCCESS) { 
			cl_int error2;
			size_t ret_val_size;
			printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	
			error2 = clGetProgramBuildInfo(program, cldevices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
			if (error2 != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
			char* build_log =  new char[ret_val_size+1];
			error2 = clGetProgramBuildInfo(program, cldevices[0], CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
			if (error2 != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));	}
			build_log[ret_val_size] = '\0';
			printf("%s", build_log);
			delete build_log;
			
		}
		unsigned int indexDevices;
		for (indexDevices = 0 ; indexDevices < totalNumberOfDevices ; indexDevices ++) {
			cl_device_id device = cldevices[indexDevices];
			OpenCLDevice* clDevice = new OpenCLDevice(context, device, program);
			clDevice->initialize(),
			gpudevices.push_back(clDevice);
			char resultString[32];
			error = clGetDeviceInfo(device, CL_DEVICE_NAME, 32, resultString, 0);
			printf("OPENCL_DEVICE: %s, ", resultString);
			error = clGetDeviceInfo(device, CL_DEVICE_VENDOR, 32, resultString, 0);
			printf("%s\n", resultString);
		}
	}
#endif

	state = COM_WSS_INITIALIZED;
}

void WorkScheduler::deinitialize() {
	Device* device;
	while(cpudevices.size()>0) {
		device = cpudevices.back();
		cpudevices.pop_back();
		device->deinitialize();
		delete device;
	}
	while(gpudevices.size()>0) {
		device = gpudevices.back();
		gpudevices.pop_back();
		device->deinitialize();
		delete device;
	}
#if COM_OPENCL_ENABLED
	if (program) {
		clReleaseProgram(program);
		program = NULL;
	}
	if (context) {
		clReleaseContext(context);
		context = NULL;
	}
#endif
	BLI_mutex_end(&cpumutex);
	BLI_mutex_end(&gpumutex);
	state = COM_WSS_DEINITIALIZED;
}
