#include "jit.h"
#include "nodes.h"

CCL_NAMESPACE_BEGIN

JITShaderManager::JITShaderManager() {
}

JITShaderManager::~JITShaderManager() {
}

void JITShaderManager::reset(Scene *scene) {
}

void JITShaderManager::device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress) {
}

void JITShaderManager::device_free(Device *device, DeviceScene *dscene, Scene *scene) {
}

void JITCompiler::recurseNodeOutputs(ShaderOutput *output)
{
	/* output was already processed */
	if(needed_outputs.find(output) != needed_outputs.end()) {
		return;
	}

	std::string name = string_printf("var%d", var_id++);
	needed_outputs.insert(std::pair<ShaderOutput*, std::string>(output, name));

	/* another output on the same node was already processed */
	if(std::find(needed_nodes.begin(), needed_nodes.end(), output->parent) != needed_nodes.end()) {
		return;
	}

	/* Add all outputs/constants needed by this node */
	needed_nodes.push_back(output->parent);
	for(int i = 0; i < output->parent->inputs.size(); i++) {
		ShaderInput *input = output->parent->inputs[i];
		if(input->link) {
			recurseNodeOutputs(input->link);
		}
		else {
			name = string_printf("const%d", var_id++);
			generate_constant(input, name);
			needed_constants.insert(std::pair<ShaderInput*, std::string>(input, name));
		}
	}
}

void JITCompiler::recurseNodeCompile(ShaderNode *node)
{
	if(std::find(needed_nodes.begin(), needed_nodes.end(), node) == needed_nodes.end()) {
		assert(0);
		return;
	}

	for(int i = 0; i < node->inputs.size(); i++) {
		ShaderInput *input = node->inputs[i];
		if(input->link) {
			recurseNodeCompile(input->link->parent);
		}
	}
	node->compile(*this);
}

void JITCompiler::compile(ShaderGraph *graph)
{
	ShaderInput *surf = graph->output()->input("Surface");
	if(surf->link) {
		recurseNodeOutputs(surf->link);
		recurseNodeCompile(surf->link->parent);
	}
}

static std::string type_to_name(ShaderSocketType type)
{
	switch(type) {
		case SHADER_SOCKET_FLOAT:
			return "float";
		case SHADER_SOCKET_INT:
			return "int";
		case SHADER_SOCKET_COLOR:
		case SHADER_SOCKET_VECTOR:
		case SHADER_SOCKET_POINT:
		case SHADER_SOCKET_NORMAL:
			return "float3";
		case SHADER_SOCKET_CLOSURE:
			return "ShaderClosure*";
		default:
			assert(!"Unknown shader socket type!\n");
			return "";
	}
}

void JITCompiler::generate_output(ShaderOutput *output, std::string name)
{
	source << type_to_name(output->type) << " " << name << ";" << std::endl;
}

void JITCompiler::generate_constant(ShaderInput *input, std::string name)
{
	assert(!input->link);
	source << type_to_name(input->type) << " " << name << " = ";
	if(input->type == SHADER_SOCKET_COLOR || input->type == SHADER_SOCKET_VECTOR || input->type == SHADER_SOCKET_POINT || input->type == SHADER_SOCKET_NORMAL) {
		source << "make_float3(" << input->value.x << "f, " << input->value.y << "f, " << input->value.z << "f);" << std::endl;
	}
	else if(input->type == SHADER_SOCKET_INT) {
		source << (int) (input->value.x);
	}
	else if(input->type == SHADER_SOCKET_FLOAT) {
		source << input->value.x << "f;" << std::endl;
	}
	else {
		assert(!"Unknown shader socket type!\n");
	}
}

void JITCompiler::generate_call(std::string function)
{
	source << function << "(kg, sd, state";
	for(std::map<ShaderInput*, std::string>::iterator i = inputArgs.begin(); i != inputArgs.end(); i++) {
		source << ", " << i->second;
	}
	for(std::map<ShaderOutput*, std::string>::iterator i = outputArgs.begin(); i != outputArgs.end(); i++) {
		source << ", " << i->second;
	}
	source << ");" << std::endl;

	inputArgs.clear();
	outputArgs.clear();
}

void JITCompiler::add_input(ShaderInput *input)
{
	if(input->link) {
		std::map<ShaderOutput*, std::string>::iterator it = needed_outputs.find(input->link);
		assert(it != needed_outputs.end());

		inputArgs.insert(std::pair<ShaderInput*, std::string>(input, it->second));
	}
	else {
		std::map<ShaderInput*, std::string>::iterator it = needed_constants.find(input);
		assert(it != needed_constants.end());

		inputArgs.insert(std::pair<ShaderInput*, std::string>(input, it->second));
	}
}

void JITCompiler::add_output(ShaderOutput *output)
{
	std::map<ShaderOutput*, std::string>::iterator it = needed_outputs.find(output);
	if(it == needed_outputs.end()) {
		outputArgs.insert(std::pair<ShaderOutput*, std::string>(output, "NULL"));
	}
	else {
		generate_output(output, it->second);
		outputArgs.insert(std::pair<ShaderOutput*, std::string>(output, it->second));
	}
}

CCL_NAMESPACE_END
