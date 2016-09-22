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

#include "util_path.h"
#include "util_time.h"
#include "util_system.h"

using std::cerr;
using std::endl;

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

string OpenCLCache::get_kernel_md5()
{
	OpenCLCache &self = global_instance();
	thread_scoped_lock lock(self.kernel_md5_lock);

	if(self.kernel_md5.empty()) {
		self.kernel_md5 = path_files_md5_hash(path_get("kernel"));
	}
	return self.kernel_md5;
}

OpenCLDeviceBase::OpenCLProgram::OpenCLProgram(OpenCLDeviceBase *device, string program_name, string kernel_file, string kernel_build_options)
 : device(device),
   program_name(program_name),
   kernel_file(kernel_file),
   kernel_build_options(kernel_build_options)
{
	loaded = false;
	program = NULL;
}

OpenCLDeviceBase::OpenCLProgram::~OpenCLProgram()
{
	release();
}

void OpenCLDeviceBase::OpenCLProgram::release()
{
	for(map<ustring, cl_kernel>::iterator kernel = kernels.begin(); kernel != kernels.end(); ++kernel) {
		if(kernel->second) {
			clReleaseKernel(kernel->second);
			kernel->second = NULL;
		}
	}
	if(program) {
		clReleaseProgram(program);
		program = NULL;
	}
}

void OpenCLDeviceBase::OpenCLProgram::add_kernel(ustring name)
{
	if(!kernels.count(name)) {
		kernels[name] = NULL;
	}
}

bool OpenCLDeviceBase::OpenCLProgram::build_kernel(const string *debug_src)
{
	string build_options;
	build_options = device->kernel_build_options(debug_src) + kernel_build_options;

	cl_int ciErr = clBuildProgram(program, 0, NULL, build_options.c_str(), NULL, NULL);

	/* show warnings even if build is successful */
	size_t ret_val_size = 0;

	clGetProgramBuildInfo(program, device->cdDevice, CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);

	if(ret_val_size > 1) {
		vector<char> build_log(ret_val_size + 1);
		clGetProgramBuildInfo(program, device->cdDevice, CL_PROGRAM_BUILD_LOG, ret_val_size, &build_log[0], NULL);

		build_log[ret_val_size] = '\0';
		/* Skip meaningless empty output from the NVidia compiler. */
		if(!(ret_val_size == 2 && build_log[0] == '\n')) {
			output_msg = string(&build_log[0]);
		}
	}

	if(ciErr != CL_SUCCESS) {
		error_msg = string("OpenCL build failed: ") + clewErrorString(ciErr);
		return false;
	}

	return true;
}

bool OpenCLDeviceBase::OpenCLProgram::compile_kernel(const string *debug_src)
{
	string source = "#include \"kernels/opencl/" + kernel_file + "\" // " + OpenCLCache::get_kernel_md5() + "\n";
	/* We compile kernels consisting of many files. unfortunately OpenCL
	 * kernel caches do not seem to recognize changes in included files.
	 * so we force recompile on changes by adding the md5 hash of all files.
	 */
	source = path_source_replace_includes(source, path_get("kernel"));

	if(debug_src) {
		path_write_text(*debug_src, source);
	}

	size_t source_len = source.size();
	const char *source_str = source.c_str();
	cl_int ciErr;

	program = clCreateProgramWithSource(device->cxContext,
	                                   1,
	                                   &source_str,
	                                   &source_len,
	                                   &ciErr);

	if(ciErr != CL_SUCCESS) {
		error_msg = string("OpenCL program creation failed: ") + clewErrorString(ciErr);
		return false;
	}

	double starttime = time_dt();

	log += "Build flags: " + kernel_build_options + "\n";

	if(!build_kernel(debug_src))
		return false;

	log += "Kernel compilation of " + program_name + " finished in " + string_printf("%.2lfs.\n", time_dt() - starttime);

	return true;
}

bool OpenCLDeviceBase::OpenCLProgram::build_process(const string& clbin)
{
	vector<string> args;
	args.push_back("-b");
	args.push_back("--python-expr");

	args.push_back(string_printf("'import _cycles; _cycles.opencl_compile(%s, %d, \"%s\", \"%s\", \"%s\", \"%s\", \"%s\")'",
	                             (DebugFlags().opencl.kernel_type != DebugFlags::OpenCL::KERNEL_DEFAULT)? "True" : "False",
                                     device->device_num, device->device_name.c_str(), device->platform_name.c_str(),
	                             (device->kernel_build_options(NULL) + kernel_build_options).c_str(),
	                             kernel_file.c_str(), clbin.c_str()));

	return call_self(args);
}

bool OpenCLDeviceBase::OpenCLProgram::load_binary(const string& clbin,
                                                  const string *debug_src)
{
	/* read binary into memory */
	vector<uint8_t> binary;

	if(!path_read_binary(clbin, binary)) {
		error_msg = "OpenCL failed to read cached binary " + clbin + ".";
		return false;
	}

	/* create program */
	cl_int status, ciErr;
	size_t size = binary.size();
	const uint8_t *bytes = &binary[0];

	program = clCreateProgramWithBinary(device->cxContext, 1, &device->cdDevice,
		&size, &bytes, &status, &ciErr);

	if(status != CL_SUCCESS || ciErr != CL_SUCCESS) {
		error_msg = "OpenCL failed create program from cached binary " + clbin + ": " + clewErrorString(status) + " " + clewErrorString(ciErr);
		return false;
	}

	if(!build_kernel(debug_src))
		return false;

	return true;
}

