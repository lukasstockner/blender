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

#include "opencl/opencl.h"

#include "device_intern.h"

CCL_NAMESPACE_BEGIN

cl_device_type opencl_device_type()
{
	switch(DebugFlags().opencl.device_type)
	{
		case DebugFlags::OpenCL::DEVICE_NONE:
			return 0;
		case DebugFlags::OpenCL::DEVICE_ALL:
			return CL_DEVICE_TYPE_ALL;
		case DebugFlags::OpenCL::DEVICE_DEFAULT:
			return CL_DEVICE_TYPE_DEFAULT;
		case DebugFlags::OpenCL::DEVICE_CPU:
			return CL_DEVICE_TYPE_CPU;
		case DebugFlags::OpenCL::DEVICE_GPU:
			return CL_DEVICE_TYPE_GPU;
		case DebugFlags::OpenCL::DEVICE_ACCELERATOR:
			return CL_DEVICE_TYPE_ACCELERATOR;
		default:
			return CL_DEVICE_TYPE_ALL;
	}
}

bool opencl_kernel_use_debug()
{
	return DebugFlags().opencl.debug;
}

bool opencl_kernel_use_advanced_shading(const string& platform)
{
	/* keep this in sync with kernel_types.h! */
	if(platform == "NVIDIA CUDA")
		return true;
	else if(platform == "Apple")
		return true;
	else if(platform == "AMD Accelerated Parallel Processing")
		return true;
	else if(platform == "Intel(R) OpenCL")
		return true;
	/* Make sure officially unsupported OpenCL platforms
	 * does not set up to use advanced shading.
	 */
	return false;
}

bool opencl_kernel_use_split(const string& platform_name,
                             const cl_device_type device_type)
{
	if(DebugFlags().opencl.kernel_type == DebugFlags::OpenCL::KERNEL_SPLIT) {
		VLOG(1) << "Forcing split kernel to use.";
		return true;
	}
	if(DebugFlags().opencl.kernel_type == DebugFlags::OpenCL::KERNEL_MEGA) {
		VLOG(1) << "Forcing mega kernel to use.";
		return false;
	}
	/* TODO(sergey): Replace string lookups with more enum-like API,
	 * similar to device/vendor checks blender's gpu.
	 */
	if(platform_name == "AMD Accelerated Parallel Processing" &&
	   device_type == CL_DEVICE_TYPE_GPU)
	{
		return true;
	}
	return false;
}

bool opencl_device_supported(const string& platform_name,
                             const cl_device_id device_id)
{
	cl_device_type device_type;
	clGetDeviceInfo(device_id,
	                CL_DEVICE_TYPE,
	                sizeof(cl_device_type),
	                &device_type,
	                NULL);
	if(platform_name == "AMD Accelerated Parallel Processing" &&
	   device_type == CL_DEVICE_TYPE_GPU)
	{
		return true;
	}
	if(platform_name == "Apple" && device_type == CL_DEVICE_TYPE_GPU) {
		return true;
	}
	return false;
}

bool opencl_platform_version_check(cl_platform_id platform,
                                   string *error)
{
	const int req_major = 1, req_minor = 1;
	int major, minor;
	char version[256];
	clGetPlatformInfo(platform,
	                  CL_PLATFORM_VERSION,
	                  sizeof(version),
	                  &version,
	                  NULL);
	if(sscanf(version, "OpenCL %d.%d", &major, &minor) < 2) {
		if(error != NULL) {
			*error = string_printf("OpenCL: failed to parse platform version string (%s).", version);
		}
		return false;
	}
	if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
		if(error != NULL) {
			*error = string_printf("OpenCL: platform version 1.1 or later required, found %d.%d", major, minor);
		}
		return false;
	}
	if(error != NULL) {
		*error = "";
	}
	return true;
}

bool opencl_device_version_check(cl_device_id device,
                                 string *error)
{
	const int req_major = 1, req_minor = 1;
	int major, minor;
	char version[256];
	clGetDeviceInfo(device,
	                CL_DEVICE_OPENCL_C_VERSION,
	                sizeof(version),
	                &version,
	                NULL);
	if(sscanf(version, "OpenCL C %d.%d", &major, &minor) < 2) {
		if(error != NULL) {
			*error = string_printf("OpenCL: failed to parse OpenCL C version string (%s).", version);
		}
		return false;
	}
	if(!((major == req_major && minor >= req_minor) || (major > req_major))) {
		if(error != NULL) {
			*error = string_printf("OpenCL: C version 1.1 or later required, found %d.%d", major, minor);
		}
		return false;
	}
	if(error != NULL) {
		*error = "";
	}
	return true;
}

