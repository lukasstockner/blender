#include "render/denoising.h"

#include "kernel/filter/filter_defines.h"

#include "util/util_foreach.h"
#include "util/util_map.h"
#include "util/util_system.h"

#include <OpenImageIO/filesystem.h>

CCL_NAMESPACE_BEGIN

/* Define channel sets. */

struct ChannelMapping {
	int channel;
	string name;
};

static void fill_mapping(vector<ChannelMapping> &map, int pos, string name, string channels)
{
	for(const char *chan = channels.c_str(); *chan; chan++) {
		map.push_back({pos++, name+"."+*chan});
	}
}

static vector<ChannelMapping> init_denoising_channels()
{
	vector<ChannelMapping> map;
	fill_mapping(map, 0, "Denoising Normal", "XYZ");
	fill_mapping(map, 3, "Denoising Normal Variance", "XYZ");
	fill_mapping(map, 6, "Denoising Albedo", "RGB");
	fill_mapping(map, 9, "Denoising Albedo Variance", "RGB");
	fill_mapping(map, 12, "Denoising Depth", "Z");
	fill_mapping(map, 13, "Denoising Depth Variance", "Z");
	fill_mapping(map, 14, "Denoising Shadow A", "XYV");
	fill_mapping(map, 17, "Denoising Shadow B", "XYV");
	fill_mapping(map, 20, "Denoising Image", "RGB");
	fill_mapping(map, 23, "Denoising Image Variance", "RGB");
	return map;
}

static vector<ChannelMapping> init_prefiltered_channels()
{
	vector<ChannelMapping> map;
	fill_mapping(map, 0, "Denoising Depth", "Z");
	fill_mapping(map, 1, "Denoising Normal", "XYZ");
	fill_mapping(map, 4, "Denoising Shadowing", "X");
	fill_mapping(map, 5, "Denoising Albedo", "RGB");
	fill_mapping(map, 8, "Denoising Image", "RGB");
	fill_mapping(map, 11, "Denoising Image Variance", "RGB");
	return map;
}

static vector<ChannelMapping> init_final_channels()
{
	vector<ChannelMapping> map;
	fill_mapping(map, 0, "Combined", "RGB");
	return map;
}

static const vector<ChannelMapping> denoising_channels = init_denoising_channels();
static const vector<ChannelMapping> prefiltered_channels = init_prefiltered_channels();
static const vector<ChannelMapping> final_channels = init_final_channels();




/* Splits in at its last dot, setting suffix to the part after the dot and in to the part before it.
 * Returns whether a dot was found. */
static bool split_last_dot(string &in, string &suffix)
{
	size_t pos = in.rfind(".");
	if(pos == string::npos) return false;
	suffix = in.substr(pos+1);
	in = in.substr(0, pos);
	return true;
}

void StandaloneDenoiser::RenderLayer::detect_sets(bool prefilter)
{
	/* If we're filtering, first check whether there is a full set of prefiltered channels.
	 * If we're prefiltering, ignore them. */
	if(!prefilter) {
		buffer_offsets.clear();
		buffer_offsets.resize(channels.size(), -1);
		has_prefiltered = true;

		/* Try to find the offset of each channel that is part of a prefiltered set. */
		foreach(const ChannelMapping& mapping, prefiltered_channels) {
			vector<string>::iterator i = find(channels.begin(), channels.end(), mapping.name);
			if(i == channels.end()) {
				has_prefiltered = false;
				break;
			}
			buffer_offsets[i - channels.begin()] = mapping.channel;
		}

		/* If we found a prefiltered set, there's no need to try to find a regular denoising data set. */
		if(has_prefiltered) {
			has_denoising = false;
			return;
		}
	}
	else {
		has_prefiltered = false;
	}

	buffer_offsets.clear();
	buffer_offsets.resize(channels.size(), -1);
	has_denoising = true;

	/* Try to find the offset of each pass that is part of a denoising data set. */
	foreach(const ChannelMapping& mapping, denoising_channels) {
		vector<string>::iterator i = find(channels.begin(), channels.end(), mapping.name);
		if(i == channels.end()) {
			has_denoising = false;
			break;
		}
		buffer_offsets[i - channels.begin()] = mapping.channel;
	}

	if(!has_denoising) {
		/* Didn't find either set, so we don't need to load any channels from this layer. */
		buffer_offsets.clear();
		buffer_offsets.resize(channels.size(), -1);
	}
}

