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

#include "denoising.h"

#include "util_image.h"

CCL_NAMESPACE_BEGIN

bool cycles_denoising_session()
{
	vector<string> frames;
	if(options.frame_range.y >= options.frame_range.x) {
		string pattern = options.filepaths[0];
		size_t pos = pattern.find("%");
		if(options.filepaths.size() != 1 || pos == string::npos || pattern.size() <= pos+3 ||!isdigit(pattern[pos+1]) || pattern[pos+2] != 'd') {
			printf("ERROR: When using the frame range option, specify the image file as a single filename including %%Xd, there X is the length of the frame numbers.");
			delete options.session;
			return false;
		}

		char pad_length = pattern[pos+1];
		vector<string> new_filepaths;
		for(int frame = options.frame_range.x; frame <= options.frame_range.y; frame++) {
			string name = pattern.substr(0, pos);
			name += string_printf(string_printf("%%0%cd", pad_length).c_str(), frame);
			name += pattern.substr(pos+3);
			frames.push_back(name);
		}
	}

	return denoise_standalone(options.session_params, frames, options.denoise_frame);
}

CCL_NAMESPACE_END