void opencl_get_usable_devices(vector<OpenCLPlatformDevice> *usable_devices, bool force_all)
{
	const bool force_all_platforms = force_all ||
		(DebugFlags().opencl.kernel_type != DebugFlags::OpenCL::KERNEL_DEFAULT);
	const cl_device_type device_type = opencl_device_type();
	static bool first_time = true;
#define FIRST_VLOG(severity) if(first_time) VLOG(severity)

	usable_devices->clear();

	if(device_type == 0) {
		FIRST_VLOG(2) << "OpenCL devices are forced to be disabled.";
		first_time = false;
		return;
	}

	vector<cl_device_id> device_ids;
	cl_uint num_devices = 0;
	vector<cl_platform_id> platform_ids;
	cl_uint num_platforms = 0;

	/* Get devices. */
	if(clGetPlatformIDs(0, NULL, &num_platforms) != CL_SUCCESS ||
	   num_platforms == 0)
	{
		FIRST_VLOG(2) << "No OpenCL platforms were found.";
		first_time = false;
		return;
	}
	platform_ids.resize(num_platforms);
	if(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL) != CL_SUCCESS) {
		FIRST_VLOG(2) << "Failed to fetch platform IDs from the driver..";
		first_time = false;
		return;
	}
	/* Devices are numbered consecutively across platforms. */
	for(int platform = 0; platform < num_platforms; platform++) {
		cl_platform_id platform_id = platform_ids[platform];
		char pname[256];
		if(clGetPlatformInfo(platform_id,
		                     CL_PLATFORM_NAME,
		                     sizeof(pname),
		                     &pname,
		                     NULL) != CL_SUCCESS)
		{
			FIRST_VLOG(2) << "Failed to get platform name, ignoring.";
			continue;
		}
		string platform_name = pname;
		FIRST_VLOG(2) << "Enumerating devices for platform "
		              << platform_name << ".";
		if(!opencl_platform_version_check(platform_id)) {
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << " due to too old compiler version.";
			continue;
		}
		num_devices = 0;
		if(clGetDeviceIDs(platform_id,
		                  device_type,
		                  0,
		                  NULL,
		                  &num_devices) != CL_SUCCESS || num_devices == 0)
		{
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << ", failed to fetch number of devices.";
			continue;
		}
		device_ids.resize(num_devices);
		if(clGetDeviceIDs(platform_id,
		                  device_type,
		                  num_devices,
		                  &device_ids[0],
		                  NULL) != CL_SUCCESS)
		{
			FIRST_VLOG(2) << "Ignoring platform " << platform_name
			              << ", failed to fetch devices list.";
			continue;
		}
		for(int num = 0; num < num_devices; num++) {
			cl_device_id device_id = device_ids[num];
			char device_name[1024] = "\0";
			if(clGetDeviceInfo(device_id,
			                   CL_DEVICE_NAME,
			                   sizeof(device_name),
			                   &device_name,
			                   NULL) != CL_SUCCESS)
			{
				FIRST_VLOG(2) << "Failed to fetch device name, ignoring.";
				continue;
			}
			if(!opencl_device_version_check(device_id)) {
				FIRST_VLOG(2) << "Ignoring device " << device_name
				              << " due to old compiler version.";
				continue;
			}
			if(force_all_platforms ||
			   opencl_device_supported(platform_name, device_id))
			{
				cl_device_type device_type;
				if(clGetDeviceInfo(device_id,
				                   CL_DEVICE_TYPE,
				                   sizeof(cl_device_type),
				                   &device_type,
				                   NULL) != CL_SUCCESS)
				{
					FIRST_VLOG(2) << "Ignoring device " << device_name
					              << ", failed to fetch device type.";
					continue;
				}
				FIRST_VLOG(2) << "Adding new device " << device_name << ".";
				usable_devices->push_back(OpenCLPlatformDevice(platform_id,
				                                               platform_name,
				                                               device_id,
				                                               device_type,
				                                               device_name));
			}
			else {
				FIRST_VLOG(2) << "Ignoring device " << device_name
				              << ", not officially supported yet.";
			}
		}
	}
	first_time = false;
}

Device *device_opencl_create(DeviceInfo& info, Stats &stats, bool background)
{
	vector<OpenCLPlatformDevice> usable_devices;
	opencl_get_usable_devices(&usable_devices);
	assert(info.num < usable_devices.size());
	const OpenCLPlatformDevice& platform_device = usable_devices[info.num];
	const string& platform_name = platform_device.platform_name;
	const cl_device_type device_type = platform_device.device_type;
	if(opencl_kernel_use_split(platform_name, device_type)) {
		VLOG(1) << "Using split kernel.";
		return opencl_create_split_device(info, stats, background);
	} else {
		VLOG(1) << "Using mega kernel.";
		return opencl_create_mega_device(info, stats, background);
	}
}

bool device_opencl_init(void)
{
	static bool initialized = false;
	static bool result = false;

	if(initialized)
		return result;

	initialized = true;

	if(opencl_device_type() != 0) {
		int clew_result = clewInit();
		if(clew_result == CLEW_SUCCESS) {
			VLOG(1) << "CLEW initialization succeeded.";
			result = true;
		}
		else {
			VLOG(1) << "CLEW initialization failed: "
			        << ((clew_result == CLEW_ERROR_ATEXIT_FAILED)
			            ? "Error setting up atexit() handler"
			            : "Error opening the library");
		}
	}
	else {
		VLOG(1) << "Skip initializing CLEW, platform is force disabled.";
		result = false;
	}

	return result;
}