void StandaloneDenoiser::RenderLayer::generate_map(int buffer_nchannels)
{
	assert(has_denoising || has_prefiltered);

	buffer_to_file_map.clear();
	buffer_to_file_map.resize(buffer_nchannels, -1);

	for(int i = 0; i < channels.size(); i++) {
		if(buffer_offsets[i] >= 0) {
			buffer_to_file_map[buffer_offsets[i]] = file_offsets[i];
		}
	}

	/* Check that all buffer channels are correctly set. */
	for(int i = 0; i < buffer_nchannels; i++) {
		assert(buffer_to_file_map[i] >= 0);
	}
}

bool StandaloneDenoiser::RenderLayer::match_channels(int frame,
                                                     const vector<string> &main_channelnames,
                                                     const std::vector<string> &frame_channelnames)
{
	assert(frame_buffer_to_file_map.count(frame) == 0);

	int buffer_nchannels = buffer_to_file_map.size();

	vector<int> frame_map(buffer_nchannels, -1);
	for(int i = 0; i < buffer_to_file_map.size(); i++) {
		string channel = main_channelnames[buffer_to_file_map[i]];

		std::vector<string>::const_iterator frame_channel = find(frame_channelnames.begin(), frame_channelnames.end(), channel);

		if(frame_channel == frame_channelnames.end()) {
			printf("Didn't find channel %s in frame %d!\n", channel.c_str(), frame);
			return false;
		}

		frame_map[i] = frame_channel - frame_channelnames.begin();
	}

	frame_buffer_to_file_map[frame] = frame_map;
	return true;
}

bool StandaloneDenoiser::parse_channel_name(string name, string &renderlayer, string &pass, string &channel)
{
	if(!split_last_dot(name, channel)) return false;
	string view;
	if(views) {
		if(!split_last_dot(name, view)) return false;
	}
	if(!split_last_dot(name, pass)) return false;
	renderlayer = name;

	if(views) {
		renderlayer += "."+view;
	}

	return true;
}

void StandaloneDenoiser::parse_channels(const ImageSpec &in_spec)
{
	const std::vector<string> &channels = in_spec.channelnames;

	out_passthrough.clear();
	out_channels.clear();
	layers.clear();

	/* Loop over all the channels in the file, parse their name and sort them
	 * by RenderLayer.
	 * Channels that can't be parsed are directly passed through to the output. */
	map<string, RenderLayer> file_layers;
	for(int i = 0; i < channels.size(); i++) {
		in_channels.push_back(channels[i]);
		string layer, pass, channel;
		if(!parse_channel_name(channels[i], layer, pass, channel)) {
			out_passthrough.push_back(i);
			out_channels.push_back(channels[i]);
			printf("Couldn't decode channel name %s, passing through to output!\n", channels[i].c_str());
			continue;
		}
		file_layers[layer].channels.push_back(pass+"."+channel);
		file_layers[layer].file_offsets.push_back(i);
	}

	/* Loop over all detected RenderLayers, check whether they contain a full set of channels
	 * of either denoising data or prefiltered data.
	 * Any channels that won't be processed internally are also passed through. */
	for(map<string, RenderLayer>::iterator i = file_layers.begin(); i != file_layers.end(); ++i) {
		RenderLayer &layer = i->second;
		/* Check for full pass set. */
		layer.detect_sets(prefilter);

		/* If we're prefiltering, we only want regular denoising data.
		 * For actual filtering, prefiltered data is fine as well. */
		bool process_layer = false;
		if(layer.has_denoising && !prefilter) {
			printf("Directly denoising isn't supported yet, please prefilter the input first!\n");
		}
		if((layer.has_denoising && prefilter) || (layer.has_prefiltered && !prefilter)) {
			layer.samples = samples;
			/* If the sample value isn't set yet, check if there is a layer-specific one in the input file. */
			if(layer.samples < 1) {
				string sample_string = in_spec.get_string_attribute("Cycles Samples " + i->first, "");
				if(sample_string != "") {
					if(!sscanf(sample_string.c_str(), "%d", &layer.samples)) {
						printf("Failed to parse samples metadata %s!\n", sample_string.c_str());
					}
				}
			}
			if(layer.samples < 1) {
				printf("No sample number specified in the file for layer %s or on the command line!\n", i->first.c_str());
				continue;
			}

			layer.generate_map(buffer_pass_stride);
			layer.name = i->first;
			process_layer = true;
			layers.push_back(layer);
		}

		/* Detect unused passes. */
		for(int j = 0; j < layer.buffer_offsets.size(); j++) {
			if(layer.buffer_offsets[j] == -1) {
				/* Special case: Denoising replaces the Combined.RGB channels, so don't pass it through as well. */
				if(!prefilter && process_layer &&
				   layer.channels[j].substr(0, 9) == "Combined." &&
				   layer.channels[j] != "Combined.A") {
					continue;
				}
				out_passthrough.push_back(layer.file_offsets[j]);
				out_channels.push_back(channels[layer.file_offsets[j]]);
			}
		}
	}

	for(int i = 0; i < layers.size(); i++) {
		/* Determine output channel names. */
		string renderlayer = layers[i].name;
		string view = "";
		if(views) {
			split_last_dot(renderlayer, view);
			view = "."+view;
		}
		const vector<ChannelMapping> &map = prefilter? prefiltered_channels : final_channels;
		layers[i].out_results.clear();
		layers[i].out_results.resize(map.size());
		for(int j = 0; j < map.size(); j++) {
			layers[i].out_results[map[j].channel] = out_channels.size();
			string pass = map[j].name, channel;
			split_last_dot(pass, channel);
			out_channels.push_back(renderlayer+"."+pass+view+"."+channel);
		}
	}
}

