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

#include <stdio.h>

#include "device/device.h"

#include "util/util_args.h"
#include "util/util_foreach.h"
#include "util/util_path.h"
#include "util/util_stats.h"
#include "util/util_string.h"
#include "util/util_task.h"
#include "util/util_logging.h"
#include <OpenImageIO/sysutil.h>

using namespace ccl;

static bool get_device(vector<DeviceInfo>& devices, DeviceInfo &info, DeviceType type)
{
	foreach(DeviceInfo& device, devices) {
		if(type == device.type) {
			info = device;
			return true;
		}
	}
	return false;
}


int main(int argc, const char **argv)
{
	util_logging_init(argv[0]);
#ifdef WITH_BLENDER
	/* TODO: This is horribly hacky. */
	bool found_folder = false;
	for(int i = 80; i > 77; i--) {
		string folder = string_printf("2.%d/scripts/addons/cycles", i);
		string path = path_join(path_dirname(Sysutil::this_program_path()), folder);
		if(path_exists(path)) {
			path_init(path, "");
			found_folder = true;
			break;
		}
	}
	if(!found_folder)
#endif
	{
		path_init();
	}

	/* device types */
	string devicelist = "";
	string devicename = "CPU";
	string announce_address = "";
	bool list = false, debug = false;
	int threads = 0, verbosity = 1;

	vector<DeviceType>& types = Device::available_types();

	foreach(DeviceType type, types) {
		if(devicelist != "")
			devicelist += ", ";

		devicelist += Device::string_from_type(type);
	}

	/* parse options */
	ArgParse ap;

	ap.options ("Usage: cycles_server [options]",
		"--device %s", &devicename, ("Devices to use: " + devicelist).c_str(),
		"--list-devices", &list, "List information about all available devices",
		"--threads %d", &threads, "Number of threads to use for CPU device",
		"--announce-to %s", &announce_address, "Continuously announce presence to this address",
#ifdef WITH_CYCLES_LOGGING
		"--debug", &debug, "Enable debug logging",
		"--verbose %d", &verbosity, "Set verbosity of the logger",
#endif
		NULL);

	if(ap.parse(argc, argv) < 0) {
		fprintf(stderr, "%s\n", ap.geterror().c_str());
		ap.usage();
		exit(EXIT_FAILURE);
	}

	if(debug) {
		util_logging_start();
		util_logging_verbosity_set(verbosity);
	}

	if(list) {
		vector<DeviceInfo>& devices = Device::available_devices();

		printf("Devices:\n");

		foreach(DeviceInfo& info, devices) {
			printf("    %s%s\n",
				info.description.c_str(),
				(info.display_device)? " (display)": "");
		}

		exit(EXIT_SUCCESS);
	}

	/* find matching device */
	DeviceType device_type = Device::type_from_string(devicename.c_str());
	vector<DeviceInfo>& devices = Device::available_devices();
	DeviceInfo device_info;

	if(!get_device(devices, device_info, device_type)) {
		printf("Couldn't find device for type %s, falling back to CPU!\n", devicename.c_str());
		get_device(devices, device_info, DEVICE_CPU);
	}

	TaskScheduler::init(threads);

	while(1) {
		Stats stats;
		Device *device = Device::create(device_info, stats, true);
		printf("Cycles Server with device: %s\n", device->info.description.c_str());
		device->server_run(announce_address);
		delete device;
	}

	TaskScheduler::exit();

	return 0;
}

