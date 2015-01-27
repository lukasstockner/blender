/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Blender Foundation (2015)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_asset.c
 *  \ingroup RNA
 */

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "BKE_asset.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "BKE_asset.h"
#include "BKE_context.h"
#include "BKE_report.h"

/* RenderEngine Callbacks */
#if 0
static void engine_tag_redraw(RenderEngine *engine)
{
	engine->flag |= RE_ENGINE_DO_DRAW;
}

static void engine_tag_update(RenderEngine *engine)
{
	engine->flag |= RE_ENGINE_DO_UPDATE;
}

static int engine_support_display_space_shader(RenderEngine *UNUSED(engine), Scene *scene)
{
	return IMB_colormanagement_support_glsl_draw(&scene->view_settings);
}

static void engine_bind_display_space_shader(RenderEngine *UNUSED(engine), Scene *scene)
{
	IMB_colormanagement_setup_glsl_draw(&scene->view_settings,
	                                    &scene->display_settings,
	                                    scene->r.dither_intensity,
	                                    false);
}

static void engine_unbind_display_space_shader(RenderEngine *UNUSED(engine))
{
	IMB_colormanagement_finish_glsl_draw();
}

static void engine_update(RenderEngine *engine, Main *bmain, Scene *scene)
{
	extern FunctionRNA rna_RenderEngine_update_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_update_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "data", &bmain);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_render(RenderEngine *engine, struct Scene *scene)
{
	extern FunctionRNA rna_RenderEngine_render_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_render_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_bake(RenderEngine *engine, struct Scene *scene, struct Object *object, const int pass_type,
                        const struct BakePixel *pixel_array, const int num_pixels, const int depth, void *result)
{
	extern FunctionRNA rna_RenderEngine_bake_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_bake_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "scene", &scene);
	RNA_parameter_set_lookup(&list, "object", &object);
	RNA_parameter_set_lookup(&list, "pass_type", &pass_type);
	RNA_parameter_set_lookup(&list, "pixel_array", &pixel_array);
	RNA_parameter_set_lookup(&list, "num_pixels", &num_pixels);
	RNA_parameter_set_lookup(&list, "depth", &depth);
	RNA_parameter_set_lookup(&list, "result", &result);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_view_update(RenderEngine *engine, const struct bContext *context)
{
	extern FunctionRNA rna_RenderEngine_view_update_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_view_update_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &context);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_view_draw(RenderEngine *engine, const struct bContext *context)
{
	extern FunctionRNA rna_RenderEngine_view_draw_func;
	PointerRNA ptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	func = &rna_RenderEngine_view_draw_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "context", &context);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}

static void engine_update_script_node(RenderEngine *engine, struct bNodeTree *ntree, struct bNode *node)
{
	extern FunctionRNA rna_RenderEngine_update_script_node_func;
	PointerRNA ptr, nodeptr;
	ParameterList list;
	FunctionRNA *func;

	RNA_pointer_create(NULL, engine->type->ext.srna, engine, &ptr);
	RNA_pointer_create((ID *)ntree, &RNA_Node, node, &nodeptr);
	func = &rna_RenderEngine_update_script_node_func;

	RNA_parameter_list_create(&list, &ptr, func);
	RNA_parameter_set_lookup(&list, "node", &nodeptr);
	engine->type->ext.call(NULL, &ptr, func, &list);

	RNA_parameter_list_free(&list);
}
#endif

/* RenderEngine registration */

static void rna_AssetEngine_unregister(Main *UNUSED(bmain), StructRNA *type)
{
	AssetEngineType *aet = RNA_struct_blender_type_get(type);

	if (!aet) {
		return;
	}
	
	RNA_struct_free_extension(type, &aet->ext);
	BLI_freelinkN(&asset_engines, aet);
	RNA_struct_free(&BLENDER_RNA, type);
}