bool StandaloneDenoiser::load_file(ImageInput *in_image, const vector<int> &buffer_to_file_map, float *mem, bool write_out, int y0, int y1)
{
	/* Read all channels of the file into temporary memory.
	 * Reading all at once and then shuffling in memory is faster than reading each channel individually. */
	float *tempmem = new float[width*(y1 - y0)*nchannels];
	if(!in_image->read_scanlines(y0, y1, 0, TypeDesc::FLOAT, tempmem)) {
		delete[] tempmem;
		return false;
	}

	/* Shuffle image data into correct channels. */
	mem += y0*buffer_pass_stride*width;
	for(int i = 0; i < (y1-y0)*width; i++) {
		for(int j = 0; j < buffer_pass_stride; j++) {
			mem[i*buffer_pass_stride + j] = tempmem[i*nchannels + buffer_to_file_map[j]];
		}
	}

	if(write_out) {
		/* Passthrough unused channels directly into the output frame. */
		int passthrough_channels = out_passthrough.size();
		float *out = out_buffer + y0*out_pass_stride*width;
		for(int i = 0; i < (y1-y0)*width; i++) {
			for(int j = 0; j < passthrough_channels; j++) {
				out[i*out_pass_stride + j] = tempmem[i*nchannels + out_passthrough[j]];
			}
		}
	}

	delete[] tempmem;
	return true;
}

static void print_progress(int num, int total)
{
#ifdef WIN32
	/* Broken on Windows :( */
	int cols = 80;
#else
	int rows, cols;
	system_console_shape(&rows, &cols);
#endif

	cols -= 1;

	int len = 1;
	for(int x = total; x > 9; x /= 10) len++;

	int bars = cols - 2*len - 6;

	int v = int(float(num)*bars/total);
	printf("\r[");
	for(int i = 0; i < v; i++) printf("=");
	if(v < bars) printf(">");
	for(int i = v+1; i < bars; i++) printf(" ");
	printf(string_printf("] %%%dd / %d", len, total).c_str(), num);
	fflush(stdout);

}

bool StandaloneDenoiser::acquire_tile(Device *device, Device *tile_device, RenderTile &tile)
{
	thread_scoped_lock tile_lock(tiles_mutex);

	if(tiles.empty()) {
		return false;
	}

	tile = tiles.front();
	tiles.pop_front();

	device->map_tile(tile_device, tile);

	print_progress(num_tiles - tiles.size(), num_tiles);

	return true;
}

/* Mapping tiles is required for regular rendering since each tile has its separate memory
 * which may be allocated on a different device.
 * For standalone denoising, there is a single memory that is present on all devices, so the only
 * thing that needs to be done here is to specify the surrounding tile geometry.
 *
 * However, since there is only one large memory, the denoised result has to be written to
 * a different buffer to avoid having to copy an entire horizontal slice of the image. */
