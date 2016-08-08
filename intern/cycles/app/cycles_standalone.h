/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __CYCLES_STANDALONE_H__
#define __CYCLES_STANDALONE_H__

#include "scene.h"
#include "session.h"

#include "util_vector.h"

CCL_NAMESPACE_BEGIN

struct Options {
	Session *session;
	Scene *scene;
	vector<string> filepaths;
	int width, height;
	SceneParams scene_params;
	SessionParams session_params;
	bool quiet;
	bool show_help, interactive, pause;
	int half_window;
	int denoise_frame;
};

extern Options options;
void session_print_status();

CCL_NAMESPACE_END

#endif /* __CYCLES_STANDALONE_H__ */
