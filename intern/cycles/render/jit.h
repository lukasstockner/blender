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

#ifndef __JIT_H__
#define __JIT_H__

#include "attribute.h"
#include "graph.h"
#include "shader.h"

#include "util_set.h"
#include "util_string.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class ImageManager;
class Scene;
class ShaderGraph;
class ShaderInput;
class ShaderNode;
class ShaderOutput;

/* Shader Manager */

class JITShaderManager : public ShaderManager {
public:
	JITShaderManager();
	~JITShaderManager();

	void reset(Scene *scene);

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene, Scene *scene);
};

/* Graph Compiler */

class JITCompiler {
public:
	JITCompiler(ShaderManager *shader_manager, ImageManager *image_manager);
	void compile(Scene *scene,
	             Shader *shader,
	             int index);

	ImageManager *image_manager;
	ShaderManager *shader_manager;
	bool background;

	void generate_call(std::string);
	void add_input(ShaderInput* input);
	void add_output(ShaderOutput* output);

protected:
	std::map<ShaderInput*, std::string> inputArgs;
	std::map<ShaderOutput*, std::string> outputArgs;

	std::vector<ShaderNode*> needed_nodes;
	std::map<ShaderOutput*, std::string> needed_outputs;
	std::map<ShaderInput*, std::string> needed_constants;

	std::stringstream source;

	int var_id;

	void recurseNodeOutputs(ShaderOutput* output);
	void recurseNodeCompile(ShaderNode* node);
	void compile(ShaderGraph*);

	void generate_output(ShaderOutput*, std::string);
	void generate_constant(ShaderInput*, std::string);
};

CCL_NAMESPACE_END

#endif /* __JIT_H__ */