void StandaloneDenoiser::map_neighboring_tiles(RenderTile *tiles, Device *tile_device)
{
	for(int i = 0; i < 9; i++) {
		if(i == 4) {
			continue;
		}

		int dx = (i%3)-1;
		int dy = (i/3)-1;
		tiles[i].x = clamp(tiles[4].x +  dx   *tile_size.x, 0,  width);
		tiles[i].w = clamp(tiles[4].x + (dx+1)*tile_size.x, 0,  width) - tiles[i].x;
		tiles[i].y = clamp(tiles[4].y +  dy   *tile_size.y, 0, height);
		tiles[i].h = clamp(tiles[4].y + (dy+1)*tile_size.y, 0, height) - tiles[i].y;

		tiles[i].buffer = tiles[4].buffer;
		tiles[i].offset = tiles[4].offset;
		tiles[i].stride = width;
	}

	device_vector<float> *target_mem = new device_vector<float>(tile_device, "denoising_target", MEM_READ_WRITE);
	target_mem->alloc(target_pass_stride*tiles[4].w*tiles[4].h);
	target_mem->zero_to_device();

	tiles[9] = tiles[4];
	tiles[9].buffer = target_mem->device_pointer;
	tiles[9].stride = tiles[9].w;
	tiles[9].offset -= tiles[9].x + tiles[9].y*tiles[9].stride;

	thread_scoped_lock target_lock(targets_mutex);
	assert(target_mems.count(tiles[4].tile_index) == 0);
	target_mems[tiles[9].tile_index] = target_mem;
}

void StandaloneDenoiser::unmap_neighboring_tiles(RenderTile *tiles)
{
	thread_scoped_lock target_lock(targets_mutex);
	assert(target_mems.count(tiles[4].tile_index) == 1);
	device_vector<float> *target_mem = target_mems[tiles[9].tile_index];
	target_mems.erase(tiles[4].tile_index);
	target_lock.unlock();

	target_mem->copy_from_device(0, target_pass_stride*tiles[9].w, tiles[9].h);

	float *result = target_mem->data();
	float *out = out_buffer + out_pass_stride*(tiles[9].y*width + tiles[9].x);
	for(int y = 0; y < tiles[9].h; y++) {
		for(int x = 0; x < tiles[9].w; x++, result += target_pass_stride) {
			for(int i = 0; i < target_pass_stride; i++) {
				out[out_pass_stride*x + layers[current_layer].out_results[i]] = result[i];
			}
		}
		out += out_pass_stride*width;
	}

	target_mem->free();
	delete target_mem;
}

void StandaloneDenoiser::release_tile()
{
}

bool StandaloneDenoiser::get_cancel()
{
	return false;
}

DeviceTask StandaloneDenoiser::create_task()
{
	DeviceTask task(DeviceTask::RENDER);
	task.acquire_tile = function_bind(&StandaloneDenoiser::acquire_tile, this, device, _1, _2);
	task.map_neighbor_tiles = function_bind(&StandaloneDenoiser::map_neighboring_tiles, this, _1, _2);
	task.unmap_neighbor_tiles = function_bind(&StandaloneDenoiser::unmap_neighboring_tiles, this, _1);
	task.release_tile = function_bind(&StandaloneDenoiser::release_tile, this);
	task.get_cancel = function_bind(&StandaloneDenoiser::get_cancel, this);
	task.denoising_radius = radius;
	task.denoising_feature_strength = feature_strength;
	task.denoising_strength = strength;
	task.denoising_relative_pca = relative_pca;
	task.pass_stride = buffer_pass_stride;
	task.target_pass_stride = target_pass_stride;
	task.pass_denoising_data = 0;
	task.pass_denoising_clean = -1;
	task.denoising_type = prefilter? DeviceTask::DENOISE_PREFILTER : DeviceTask::DENOISE_FILTER;
	task.denoising_from_render = false;
	task.denoising_frames = frames;
	task.denoising_frame_stride = buffer_frame_stride;

	thread_scoped_lock tile_lock(tiles_mutex);
	thread_scoped_lock targets_lock(targets_mutex);

	tiles.clear();
	target_mems.clear();

	int tiles_x = divide_up(width, tile_size.x), tiles_y = divide_up(height, tile_size.y);
	for(int ty = 0; ty < tiles_y; ty++) {
		for(int tx = 0; tx < tiles_x; tx++) {
			RenderTile tile;
			tile.x = tx * tile_size.x;
			tile.y = ty * tile_size.y;
			tile.w = min(width - tile.x, tile_size.x);
			tile.h = min(height - tile.y, tile_size.y);
			tile.start_sample = 0;
			tile.num_samples = layers[current_layer].samples;
			tile.sample = 0;
			tile.offset = 0;
			tile.stride = width;
			tile.tile_index = ty*tiles_x + tx;
			tile.task = RenderTile::DENOISE;
			tile.buffers = NULL;
			tile.buffer = buffer.device_pointer;
			tiles.push_back(tile);
		}
	}
	num_tiles = tiles.size();

	return task;
}

