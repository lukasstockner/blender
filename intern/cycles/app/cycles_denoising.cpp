/*
 * Copyright 2016 Blender Foundation
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

#include "cycles_denoising.h"

#include "util_image.h"

CCL_NAMESPACE_BEGIN

typedef class PassTypeInfo
{
public:
	PassTypeInfo(DenoiseExtendedTypes type, int num_channels, string channels)
	 : type(type), num_channels(num_channels), channels(channels) {}
	PassTypeInfo() : type(EX_TYPE_NONE), num_channels(0), channels("") {}

	DenoiseExtendedTypes type;
	int num_channels;
	string channels;

	bool operator<(const PassTypeInfo &other) const {
		return type < other.type;
	}
} PassTypeInfo;

static map<string, PassTypeInfo> denoise_passes_init()
{
	map<string, PassTypeInfo> passes;

	passes["DenoiseNormal"]    = PassTypeInfo(EX_TYPE_DENOISE_NORMAL,     3, "XYZ");
	passes["DenoiseNormalVar"] = PassTypeInfo(EX_TYPE_DENOISE_NORMAL_VAR, 3, "XYZ");
	passes["DenoiseAlbedo"]    = PassTypeInfo(EX_TYPE_DENOISE_ALBEDO,     3, "RGB");
	passes["DenoiseAlbedoVar"] = PassTypeInfo(EX_TYPE_DENOISE_ALBEDO_VAR, 3, "RGB");
	passes["DenoiseDepth"]     = PassTypeInfo(EX_TYPE_DENOISE_DEPTH,      1, "Z");
	passes["DenoiseDepthVar"]  = PassTypeInfo(EX_TYPE_DENOISE_DEPTH_VAR,  1, "Z");
	passes["DenoiseShadowA"]   = PassTypeInfo(EX_TYPE_DENOISE_SHADOW_A,   3, "RGB");
	passes["DenoiseShadowB"]   = PassTypeInfo(EX_TYPE_DENOISE_SHADOW_B,   3, "RGB");
	passes["DenoiseNoisy"]     = PassTypeInfo(EX_TYPE_DENOISE_NOISY,      3, "RGB");
	passes["DenoiseNoisyVar"]  = PassTypeInfo(EX_TYPE_DENOISE_NOISY_VAR,  3, "RGB");
	passes["DenoiseClean"]     = PassTypeInfo(EX_TYPE_DENOISE_CLEAN,      3, "RGB");

	return passes;
}

static map<string, PassTypeInfo> denoise_passes_map = denoise_passes_init();

static bool split_channel(string full_channel, string &layer, string &pass, string &channel)
{
	/* Splits channel name into <layer>.<pass>.<channel> */
	if(std::count(full_channel.begin(), full_channel.end(), '.') != 2) {
		return false;
	}

	int first_dot = full_channel.find(".");
	int second_dot = full_channel.rfind(".");
	layer = full_channel.substr(0, first_dot);
	pass = full_channel.substr(first_dot + 1, second_dot - first_dot - 1);
	channel = full_channel.substr(second_dot + 1);

	return true;
}

static int find_channel(string channels, string channel)
{
	if(channel.length() != 1) return -1;
	size_t pos = channels.find(channel);
	if(pos == string::npos) return -1;
	return pos;
}