void device_opencl_info(vector<DeviceInfo>& devices)
{
	vector<OpenCLPlatformDevice> usable_devices;
	opencl_get_usable_devices(&usable_devices);
	/* Devices are numbered consecutively across platforms. */
	int num_devices = 0;
	foreach(OpenCLPlatformDevice& platform_device, usable_devices) {
		const string& platform_name = platform_device.platform_name;
		const cl_device_type device_type = platform_device.device_type;
		const string& device_name = platform_device.device_name;
		DeviceInfo info;
		info.type = DEVICE_OPENCL;
		info.description = string_remove_trademark(string(device_name));
		info.num = num_devices;
		info.id = string_printf("OPENCL_%d", info.num);
		/* We don't know if it's used for display, but assume it is. */
		info.display_device = true;
		info.advanced_shading = opencl_kernel_use_advanced_shading(platform_name);
		info.pack_images = true;
		info.use_split_kernel = opencl_kernel_use_split(platform_name,
		                                                device_type);
		devices.push_back(info);
		num_devices++;
	}
}

string device_opencl_capabilities(void)
{
	if(opencl_device_type() == 0) {
		return "All OpenCL devices are forced to be OFF";
	}
	string result = "";
	string error_msg = "";  /* Only used by opencl_assert(), but in the future
	                         * it could also be nicely reported to the console.
	                         */
	cl_uint num_platforms = 0;
	opencl_assert(clGetPlatformIDs(0, NULL, &num_platforms));
	if(num_platforms == 0) {
		return "No OpenCL platforms found\n";
	}
	result += string_printf("Number of platforms: %u\n", num_platforms);

	vector<cl_platform_id> platform_ids;
	platform_ids.resize(num_platforms);
	opencl_assert(clGetPlatformIDs(num_platforms, &platform_ids[0], NULL));

#define APPEND_STRING_INFO(func, id, name, what) \
	do { \
		char data[1024] = "\0"; \
		opencl_assert(func(id, what, sizeof(data), &data, NULL)); \
		result += string_printf("%s: %s\n", name, data); \
	} while(false)
#define APPEND_PLATFORM_STRING_INFO(id, name, what) \
	APPEND_STRING_INFO(clGetPlatformInfo, id, "\tPlatform " name, what)
#define APPEND_DEVICE_STRING_INFO(id, name, what) \
	APPEND_STRING_INFO(clGetDeviceInfo, id, "\t\t\tDevice " name, what)

	vector<cl_device_id> device_ids;
	for(cl_uint platform = 0; platform < num_platforms; ++platform) {
		cl_platform_id platform_id = platform_ids[platform];

		result += string_printf("Platform #%u\n", platform);

		APPEND_PLATFORM_STRING_INFO(platform_id, "Name", CL_PLATFORM_NAME);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Vendor", CL_PLATFORM_VENDOR);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Version", CL_PLATFORM_VERSION);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Profile", CL_PLATFORM_PROFILE);
		APPEND_PLATFORM_STRING_INFO(platform_id, "Extensions", CL_PLATFORM_EXTENSIONS);

		cl_uint num_devices = 0;
		opencl_assert(clGetDeviceIDs(platform_ids[platform],
		                             CL_DEVICE_TYPE_ALL,
		                             0,
		                             NULL,
		                             &num_devices));
		result += string_printf("\tNumber of devices: %u\n", num_devices);

		device_ids.resize(num_devices);
		opencl_assert(clGetDeviceIDs(platform_ids[platform],
		                             CL_DEVICE_TYPE_ALL,
		                             num_devices,
		                             &device_ids[0],
		                             NULL));
		for(cl_uint device = 0; device < num_devices; ++device) {
			cl_device_id device_id = device_ids[device];

			result += string_printf("\t\tDevice: #%u\n", device);

			APPEND_DEVICE_STRING_INFO(device_id, "Name", CL_DEVICE_NAME);
			APPEND_DEVICE_STRING_INFO(device_id, "Vendor", CL_DEVICE_VENDOR);
			APPEND_DEVICE_STRING_INFO(device_id, "OpenCL C Version", CL_DEVICE_OPENCL_C_VERSION);
			APPEND_DEVICE_STRING_INFO(device_id, "Profile", CL_DEVICE_PROFILE);
			APPEND_DEVICE_STRING_INFO(device_id, "Version", CL_DEVICE_VERSION);
			APPEND_DEVICE_STRING_INFO(device_id, "Extensions", CL_DEVICE_EXTENSIONS);
		}
	}

#undef APPEND_STRING_INFO
#undef APPEND_PLATFORM_STRING_INFO
#undef APPEND_DEVICE_STRING_INFO

	return result;
}

CCL_NAMESPACE_END

#endif /* WITH_OPENCL */