bool StandaloneDenoiser::open_frames(string in_filename, string out_filename)
{
	if(!Filesystem::is_regular(in_filename)) {
		error = "Couldn't find a file called " + in_filename + "!";
		return false;
	}

	in = ImageInput::open(in_filename);
	if(!in) {
		error = "Couldn't open " + in_filename + "!";
		return false;
	}

	const ImageSpec &in_spec = in->spec();
	width = in_spec.width;
	height = in_spec.height;
	nchannels = in_spec.nchannels;

	/* If the sample value isn't set already, check whether the input file contains it. */
	if(samples < 1) {
		string sample_string = in_spec.get_string_attribute("Cycles Samples", "");
		if(sample_string != "") {
			if(!sscanf(sample_string.c_str(), "%d", &samples)) {
				printf("Failed to parse samples metadata %s!\n", sample_string.c_str());
			}
		}
	}

	parse_channels(in_spec);

	if(layers.size() == 0) {
		error = "Didn't find a RenderLayer containing denoising info!";
		return false;
	}

	out = ImageOutput::create(out_filename);
	if(!out) {
		error = "Couldn't open " + out_filename + "!";
		return false;
	}

	ImageSpec out_spec(width, height, out_channels.size(), TypeDesc::FLOAT);
	out_spec.channelnames.clear();

	/* Set the channels of the output image based on the input parsing result. */
	foreach(string channel, out_channels) {
		out_spec.channelnames.push_back(channel);
	}

	/* Pass through the attributes of the input frame. */
	out_spec.extra_attribs = in_spec.extra_attribs;

	out->open(out_filename, out_spec);

	out_pass_stride = out_channels.size();
	out_buffer = new float[width*height*out_pass_stride];

	return true;
}

bool StandaloneDenoiser::run_filter()
{
	prefilter = false;
	buffer_pass_stride = 14;
	target_pass_stride = 3;
	frames.clear();

	DeviceRequestedFeatures req;
	device->load_kernels(req);

	string pattern, framestring;
	if(!Filesystem::parse_pattern(in_path.c_str(), 0, pattern, framestring)) {
		error = "Malformed input pattern!";
		return false;
	}
	std::vector<int> frame_range;
	if(framestring != "") {
		if(!Filesystem::enumerate_sequence(framestring, frame_range)) {
			printf("Couldn't parse frame sequence %s, falling back to regular interval!\n", framestring.c_str());
		}
	}
	if(frame_range.size() == 0) {
		for(int f = center_frame - frame_radius; f <= center_frame + frame_radius; f++) {
			frame_range.push_back(f);
		}
	}

	std::vector<int>::iterator center_frame_iter = find(frame_range.begin(), frame_range.end(), center_frame);
	if(center_frame_iter != frame_range.end()) {
		frame_range.erase(center_frame_iter);
	}

	string out_filename = out_path;
	string out_framestring;
	if(Filesystem::parse_pattern(out_path.c_str(), 0, out_filename, out_framestring)) {
		out_filename = string_printf(out_filename.c_str(), center_frame);
		if(out_framestring != "") {
			printf("Ignoring output file frame range!\n");
		}
	}

	string center_filename = string_printf(pattern.c_str(), center_frame);
	if(!open_frames(center_filename, out_filename)) {
		return false;
	}

	vector<ImageInput*> in_frames;
	foreach(int frame, frame_range) {
		string filename = string_printf(pattern.c_str(), frame);

		if(!Filesystem::is_regular(filename)) {
			printf("Couldn't find frame %s, skipping...\n", filename.c_str());
			continue;
		}

		ImageInput *in_frame = ImageInput::open(filename);
		if(!in_frame) {
			printf("Couldn't open frame %s, skipping...\n", filename.c_str());
			continue;
		}

		const ImageSpec &in_spec = in_frame->spec();
		if(in_spec.width != width || in_spec.height != height) {
			printf("Frame %s has wrong dimensions, skipping...\n", filename.c_str());
			in_frame->close();
			ImageInput::destroy(in_frame);
			continue;
		}

		bool missing_channels = false;
		for(current_layer = 0; current_layer < layers.size(); current_layer++) {
			if(!layers[current_layer].match_channels(frame, in_channels, in_spec.channelnames)) {
				missing_channels = true;
				break;
			}
		}
		if(missing_channels) {
			printf("Frame %s misses channels, skipping...\n", filename.c_str());
			in_frame->close();
			ImageInput::destroy(in_frame);
			continue;
		}

		in_frames.push_back(in_frame);
		frames.push_back(frame);

		if(frames.size() == MAX_SECONDARY_FRAMES) {
			printf("Reached maximum of %d secondary frames, will skip following ones...\n", MAX_SECONDARY_FRAMES);
			break;
		}
	}

	num_frames = in_frames.size()+1;
	buffer_frame_stride = width*height*buffer_pass_stride;

	buffer.alloc(width*buffer_pass_stride, height*num_frames);
	buffer.zero_to_device();

	for(current_layer = 0; current_layer < layers.size(); current_layer++) {
		/* ToDo: Interleave processing and striped loading. */
		load_file(in,
		          layers[current_layer].buffer_to_file_map,
		          buffer.data(),
		          current_layer == 0,
		          0, height);
		for(int i = 0; i < in_frames.size(); i++) {
			load_file(in_frames[i],
			          layers[current_layer].frame_buffer_to_file_map[frames[i]],
			          buffer.data() + (i+1)*buffer_frame_stride,
			          false,
			          0, height);
		}

		buffer.copy_to_device();

		DeviceTask task = create_task();

		device->task_add(task);
		device->task_wait();
		printf("\n");
	}

	out->write_image(TypeDesc::FLOAT, out_buffer);

	free();

	return true;
}