static RenderBuffers* load_frame(string file, Device *device)
{
	RenderBuffers *buffers = NULL;

	ImageInput *frame = ImageInput::open(file);
	if(!frame) {
		printf("Couldn't open frame %s!\n", file.c_str());
		return NULL;
	}

	const ImageSpec &spec = frame->spec();

	/* Find a single RenderLayer to load. */
	string renderlayer = "";
	string layer, pass, channel;
	for(int i = 0; i < spec.nchannels; i++) {
		if(!split_channel(spec.channelnames[i], layer, pass, channel)) continue;
		if(pass == "DenoiseNoisy") {
			renderlayer = layer;
			break;
		}
	}

	if(renderlayer != "") {
		/* Find all passes that the frame contains. */
		int passes = EX_TYPE_NONE;
		map<DenoiseExtendedTypes, int> num_channels;
		map<PassTypeInfo, int3> channel_ids;
		for(int i = 0; i < spec.nchannels; i++) {
			if(!split_channel(spec.channelnames[i], layer, pass, channel)) continue;
			if(layer != renderlayer) {
				/* The channel belongs to another RenderLayer. */
				continue;
			}
			if(denoise_passes_map.count(pass)) {
				PassTypeInfo type = denoise_passes_map[pass];
				assert(type.num_channels <= 3);
				/* Pass was found, count the channels. */
				size_t channel_id = find_channel(type.channels, channel);
				if(channel_id != -1) {
					/* This channel is part of the pass, so count it. */
					num_channels[type.type]++;
					/* Remember which OIIO channel belongs to which pass. */
					channel_ids[type][channel_id] = i;
					if(num_channels[type.type] == type.num_channels) {
						/* We found all the channels of the pass! */
						passes |= type.type;
					}
				}
			}
		}

		if((~passes & EX_TYPE_DENOISE_REQUIRED) == 0) {
			printf("Found all needed passes in the frame!\n");

			BufferParams params;
			params.width  = params.full_width  = params.final_width  = spec.width;
			params.height = params.full_height = params.final_height = spec.height;
			params.full_x = params.full_y = 0;
			params.denoising_passes = true;
			params.selective_denoising = (passes & EX_TYPE_DENOISE_CLEAN);

			buffers = new RenderBuffers(device);
			buffers->reset(device, params);

			int4 rect = make_int4(0, 0, params.width, params.height);

			float *pass_data = new float[4*params.width*params.height];
			/* Read all the passes from the file. */
			for(map<PassTypeInfo, int3>::iterator i = channel_ids.begin(); i != channel_ids.end(); i++)
			{
				for(int c = 0; c < i->first.num_channels; c++) {
					int xstride = i->first.num_channels*sizeof(float);
					int ystride = params.width * xstride;
					printf("Reading pass %s!            \r", spec.channelnames[i->second[c]].c_str());
					fflush(stdout);
					frame->read_image(i->second[c], i->second[c]+1, TypeDesc::FLOAT, pass_data + c, xstride, ystride);
				}
				buffers->get_denoising_rect(i->first.type, 1.0f, options.session_params.samples, i->first.num_channels, rect, pass_data, true);
			}

			/* Read combined channel. */
			for(int i = 0; i < spec.nchannels; i++) {
				if(!split_channel(spec.channelnames[i], layer, pass, channel)) continue;
				if(layer != renderlayer || pass != "Combined") continue;

				size_t channel_id = find_channel("RGBA", channel);
				if(channel_id != 1) {
					int xstride = 4*sizeof(float);
					int ystride = params.width * xstride;
					printf("Reading pass %s!            \r", spec.channelnames[i].c_str());
					fflush(stdout);
					frame->read_image(i, i+1, TypeDesc::FLOAT, pass_data + channel_id, xstride, ystride);
				}
			}
			buffers->get_pass_rect(PASS_COMBINED, 1.0f, options.session_params.samples, 4, rect, pass_data, true);

			delete[] pass_data;

			buffers->copy_to_device();
		}
		else {
			printf("The frame is missing some pass!\n");
		}
	}
	else {
		printf("Didn't fine a suitable RenderLayer!\n");
	}

	frame->close();
	ImageInput::destroy(frame);

	return buffers;
}

bool cycles_denoising_session()
{
	options.session_params.only_denoise = true;
	options.session_params.progressive_refine = false;
	options.session_params.progressive = false;
	options.session_params.background = true;
	options.session_params.tile_order = TILE_BOTTOM_TO_TOP;

	options.session = new Session(options.session_params);
	options.session->progress.set_update_callback(function_bind(&session_print_status));
	options.session->set_pause(false);

	RenderBuffers *buffers = load_frame(options.filepaths[0], options.session->device);
	if(!buffers) {
		return false;
	}
	options.session->buffers = buffers;

	options.session->start_denoise();
	options.session->wait();

	/* Required for correct scaling of the output. */
	options.session->params.samples--;

	delete options.session;

	return true;
}

CCL_NAMESPACE_END