static StructRNA *rna_AssetEngine_register(Main *bmain, ReportList *reports, void *data, const char *identifier,
                                           StructValidateFunc validate, StructCallbackFunc call, StructFreeFunc free)
{
	AssetEngineType *aet, dummyaet = {NULL};
	AssetEngine dummyengine = {NULL};
	PointerRNA dummyptr;
	int have_function[6];

	/* setup dummy engine & engine type to store static properties in */
	dummyengine.type = &dummyaet;
	RNA_pointer_create(NULL, &RNA_AssetEngine, &dummyengine, &dummyptr);

	/* validate the python class */
	if (validate(&dummyptr, data, have_function) != 0) {
		return NULL;
	}

	if (strlen(identifier) >= sizeof(dummyaet.idname)) {
		BKE_reportf(reports, RPT_ERROR, "Registering asset engine class: '%s' is too long, maximum length is %d",
		            identifier, (int)sizeof(dummyaet.idname));
		return NULL;
	}

	/* Check if we have registered this engine type before, and remove it. */
	aet = BLI_rfindstring(&asset_engines, dummyaet.idname, offsetof(AssetEngineType, idname));
	if (aet && aet->ext.srna) {
		rna_AssetEngine_unregister(bmain, aet->ext.srna);
	}
	
	/* Create a new engine type. */
	aet = MEM_callocN(sizeof(AssetEngineType), __func__);
	memcpy(aet, &dummyaet, sizeof(*aet));

	aet->ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, aet->idname, &RNA_AssetEngine);
	aet->ext.data = data;
	aet->ext.call = call;
	aet->ext.free = free;
	RNA_struct_blender_type_set(aet->ext.srna, aet);

#if 0
	aet->update = (have_function[0]) ? engine_update : NULL;
	aet->render = (have_function[1]) ? engine_render : NULL;
	aet->bake = (have_function[2]) ? engine_bake : NULL;
	aet->view_update = (have_function[3]) ? engine_view_update : NULL;
	aet->view_draw = (have_function[4]) ? engine_view_draw : NULL;
	aet->update_script_node = (have_function[5]) ? engine_update_script_node : NULL;
#endif

	BLI_addtail(&asset_engines, aet);

	return aet->ext.srna;
}

static void **rna_AssetEngine_instance(PointerRNA *ptr)
{
	AssetEngine *engine = ptr->data;
	return &engine->py_instance;
}

static StructRNA *rna_AssetEngine_refine(PointerRNA *ptr)
{
	AssetEngine *engine = ptr->data;
	return (engine->type && engine->type->ext.srna) ? engine->type->ext.srna : &RNA_AssetEngine;
}

#if 0
static PointerRNA rna_RenderEngine_render_get(PointerRNA *ptr)
{
	RenderEngine *engine = (RenderEngine *)ptr->data;

	if (engine->re) {
		RenderData *r = RE_engine_get_render_data(engine->re);

		return rna_pointer_inherit_refine(ptr, &RNA_RenderSettings, r);
	}
	else {
		return rna_pointer_inherit_refine(ptr, &RNA_RenderSettings, NULL);
	}
}

static void rna_RenderResult_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderResult *rr = (RenderResult *)ptr->data;
	rna_iterator_listbase_begin(iter, &rr->layers, NULL);
}

static void rna_RenderLayer_passes_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	rna_iterator_listbase_begin(iter, &rl->passes, NULL);
}

static int rna_RenderLayer_rect_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	RenderLayer *rl = (RenderLayer *)ptr->data;

	length[0] = rl->rectx * rl->recty;
	length[1] = 4;

	return length[0] * length[1];
}

static void rna_RenderLayer_rect_get(PointerRNA *ptr, float *values)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	memcpy(values, rl->rectf, sizeof(float) * rl->rectx * rl->recty * 4);
}

void rna_RenderLayer_rect_set(PointerRNA *ptr, const float *values)
{
	RenderLayer *rl = (RenderLayer *)ptr->data;
	memcpy(rl->rectf, values, sizeof(float) * rl->rectx * rl->recty * 4);
}

static int rna_RenderPass_rect_get_length(PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION])
{
	RenderPass *rpass = (RenderPass *)ptr->data;

	length[0] = rpass->rectx * rpass->recty;
	length[1] = rpass->channels;

	return length[0] * length[1];
}

static void rna_RenderPass_rect_get(PointerRNA *ptr, float *values)
{
	RenderPass *rpass = (RenderPass *)ptr->data;
	memcpy(values, rpass->rect, sizeof(float) * rpass->rectx * rpass->recty * rpass->channels);
}

void rna_RenderPass_rect_set(PointerRNA *ptr, const float *values)
{
	RenderPass *rpass = (RenderPass *)ptr->data;
	memcpy(rpass->rect, values, sizeof(float) * rpass->rectx * rpass->recty * rpass->channels);
}

static PointerRNA rna_BakePixel_next_get(PointerRNA *ptr)
{
	BakePixel *bp = ptr->data;
	return rna_pointer_inherit_refine(ptr, &RNA_BakePixel, bp + 1);
}
#endif

#else /* RNA_RUNTIME */