bool StandaloneDenoiser::run_prefilter()
{
	prefilter = true;
	buffer_pass_stride = 26;
	target_pass_stride = 14;
	frames.clear();

	DeviceRequestedFeatures req;
	device->load_kernels(req);

	std::vector<string> in_filenames, out_filenames;
	std::vector<int> frames;
	string pattern, framestring;
	if(Filesystem::parse_pattern(in_path.c_str(), 0, pattern, framestring)) {
		assert(pattern.find("%") != string::npos);
		if(framestring == "") {
			if(!Filesystem::scan_for_matching_filenames(pattern, frames, in_filenames)) {
				error = "Couldn't search for files matching "+pattern+"!";
				return false;
			}
			if(frames.size() == 0) {
				error = "Didn't find any files matching "+pattern+"!";
				return false;
			}
		}
		else {
			if(!Filesystem::enumerate_sequence(framestring, frames)) {
				error = "Couldn't parse frame sequence "+framestring+"!";
				return false;
			}
			if(!Filesystem::enumerate_file_sequence(pattern, frames, in_filenames)) {
				error = "Couldn't enumerate file sequence "+pattern+"!";
				return false;
			}
		}

		if (!Filesystem::parse_pattern(out_path.c_str(), 0, pattern, framestring)) {
			error = "Malformed output pattern!";
			return false;
		}
		if (framestring != "") {
			printf("Ignoring output framestring!\n");
		}

		if (pattern.find("%") == string::npos) {
			error = "If an input pattern is given, the output also needs to be one!";
			return false;
		}

		if (!Filesystem::enumerate_file_sequence(pattern, frames, out_filenames)) {
			error = "Couldn't enumerate file sequence " + pattern + "!";
			return false;
		}
	}
	else {
		frames.push_back(0);
		in_filenames.push_back(in_path);
		out_filenames.push_back(out_path);
	}

	for(int i = 0; i < frames.size(); i++) {
		printf("Prefiltering %s into %s!\n", in_filenames[i].c_str(), out_filenames[i].c_str());

		if(!open_frames(in_filenames[i], out_filenames[i])) {
			return false;
		}

		buffer.alloc(width*buffer_pass_stride, height);
		buffer.zero_to_device();
		for(current_layer = 0; current_layer < layers.size(); current_layer++) {
			/* ToDo: Interleave processing and striped loading. */
			load_file(in,
			          layers[current_layer].buffer_to_file_map,
			          buffer.data(),
			          current_layer == 0,
			          0, height);

			buffer.copy_to_device();

			DeviceTask task = create_task();

			device->task_add(task);
			device->task_wait();
			printf("\n");
		}

		out->write_image(TypeDesc::FLOAT, out_buffer);

		free();
	}

	return true;
}

void StandaloneDenoiser::free()
{
	buffer.free();

	delete[] out_buffer;
	out_buffer = NULL;

	if(in) {
		in->close();
		ImageInput::destroy(in);
		in = NULL;
	}

	if(out) {
		out->close();
		ImageOutput::destroy(out);
		out = NULL;
	}
}

CCL_NAMESPACE_END