bool OpenCLDeviceBase::OpenCLProgram::save_binary(const string& clbin)
{
	size_t size = 0;
	clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);

	if(!size)
		return false;

	vector<uint8_t> binary(size);
	uint8_t *bytes = &binary[0];

	clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);

	return path_write_binary(clbin, binary);
}

void OpenCLDeviceBase::OpenCLProgram::load()
{
	assert(device);

	loaded = false;

	string device_md5 = device->device_md5_hash(kernel_build_options);

	/* Try to use cached kernel. */
	thread_scoped_lock cache_locker;
	ustring cache_key(program_name + device_md5);
	program = device->load_cached_kernel(cache_key,
	                                     cache_locker);

	if(!program) {
		log += "OpenCL program " + program_name + " not found in cache.\n";

		string basename = "cycles_kernel_" + program_name + "_" + device_md5 + "_" + OpenCLCache::get_kernel_md5();
		basename = path_cache_get(path_join("kernels", basename));
		string clbin = basename + ".clbin";

		/* path to preprocessed source for debugging */
		string clsrc, *debug_src = NULL;

		if(opencl_kernel_use_debug()) {
			clsrc = basename + ".cl";
			debug_src = &clsrc;
		}

		/* If binary kernel exists already, try use it. */
		if(path_exists(clbin) && load_binary(clbin)) {
			/* Kernel loaded from binary, nothing to do. */
			log += "Loaded program from " + clbin + ".\n";
		}
		else {
			if(!path_exists(clbin) && build_process(clbin) && path_exists(clbin) && load_binary(clbin)) {
				log += "Build and loded program from " + clbin + ".\n";
			}
			else {
				log += "Kernel file " + clbin + " either doesn't exist or failed to be loaded by driver.\n";

				/* If does not exist or loading binary failed, compile kernel. */
				if(!compile_kernel(debug_src)) {
					return;
				}

				/* Save binary for reuse. */
				if(!save_binary(clbin)) {
					log += "Saving compiled OpenCL kernel to " + clbin + " failed!";
				}
			}
		}

		/* Cache the program. */
		device->store_cached_kernel(program,
		                            cache_key,
		                            cache_locker);
	}
	else {
		log += "Found cached OpenCL program " + program_name + ".\n";
	}

	for(map<ustring, cl_kernel>::iterator kernel = kernels.begin(); kernel != kernels.end(); ++kernel) {
		assert(kernel->second == NULL);
		cl_int ciErr;
		string name = "kernel_ocl_" + kernel->first.string();
		kernel->second = clCreateKernel(program, name.c_str(), &ciErr);
		if(device->opencl_error(ciErr)) {
			error_msg = "Error getting kernel " + name + " from program " + program_name + ": " + clewErrorString(ciErr);
			return;
		}
	}

	loaded = true;
}

void OpenCLDeviceBase::OpenCLProgram::report_error()
{
	if(loaded) return;

	cerr << error_msg << endl;
	if(!output_msg.empty()) {
		cerr << "OpenCL kernel build output for " << program_name << ":" << endl;
		cerr << output_msg << endl;
	}
}

cl_kernel OpenCLDeviceBase::OpenCLProgram::operator()()
{
	assert(kernels.size() == 1);
	return kernels.begin()->second;
}

cl_kernel OpenCLDeviceBase::OpenCLProgram::operator()(ustring name)
{
	assert(kernels.count(name));
	return kernels[name];
}

bool opencl_build_kernel(bool force_all_platforms, int device_platform_id, string device_name, string platform_name, string build_options, string kernel_file, string binary_path)
{
	bool result = false;

	if(clewInit() != CLEW_SUCCESS) return false;

	vector<OpenCLPlatformDevice> usable_devices;
	opencl_get_usable_devices(&usable_devices, force_all_platforms);
	if(device_platform_id >= usable_devices.size()) return false;

	OpenCLPlatformDevice& platform_device = usable_devices[device_platform_id];
	if(platform_device.platform_name != platform_name ||
	   platform_device.device_name != device_name) {
		return false;
	}

	cl_platform_id platform = platform_device.platform_id;
	cl_device_id device = platform_device.device_id;
	const cl_context_properties context_props[] = {
		CL_CONTEXT_PLATFORM, (cl_context_properties) platform,
		0, 0
	};

	cl_int err;
	cl_context context = clCreateContext(context_props, 1, &device, NULL, NULL, &err);
	if(err != CL_SUCCESS) return false;

	string source = "#include \"kernels/opencl/" + kernel_file + "\" // " + path_files_md5_hash(path_get("kernel")) + "\n";
	source = path_source_replace_includes(source, path_get("kernel"));
	size_t source_len = source.size();
	const char *source_str = source.c_str();
	cl_program program = clCreateProgramWithSource(context, 1, &source_str, &source_len, &err);
	if(err == CL_SUCCESS) {
		err = clBuildProgram(program, 0, NULL, build_options.c_str(), NULL, NULL);
		if(err == CL_SUCCESS) {
			size_t size = 0;
			clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, sizeof(size_t), &size, NULL);
			if(size > 0) {
				vector<uint8_t> binary(size);
				uint8_t *bytes = &binary[0];
				clGetProgramInfo(program, CL_PROGRAM_BINARIES, sizeof(uint8_t*), &bytes, NULL);
				result = path_write_binary(binary_path, binary);
			}
		}
		clReleaseProgram(program);
	}

	clReleaseContext(context);

	return result;
}

CCL_NAMESPACE_END

#endif
