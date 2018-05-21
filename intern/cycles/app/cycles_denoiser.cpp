/*
 * Copyright 2018 Blender Foundation
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

#include "render/denoising.h"

#include "util/util_args.h"
#include "util/util_foreach.h"
#include "util/util_logging.h"
#include "util/util_path.h"
#include "util/util_system.h"
#include "util/util_version.h"

using namespace ccl;

vector<string> filenames;

static int files_parse(int argc, const char *argv[])
{
	for(int i = 0; i < argc; i++) {
		filenames.push_back(argv[i]);
	}
	return 0;
}

int main(int argc, const char **argv)
{
	util_logging_init(argv[0]);

	string source_path = "";
#ifdef CYCLES_DATA_PATH
	source_path = path_join(path_dirname(system_get_executable_path()), CYCLES_DATA_PATH);
#endif
	path_init(source_path);

	string device_names = "";
	string devicename = "CPU";
	vector<DeviceType>& types = Device::available_types();
	foreach(DeviceType type, types) {
		if(device_names != "")
			device_names += ", ";
		device_names += Device::string_from_type(type);
	}

	ArgParse ap;
	bool help = false, debug = false, version = false, list = false, prefilter = false, views = false, relative_pca = false, ldr = false;
	bool no_passthrough_additional = false, no_passthrough_unknown = false, no_passthrough_incomplete = false, profiling = false;
	int verbosity = 1, threads = 0, samples = 0, frame_radius = 2, radius = 8;
	float strength = 0.5f, feature_strength = 0.5f;
	int2 tile_size = make_int2(64, 64);
	string center_frame;

	ap.options ("Usage: cycles_denoiser [options] --samples <spp> <input> <output>",
		"%*", files_parse, "",
		"--prefilter", &prefilter, "Prefilter the input frame",
		"--device %s", &devicename, ("Devices to use: " + device_names).c_str(),
		"--threads %d", &threads, "CPU Rendering Threads",
		"--tile-width %d", &tile_size.x, "Tile width in pixels",
		"--tile-height %d", &tile_size.y, "Tile height in pixels",
		"--list-devices", &list, "List information about all available devices and exit",
		"--samples %d", &samples, "Override for the number of samples that the image was rendered with",
		"--center-frame %s", &center_frame, "Frame(s) to be denoised",
		"--frame-radius %d", &frame_radius, "How many frames to denoise before and after the center frame (overridden by frame sequences in the input pattern)",
		"--radius %d", &radius, "Denoising radius in pixels (default is 8)",
		"--strength %f", &strength, "Denoising strength (between 0 and 1, default is 0.5)",
		"--feature-strength %f", &feature_strength, "Denoising feature strength (between 0 and 1, default is 0.5)",
		"--relative-pca", &relative_pca, "Use relative PCA (default is off)",
		"--no-passthrough-additional", &no_passthrough_additional, "Don't passthrough non-denoising channels of processed render layers",
		"--no-passthrough-incomplete", &no_passthrough_incomplete, "Don't passthrough render layers without a full set of denoising channels",
		"--no-passthrough-unknown", &no_passthrough_unknown, "Don't passthrough channels that couldn't be parsed",
		"--profiling", &profiling, "Output profiling information",
		"--ldr", &ldr, "Transform the output image to sRGB before saving (used for outputting file formats like PNG or JPEG)",
#ifdef WITH_CYCLES_LOGGING
		"--debug", &debug, "Enable debug logging",
		"--verbose %d", &verbosity, "Set verbosity of the logger",
#endif
		"--views", &views, "Input frames contain multiple views",
		"--help", &help, "Print help message and exit",
		"--version", &version, "Print version number and exit",
		NULL);

	if(ap.parse(argc, argv) < 0) {
		fprintf(stderr, "%s\n", ap.geterror().c_str());
		ap.usage();
		exit(EXIT_FAILURE);
	}

	if(version) {
		printf("%s\n", CYCLES_VERSION_STRING);
		exit(EXIT_SUCCESS);
	}
	if(list) {
		vector<DeviceInfo>& devices = Device::available_devices();
		printf("Devices:\n");
		foreach(DeviceInfo& info, devices) {
			printf("    %-10s%s%s\n",
				Device::string_from_type(info.type).c_str(),
				info.description.c_str(),
				(info.display_device)? " (display)": "");
		}
		exit(EXIT_SUCCESS);
	}
	if(help || filenames.size() != 2) {
		ap.usage();
		exit(EXIT_FAILURE);
	}

	if(debug) {
		util_logging_start();
		util_logging_verbosity_set(verbosity);
	}

	TaskScheduler::init(threads);
	if(std::atexit(TaskScheduler::exit) != 0) {
		fprintf(stderr, "Registering atexit failed!\n");
		TaskScheduler::exit();
		exit(EXIT_FAILURE);
	}

	/* find matching device */
	DeviceType device_type = Device::type_from_string(devicename.c_str());
	vector<DeviceInfo>& devices = Device::available_devices();
	bool device_available = false;
	DeviceInfo device_info;

	foreach(DeviceInfo& device, devices) {
		if(device_type == device.type) {
			device_info = device;
			device_available = true;
			break;
		}
	}

	if(device_info.type == DEVICE_NONE || !device_available) {
		fprintf(stderr, "Unknown device: %s\n", devicename.c_str());
		exit(EXIT_FAILURE);
	}

	Stats stats;
	StandaloneDenoiser denoiser(Device::create(device_info, stats, true));
	denoiser.views = views;
	denoiser.tile_size = tile_size;
	denoiser.samples = samples;
	denoiser.ldr_out = ldr;
	denoiser.center_frame = center_frame;
	denoiser.frame_radius = frame_radius;
	denoiser.strength = strength;
	denoiser.feature_strength = feature_strength;
	denoiser.relative_pca = relative_pca;
	denoiser.radius = radius;
	denoiser.in_path = filenames[0];
	denoiser.out_path = filenames[1];
	denoiser.passthrough_incomplete = !no_passthrough_incomplete;
	denoiser.passthrough_additional = !no_passthrough_additional;
	denoiser.passthrough_unknown = !no_passthrough_unknown;

	if(prefilter) {
		if(!denoiser.run_prefilter()) {
			fprintf(stderr, "%s\n", denoiser.error.c_str());
			exit(EXIT_FAILURE);
		}
	}
	else {
		if(!denoiser.run_filter()) {
			fprintf(stderr, "%s\n", denoiser.error.c_str());
			exit(EXIT_FAILURE);
		}
	}

	if(profiling) {
		denoiser.output_profiling();
	}

	return 0;
}