static void rna_def_asset_engine(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "AssetEngine", NULL);
	RNA_def_struct_sdna(srna, "AssetEngine");
	RNA_def_struct_ui_text(srna, "Asset Engine", "An assets manager");
	RNA_def_struct_refine_func(srna, "rna_AssetEngine_refine");
	RNA_def_struct_register_funcs(srna, "rna_AssetEngine_register", "rna_AssetEngine_unregister",
	                              "rna_AssetEngine_instance");

#if 0
	/* final render callbacks */
	func = RNA_def_function(srna, "update", NULL);
	RNA_def_function_ui_description(func, "Export scene data for render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "data", "BlendData", "", "");
	RNA_def_pointer(func, "scene", "Scene", "", "");

	func = RNA_def_function(srna, "render", NULL);
	RNA_def_function_ui_description(func, "Render scene into an image");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "scene", "Scene", "", "");

	func = RNA_def_function(srna, "bake", NULL);
	RNA_def_function_ui_description(func, "Bake passes");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	prop = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_pointer(func, "object", "Object", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_enum(func, "pass_type", render_pass_type_items, 0, "Pass", "Pass to bake");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_pointer(func, "pixel_array", "BakePixel", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "num_pixels", 0, 0, INT_MAX, "Number of Pixels", "Size of the baking batch", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "depth", 0, 0, INT_MAX, "Pixels depth", "Number of channels", 1, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	/* TODO, see how array size of 0 works, this shouldnt be used */
	prop = RNA_def_pointer(func, "result", "AnyType", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	/* viewport render callbacks */
	func = RNA_def_function(srna, "view_update", NULL);
	RNA_def_function_ui_description(func, "Update on data changes for viewport render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	RNA_def_pointer(func, "context", "Context", "", "");

	func = RNA_def_function(srna, "view_draw", NULL);
	RNA_def_function_ui_description(func, "Draw viewport render");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
	RNA_def_pointer(func, "context", "Context", "", "");

	/* shader script callbacks */
	func = RNA_def_function(srna, "update_script_node", NULL);
	RNA_def_function_ui_description(func, "Compile shader script node");
	RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
	prop = RNA_def_pointer(func, "node", "Node", "", "");
	RNA_def_property_flag(prop, PROP_RNAPTR);

	/* tag for redraw */
	func = RNA_def_function(srna, "tag_redraw", "engine_tag_redraw");
	RNA_def_function_ui_description(func, "Request redraw for viewport rendering");

	/* tag for update */
	func = RNA_def_function(srna, "tag_update", "engine_tag_update");
	RNA_def_function_ui_description(func, "Request update call for viewport rendering");

	func = RNA_def_function(srna, "begin_result", "RE_engine_begin_result");
	RNA_def_function_ui_description(func, "Create render result to write linear floating point render layers and passes");
	prop = RNA_def_int(func, "x", 0, 0, INT_MAX, "X", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "y", 0, 0, INT_MAX, "Y", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "w", 0, 0, INT_MAX, "Width", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_int(func, "h", 0, 0, INT_MAX, "Height", "", 0, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	RNA_def_string(func, "layer", NULL, 0, "Layer", "Single layer to get render result for");  /* NULL ok here */
	prop = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_function_return(func, prop);

	func = RNA_def_function(srna, "update_result", "RE_engine_update_result");
	RNA_def_function_ui_description(func, "Signal that pixels have been updated and can be redrawn in the user interface");
	prop = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "end_result", "RE_engine_end_result");
	RNA_def_function_ui_description(func, "All pixels in the render result have been set and are final");
	prop = RNA_def_pointer(func, "result", "RenderResult", "Result", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	RNA_def_boolean(func, "cancel", 0, "Cancel", "Don't mark tile as done, don't merge results unless forced");
	RNA_def_boolean(func, "do_merge_results", 0, "Merge Results", "Merge results even if cancel=true");

	func = RNA_def_function(srna, "test_break", "RE_engine_test_break");
	RNA_def_function_ui_description(func, "Test if the render operation should been canceled, this is a fast call that should be used regularly for responsiveness");
	prop = RNA_def_boolean(func, "do_break", 0, "Break", "");
	RNA_def_function_return(func, prop);

	func = RNA_def_function(srna, "update_stats", "RE_engine_update_stats");
	RNA_def_function_ui_description(func, "Update and signal to redraw render status text");
	prop = RNA_def_string(func, "stats", NULL, 0, "Stats", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_string(func, "info", NULL, 0, "Info", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "frame_set", "RE_engine_frame_set");
	RNA_def_function_ui_description(func, "Evaluate scene at a different frame (for motion blur)");
	prop = RNA_def_int(func, "frame", 0, INT_MIN, INT_MAX, "Frame", "", INT_MIN, INT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_float(func, "subframe", 0.0f, 0.0f, 1.0f, "Subframe", "", 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "update_progress", "RE_engine_update_progress");
	RNA_def_function_ui_description(func, "Update progress percentage of render");
	prop = RNA_def_float(func, "progress", 0, 0.0f, 1.0f, "", "Percentage of render that's done", 0.0f, 1.0f);
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "update_memory_stats", "RE_engine_update_memory_stats");
	RNA_def_function_ui_description(func, "Update memory usage statistics");
	RNA_def_float(func, "memory_used", 0, 0.0f, FLT_MAX, "", "Current memory usage in megabytes", 0.0f, FLT_MAX);
	RNA_def_float(func, "memory_peak", 0, 0.0f, FLT_MAX, "", "Peak memory usage in megabytes", 0.0f, FLT_MAX);
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "report", "RE_engine_report");
	RNA_def_function_ui_description(func, "Report info, warning or error messages");
	prop = RNA_def_enum_flag(func, "type", wm_report_items, 0, "Type", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_string(func, "message", NULL, 0, "Report Message", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "error_set", "RE_engine_set_error_message");
	RNA_def_function_ui_description(func, "Set error message displaying after the render is finished");
	prop = RNA_def_string(func, "message", NULL, 0, "Report Message", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "bind_display_space_shader", "engine_bind_display_space_shader");
	RNA_def_function_ui_description(func, "Bind GLSL fragment shader that converts linear colors to display space colors using scene color management settings");
	prop = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);

	func = RNA_def_function(srna, "unbind_display_space_shader", "engine_unbind_display_space_shader");
	RNA_def_function_ui_description(func, "Unbind GLSL display space shader, must always be called after binding the shader");

	func = RNA_def_function(srna, "support_display_space_shader", "engine_support_display_space_shader");
	RNA_def_function_ui_description(func, "Test if GLSL display space shader is supported for the combination of graphics card and scene settings");
	prop = RNA_def_pointer(func, "scene", "Scene", "", "");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	prop = RNA_def_boolean(func, "supported", 0, "Supported", "");
	RNA_def_function_return(func, prop);
#endif

	RNA_define_verify_sdna(false);

#if 0
	prop = RNA_def_property(srna, "is_animation", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_ANIMATION);

	prop = RNA_def_property(srna, "is_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_PREVIEW);

	prop = RNA_def_property(srna, "camera_override", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "camera_override");
	RNA_def_property_struct_type(prop, "Object");

	prop = RNA_def_property(srna, "layer_override", PROP_BOOLEAN, PROP_LAYER_MEMBER);
	RNA_def_property_boolean_sdna(prop, NULL, "layer_override", 1);
	RNA_def_property_array(prop, 20);

	prop = RNA_def_property(srna, "tile_x", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "tile_x");
	prop = RNA_def_property(srna, "tile_y", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_sdna(prop, NULL, "tile_y");

	prop = RNA_def_property(srna, "resolution_x", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "resolution_x");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	prop = RNA_def_property(srna, "resolution_y", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "resolution_y");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* Render Data */
	prop = RNA_def_property(srna, "render", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "RenderSettings");
	RNA_def_property_pointer_funcs(prop, "rna_RenderEngine_render_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Render Data", "");

	prop = RNA_def_property(srna, "use_highlight_tiles", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", RE_ENGINE_HIGHLIGHT_TILES);
#endif

	/* registration */

	prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->idname");
	RNA_def_property_flag(prop, PROP_REGISTER);

	prop = RNA_def_property(srna, "bl_label", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "type->name");
	RNA_def_property_flag(prop, PROP_REGISTER);

#if 0
	prop = RNA_def_property(srna, "bl_use_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_PREVIEW);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_texture_preview", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_TEXTURE_PREVIEW);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_postprocess", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "type->flag", RE_USE_POSTPROCESS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_shading_nodes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SHADING_NODES);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_exclude_layers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_EXCLUDE_LAYERS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);

	prop = RNA_def_property(srna, "bl_use_save_buffers", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "type->flag", RE_USE_SAVE_BUFFERS);
	RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL);
#endif

	RNA_define_verify_sdna(true);
}

void RNA_def_asset(BlenderRNA *brna)
{
	rna_def_asset_engine(brna);
#if 0
	rna_def_render_result(brna);
	rna_def_render_layer(brna);
	rna_def_render_pass(brna);
	rna_def_render_bake_pixel(brna);
#endif
}

#endif /* RNA_RUNTIME */
