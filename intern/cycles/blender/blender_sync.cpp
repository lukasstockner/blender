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

#include "background.h"
#include "camera.h"
#include "film.h"
#include "graph.h"
#include "integrator.h"
#include "light.h"
#include "mesh.h"
#include "nodes.h"
#include "object.h"
#include "scene.h"
#include "shader.h"
#include "curves.h"

#include "device.h"

#include "blender_sync.h"
#include "blender_session.h"
#include "blender_util.h"

#include "util_debug.h"
#include "util_foreach.h"
#include "util_opengl.h"
#include "util_hash.h"

CCL_NAMESPACE_BEGIN

/* Constructor */

BlenderSync::BlenderSync(BL::RenderEngine& b_engine,
                         BL::BlendData& b_data,
                         BL::Scene& b_scene,
                         Scene *scene,
                         bool preview,
                         Progress &progress,
                         bool is_cpu)
: b_engine(b_engine),
  b_data(b_data),
  b_scene(b_scene),
  shader_map(&scene->shaders),
  object_map(&scene->objects),
  mesh_map(&scene->meshes),
  light_map(&scene->lights),
  particle_system_map(&scene->particle_systems),
  world_map(NULL),
  world_recalc(false),
  scene(scene),
  preview(preview),
  experimental(false),
  is_cpu(is_cpu),
  dicing_rate(1.0f),
  max_subdivisions(12),
  progress(progress)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate") : RNA_float_get(&cscene, "dicing_rate");
	max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");
}

BlenderSync::~BlenderSync()
{
}

/* Sync */

bool BlenderSync::sync_recalc()
{
	/* sync recalc flags from blender to cycles. actual update is done separate,
	 * so we can do it later on if doing it immediate is not suitable */

	BL::BlendData::materials_iterator b_mat;
	bool has_updated_objects = b_data.objects.is_updated();
	for(b_data.materials.begin(b_mat); b_mat != b_data.materials.end(); ++b_mat) {
		if(b_mat->is_updated() || (b_mat->node_tree() && b_mat->node_tree().is_updated())) {
			shader_map.set_recalc(*b_mat);
		}
		else {
			Shader *shader = shader_map.find(*b_mat);
			if(has_updated_objects && shader != NULL && shader->has_object_dependency) {
				shader_map.set_recalc(*b_mat);
			}
		}
	}

	BL::BlendData::lamps_iterator b_lamp;

	for(b_data.lamps.begin(b_lamp); b_lamp != b_data.lamps.end(); ++b_lamp)
		if(b_lamp->is_updated() || (b_lamp->node_tree() && b_lamp->node_tree().is_updated()))
			shader_map.set_recalc(*b_lamp);

	bool dicing_prop_changed = false;

	if(experimental) {
		PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

		float updated_dicing_rate = preview ? RNA_float_get(&cscene, "preview_dicing_rate")
		                                    : RNA_float_get(&cscene, "dicing_rate");

		if(dicing_rate != updated_dicing_rate) {
			dicing_rate = updated_dicing_rate;
			dicing_prop_changed = true;
		}

		int updated_max_subdivisions = RNA_int_get(&cscene, "max_subdivisions");

		if(max_subdivisions != updated_max_subdivisions) {
			max_subdivisions = updated_max_subdivisions;
			dicing_prop_changed = true;
		}
	}

	BL::BlendData::objects_iterator b_ob;

	for(b_data.objects.begin(b_ob); b_ob != b_data.objects.end(); ++b_ob) {
		if(b_ob->is_updated()) {
			object_map.set_recalc(*b_ob);
			light_map.set_recalc(*b_ob);
		}

		if(object_is_mesh(*b_ob)) {
			if(b_ob->is_updated_data() || b_ob->data().is_updated() ||
			   (dicing_prop_changed && object_subdivision_type(*b_ob, preview, experimental) != Mesh::SUBDIVISION_NONE))
			{
				BL::ID key = BKE_object_is_modified(*b_ob)? *b_ob: b_ob->data();
				mesh_map.set_recalc(key);
			}
		}
		else if(object_is_light(*b_ob)) {
			if(b_ob->is_updated_data() || b_ob->data().is_updated())
				light_map.set_recalc(*b_ob);
		}
		
		if(b_ob->is_updated_data()) {
			BL::Object::particle_systems_iterator b_psys;
			for(b_ob->particle_systems.begin(b_psys); b_psys != b_ob->particle_systems.end(); ++b_psys)
				particle_system_map.set_recalc(*b_ob);
		}
	}

	BL::BlendData::meshes_iterator b_mesh;

	for(b_data.meshes.begin(b_mesh); b_mesh != b_data.meshes.end(); ++b_mesh) {
		if(b_mesh->is_updated()) {
			mesh_map.set_recalc(*b_mesh);
		}
	}

	BL::BlendData::worlds_iterator b_world;

	for(b_data.worlds.begin(b_world); b_world != b_data.worlds.end(); ++b_world) {
		if(world_map == b_world->ptr.data) {
			if(b_world->is_updated() ||
			   (b_world->node_tree() && b_world->node_tree().is_updated()))
			{
				world_recalc = true;
			}
			else if(b_world->node_tree() && b_world->use_nodes()) {
				Shader *shader = scene->default_background;
				if(has_updated_objects && shader->has_object_dependency) {
					world_recalc = true;
				}
			}
		}
	}

	bool recalc =
		shader_map.has_recalc() ||
		object_map.has_recalc() ||
		light_map.has_recalc() ||
		mesh_map.has_recalc() ||
		particle_system_map.has_recalc() ||
		BlendDataObjects_is_updated_get(&b_data.ptr) ||
		world_recalc;

	return recalc;
}

void BlenderSync::sync_data(BL::RenderSettings& b_render,
                            BL::SpaceView3D& b_v3d,
                            BL::Object& b_override,
                            int width, int height,
                            void **python_thread_state,
                            const char *layer)
{
	sync_render_layers(b_v3d, layer);
	sync_integrator();
	sync_film();
	sync_shaders();
	sync_images();
	sync_curve_settings();

	mesh_synced.clear(); /* use for objects and motion sync */

	if(scene->need_motion() == Scene::MOTION_PASS ||
	   scene->need_motion() == Scene::MOTION_NONE ||
	   scene->camera->motion_position == Camera::MOTION_POSITION_CENTER)
	{
		sync_objects(b_v3d);
	}
	sync_motion(b_render,
	            b_v3d,
	            b_override,
	            width, height,
	            python_thread_state);

	mesh_synced.clear();
}

/* Integrator */

void BlenderSync::sync_integrator()
{
#ifdef __CAMERA_MOTION__
	BL::RenderSettings r = b_scene.render();
#endif
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	experimental = (get_enum(cscene, "feature_set") != 0);

	Integrator *integrator = scene->integrator;
	Integrator previntegrator = *integrator;

	integrator->min_bounce = get_int(cscene, "min_bounces");
	integrator->max_bounce = get_int(cscene, "max_bounces");

	integrator->max_diffuse_bounce = get_int(cscene, "diffuse_bounces");
	integrator->max_glossy_bounce = get_int(cscene, "glossy_bounces");
	integrator->max_transmission_bounce = get_int(cscene, "transmission_bounces");
	integrator->max_volume_bounce = get_int(cscene, "volume_bounces");

	integrator->transparent_max_bounce = get_int(cscene, "transparent_max_bounces");
	integrator->transparent_min_bounce = get_int(cscene, "transparent_min_bounces");
	integrator->transparent_shadows = get_boolean(cscene, "use_transparent_shadows");

	integrator->volume_max_steps = get_int(cscene, "volume_max_steps");
	integrator->volume_step_size = get_float(cscene, "volume_step_size");

	integrator->caustics_reflective = get_boolean(cscene, "caustics_reflective");
	integrator->caustics_refractive = get_boolean(cscene, "caustics_refractive");
	integrator->filter_glossy = get_float(cscene, "blur_glossy");

	integrator->seed = get_int(cscene, "seed");
	if(get_boolean(cscene, "use_animated_seed")) {
		integrator->seed = hash_int_2d(b_scene.frame_current(),
		                               get_int(cscene, "seed"));
		if(b_scene.frame_subframe() != 0.0f) {
			/* TODO(sergey): Ideally should be some sort of hash_merge,
			 * but this is good enough for now.
			 */
			integrator->seed += hash_int_2d((int)(b_scene.frame_subframe() * (float)INT_MAX),
			                                get_int(cscene, "seed"));
		}
	}

	int sampling_pattern = get_enum(cscene, "sampling_pattern");
	switch(sampling_pattern) {
		case 1: /* Dithered Sobol */
			integrator->sampling_pattern = SAMPLING_PATTERN_SOBOL;
			integrator->use_dithered_sampling = true;
			break;
		case 2: /* Correlated Multi-Jittered */
			integrator->sampling_pattern = SAMPLING_PATTERN_CMJ;
			integrator->use_dithered_sampling = false;
			break;
		case 0: /* Sobol */
		default:
			integrator->sampling_pattern = SAMPLING_PATTERN_SOBOL;
			integrator->use_dithered_sampling = false;
			break;
	}
	integrator->scrambling_distance = get_float(cscene, "scrambling_distance");

	integrator->sample_clamp_direct = get_float(cscene, "sample_clamp_direct");
	integrator->sample_clamp_indirect = get_float(cscene, "sample_clamp_indirect");
#ifdef __CAMERA_MOTION__
	if(!preview) {
		if(integrator->motion_blur != r.use_motion_blur()) {
			scene->object_manager->tag_update(scene);
			scene->camera->tag_update();
		}

		integrator->motion_blur = r.use_motion_blur();
	}
#endif

	integrator->method = (Integrator::Method)get_enum(cscene,
	                                                  "progressive",
	                                                  Integrator::NUM_METHODS,
	                                                  Integrator::PATH);

	integrator->sample_all_lights_direct = get_boolean(cscene, "sample_all_lights_direct");
	integrator->sample_all_lights_indirect = get_boolean(cscene, "sample_all_lights_indirect");
	integrator->light_sampling_threshold = get_float(cscene, "light_sampling_threshold");

	int diffuse_samples = get_int(cscene, "diffuse_samples");
	int glossy_samples = get_int(cscene, "glossy_samples");
	int transmission_samples = get_int(cscene, "transmission_samples");
	int ao_samples = get_int(cscene, "ao_samples");
	int mesh_light_samples = get_int(cscene, "mesh_light_samples");
	int subsurface_samples = get_int(cscene, "subsurface_samples");
	int volume_samples = get_int(cscene, "volume_samples");

	if(get_boolean(cscene, "use_square_samples")) {
		integrator->diffuse_samples = diffuse_samples * diffuse_samples;
		integrator->glossy_samples = glossy_samples * glossy_samples;
		integrator->transmission_samples = transmission_samples * transmission_samples;
		integrator->ao_samples = ao_samples * ao_samples;
		integrator->mesh_light_samples = mesh_light_samples * mesh_light_samples;
		integrator->subsurface_samples = subsurface_samples * subsurface_samples;
		integrator->volume_samples = volume_samples * volume_samples;
	} 
	else {
		integrator->diffuse_samples = diffuse_samples;
		integrator->glossy_samples = glossy_samples;
		integrator->transmission_samples = transmission_samples;
		integrator->ao_samples = ao_samples;
		integrator->mesh_light_samples = mesh_light_samples;
		integrator->subsurface_samples = subsurface_samples;
		integrator->volume_samples = volume_samples;
	}

	if(integrator->modified(previntegrator))
		integrator->tag_update(scene);
}

/* Film */

void BlenderSync::sync_film()
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	Film *film = scene->film;
	Film prevfilm = *film;
	
	film->exposure = get_float(cscene, "film_exposure");
	film->filter_type = (FilterType)get_enum(cscene,
	                                         "pixel_filter_type",
	                                         FILTER_NUM_TYPES,
	                                         FILTER_BLACKMAN_HARRIS);
	film->filter_width = (film->filter_type == FILTER_BOX)? 1.0f: get_float(cscene, "filter_width");

	if(b_scene.world()) {
		BL::WorldMistSettings b_mist = b_scene.world().mist_settings();

		film->mist_start = b_mist.start();
		film->mist_depth = b_mist.depth();

		switch(b_mist.falloff()) {
			case BL::WorldMistSettings::falloff_QUADRATIC:
				film->mist_falloff = 2.0f;
				break;
			case BL::WorldMistSettings::falloff_LINEAR:
				film->mist_falloff = 1.0f;
				break;
			case BL::WorldMistSettings::falloff_INVERSE_QUADRATIC:
				film->mist_falloff = 0.5f;
				break;
		}
	}

	if(film->modified(prevfilm))
		film->tag_update(scene);
}

/* Render Layer */

void BlenderSync::sync_render_layers(BL::SpaceView3D& b_v3d, const char *layer)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	string layername;

	/* 3d view */
	if(b_v3d) {
		if(RNA_boolean_get(&cscene, "preview_active_layer")) {
			BL::RenderLayers layers(b_scene.render().ptr);
			layername = layers.active().name();
			layer = layername.c_str();
		}
		else {
			render_layer.scene_layer = get_layer(b_v3d.layers(), b_v3d.layers_local_view());
			render_layer.layer = render_layer.scene_layer;
			render_layer.exclude_layer = 0;
			render_layer.holdout_layer = 0;
			render_layer.material_override = PointerRNA_NULL;
			render_layer.use_background_shader = true;
			render_layer.use_background_ao = true;
			render_layer.use_hair = true;
			render_layer.use_surfaces = true;
			render_layer.use_viewport_visibility = true;
			render_layer.samples = 0;
			render_layer.bound_samples = false;
			return;
		}
	}

	/* render layer */
	BL::RenderSettings r = b_scene.render();
	BL::RenderSettings::layers_iterator b_rlay;
	int use_layer_samples = get_enum(cscene, "use_layer_samples");
	bool first_layer = true;
	uint layer_override = get_layer(b_engine.layer_override());
	uint scene_layers = layer_override ? layer_override : get_layer(b_scene.layers());

	for(r.layers.begin(b_rlay); b_rlay != r.layers.end(); ++b_rlay) {
		if((!layer && first_layer) || (layer && b_rlay->name() == layer)) {
			render_layer.name = b_rlay->name();

			render_layer.holdout_layer = get_layer(b_rlay->layers_zmask());
			render_layer.exclude_layer = get_layer(b_rlay->layers_exclude());

			render_layer.scene_layer = scene_layers & ~render_layer.exclude_layer;
			render_layer.scene_layer |= render_layer.exclude_layer & render_layer.holdout_layer;

			render_layer.layer = get_layer(b_rlay->layers());
			render_layer.layer |= render_layer.holdout_layer;

			render_layer.material_override = b_rlay->material_override();
			render_layer.use_background_shader = b_rlay->use_sky();
			render_layer.use_background_ao = b_rlay->use_ao();
			render_layer.use_surfaces = b_rlay->use_solid();
			render_layer.use_hair = b_rlay->use_strand();
			render_layer.use_viewport_visibility = false;

			render_layer.bound_samples = (use_layer_samples == 1);
			if(use_layer_samples != 2) {
				int samples = b_rlay->samples();
				if(get_boolean(cscene, "use_square_samples"))
					render_layer.samples = samples * samples;
				else
					render_layer.samples = samples;
			}
		}

		first_layer = false;
	}
}

/* Images */
void BlenderSync::sync_images()
{
	/* Sync is a convention for this API, but currently it frees unused buffers. */

	const bool is_interface_locked = b_engine.render() &&
	                                 b_engine.render().use_lock_interface();
	if(is_interface_locked == false && BlenderSession::headless == false) {
		/* If interface is not locked, it's possible image is needed for
		 * the display.
		 */
		return;
	}
	/* Free buffers used by images which are not needed for render. */
	BL::BlendData::images_iterator b_image;
	for(b_data.images.begin(b_image);
	    b_image != b_data.images.end();
	    ++b_image)
	{
		/* TODO(sergey): Consider making it an utility function to check
		 * whether image is considered builtin.
		 */
		const bool is_builtin = b_image->packed_file() ||
		                        b_image->source() == BL::Image::source_GENERATED ||
		                        b_image->source() == BL::Image::source_MOVIE ||
		                        b_engine.is_preview();
		if(is_builtin == false) {
			b_image->buffers_free();
		}
		/* TODO(sergey): Free builtin images not used by any shader. */
	}
}

/* Scene Parameters */

SceneParams BlenderSync::get_scene_params(BL::Scene& b_scene,
                                          bool background,
                                          bool is_cpu)
{
	BL::RenderSettings r = b_scene.render();
	SceneParams params;
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

	if(shadingsystem == 0)
		params.shadingsystem = SHADINGSYSTEM_SVM;
	else if(shadingsystem == 1)
		params.shadingsystem = SHADINGSYSTEM_OSL;
	
	if(background)
		params.bvh_type = SceneParams::BVH_STATIC;
	else
		params.bvh_type = (SceneParams::BVHType)get_enum(
		        cscene,
		        "debug_bvh_type",
		        SceneParams::BVH_NUM_TYPES,
		        SceneParams::BVH_STATIC);

	params.use_bvh_spatial_split = RNA_boolean_get(&cscene, "debug_use_spatial_splits");
	params.use_bvh_unaligned_nodes = RNA_boolean_get(&cscene, "debug_use_hair_bvh");

	if(background && params.shadingsystem != SHADINGSYSTEM_OSL)
		params.persistent_data = r.use_persistent_data();
	else
		params.persistent_data = false;

	int texture_limit;
	if(background) {
		texture_limit = RNA_enum_get(&cscene, "texture_limit_render");
	}
	else {
		texture_limit = RNA_enum_get(&cscene, "texture_limit");
	}
	if(texture_limit > 0 && b_scene.render().use_simplify()) {
		params.texture_limit = 1 << (texture_limit + 6);
	}
	else {
		params.texture_limit = 0;
	}

#if !(defined(__GNUC__) && (defined(i386) || defined(_M_IX86)))
	if(is_cpu) {
		params.use_qbvh = DebugFlags().cpu.qbvh && system_cpu_support_sse2();
	}
	else
#endif
	{
		params.use_qbvh = false;
	}

	return params;
}

/* Session Parameters */

bool BlenderSync::get_session_pause(BL::Scene& b_scene, bool background)
{
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");
	return (background)? false: get_boolean(cscene, "preview_pause");
}

SessionParams BlenderSync::get_session_params(BL::RenderEngine& b_engine,
                                              BL::UserPreferences& b_userpref,
                                              BL::Scene& b_scene,
                                              bool background)
{
	SessionParams params;
	PointerRNA cscene = RNA_pointer_get(&b_scene.ptr, "cycles");

	/* feature set */
	params.experimental = (get_enum(cscene, "feature_set") != 0);

	/* device type */
	vector<DeviceInfo>& devices = Device::available_devices();
	
	/* device default CPU */
	foreach(DeviceInfo& device, devices) {
		if(device.type == DEVICE_CPU) {
			params.device = device;
			break;
		}
	}

	if(get_enum(cscene, "device") == 2) {
		/* find network device */
		foreach(DeviceInfo& info, devices)
			if(info.type == DEVICE_NETWORK)
				params.device = info;
	}
	else if(get_enum(cscene, "device") == 1) {
		PointerRNA b_preferences;

		BL::UserPreferences::addons_iterator b_addon_iter;
		for(b_userpref.addons.begin(b_addon_iter); b_addon_iter != b_userpref.addons.end(); ++b_addon_iter) {
			if(b_addon_iter->module() == "cycles") {
				b_preferences = b_addon_iter->preferences().ptr;
				break;
			}
		}

		int compute_device = get_enum(b_preferences, "compute_device_type");

		if(compute_device != 0) {
			vector<DeviceInfo> used_devices;
			RNA_BEGIN(&b_preferences, device, "devices") {
				if(get_enum(device, "type") == compute_device && get_boolean(device, "use")) {
					string id = get_string(device, "id");
					foreach(DeviceInfo& info, devices) {
						if(info.id == id) {
							used_devices.push_back(info);
							break;
						}
					}
				}
			} RNA_END

			if(used_devices.size() == 1) {
				params.device = used_devices[0];
			}
			else if(used_devices.size() > 1) {
				params.device = Device::get_multi_device(used_devices);
			}
			/* Else keep using the CPU device that was set before. */
		}
	}

	/* Background */
	params.background = background;

	/* samples */
	int samples = get_int(cscene, "samples");
	int aa_samples = get_int(cscene, "aa_samples");
	int preview_samples = get_int(cscene, "preview_samples");
	int preview_aa_samples = get_int(cscene, "preview_aa_samples");
	
	if(get_boolean(cscene, "use_square_samples")) {
		aa_samples = aa_samples * aa_samples;
		preview_aa_samples = preview_aa_samples * preview_aa_samples;

		samples = samples * samples;
		preview_samples = preview_samples * preview_samples;
	}

	if(get_enum(cscene, "progressive") == 0) {
		if(background) {
			params.samples = aa_samples;
		}
		else {
			params.samples = preview_aa_samples;
			if(params.samples == 0)
				params.samples = INT_MAX;
		}
	}
	else {
		if(background) {
			params.samples = samples;
		}
		else {
			params.samples = preview_samples;
			if(params.samples == 0)
				params.samples = INT_MAX;
		}
	}

	/* tiles */
	if(params.device.type != DEVICE_CPU && !background) {
		/* currently GPU could be much slower than CPU when using tiles,
		 * still need to be investigated, but meanwhile make it possible
		 * to work in viewport smoothly
		 */
		int debug_tile_size = get_int(cscene, "debug_tile_size");

		params.tile_size = make_int2(debug_tile_size, debug_tile_size);
	}
	else {
		int tile_x = b_engine.tile_x();
		int tile_y = b_engine.tile_y();

		params.tile_size = make_int2(tile_x, tile_y);
	}

	if((BlenderSession::headless == false) && background) {
		params.tile_order = (TileOrder)get_enum(cscene, "tile_order");
	}
	else {
		params.tile_order = TILE_BOTTOM_TO_TOP;
	}

	params.start_resolution = get_int(cscene, "preview_start_resolution");

	/* other parameters */
	if(b_scene.render().threads_mode() == BL::RenderSettings::threads_mode_FIXED)
		params.threads = b_scene.render().threads();
	else
		params.threads = 0;

	params.cancel_timeout = (double)get_float(cscene, "debug_cancel_timeout");
	params.reset_timeout = (double)get_float(cscene, "debug_reset_timeout");
	params.text_timeout = (double)get_float(cscene, "debug_text_timeout");

	params.progressive_refine = get_boolean(cscene, "use_progressive_refine");

	if(background) {
		if(params.progressive_refine)
			params.progressive = true;
		else
			params.progressive = false;

		params.start_resolution = INT_MAX;
	}
	else
		params.progressive = true;

	/* shading system - scene level needs full refresh */
	const bool shadingsystem = RNA_boolean_get(&cscene, "shading_system");

	if(shadingsystem == 0)
		params.shadingsystem = SHADINGSYSTEM_SVM;
	else if(shadingsystem == 1)
		params.shadingsystem = SHADINGSYSTEM_OSL;
	
	/* color managagement */
#ifdef GLEW_MX
	/* When using GLEW MX we need to check whether we've got an OpenGL
	 * context for current window. This is because command line rendering
	 * doesn't have OpenGL context actually.
	 */
	if(glewGetContext() != NULL)
#endif
	{
		params.display_buffer_linear = GLEW_ARB_half_float_pixel &&
		                               b_engine.support_display_space_shader(b_scene);
	}

	if(b_engine.is_preview()) {
		/* For preview rendering we're using same timeout as
		 * blender's job update.
		 */
		params.progressive_update_timeout = 0.1;
	}

	return params;
}

PassType BlenderSync::get_pass_type(BL::RenderPass& b_pass)
{
	switch(b_pass.type()) {
		case BL::RenderPass::type_COMBINED:
			return PASS_COMBINED;

		case BL::RenderPass::type_Z:
			return PASS_DEPTH;
		case BL::RenderPass::type_MIST:
			return PASS_MIST;
		case BL::RenderPass::type_NORMAL:
			return PASS_NORMAL;
		case BL::RenderPass::type_OBJECT_INDEX:
			return PASS_OBJECT_ID;
		case BL::RenderPass::type_UV:
			return PASS_UV;
		case BL::RenderPass::type_VECTOR:
			return PASS_MOTION;
		case BL::RenderPass::type_MATERIAL_INDEX:
			return PASS_MATERIAL_ID;

		case BL::RenderPass::type_DIFFUSE_DIRECT:
			return PASS_DIFFUSE_DIRECT;
		case BL::RenderPass::type_GLOSSY_DIRECT:
			return PASS_GLOSSY_DIRECT;
		case BL::RenderPass::type_TRANSMISSION_DIRECT:
			return PASS_TRANSMISSION_DIRECT;
		case BL::RenderPass::type_SUBSURFACE_DIRECT:
			return PASS_SUBSURFACE_DIRECT;

		case BL::RenderPass::type_DIFFUSE_INDIRECT:
			return PASS_DIFFUSE_INDIRECT;
		case BL::RenderPass::type_GLOSSY_INDIRECT:
			return PASS_GLOSSY_INDIRECT;
		case BL::RenderPass::type_TRANSMISSION_INDIRECT:
			return PASS_TRANSMISSION_INDIRECT;
		case BL::RenderPass::type_SUBSURFACE_INDIRECT:
			return PASS_SUBSURFACE_INDIRECT;

		case BL::RenderPass::type_DIFFUSE_COLOR:
			return PASS_DIFFUSE_COLOR;
		case BL::RenderPass::type_GLOSSY_COLOR:
			return PASS_GLOSSY_COLOR;
		case BL::RenderPass::type_TRANSMISSION_COLOR:
			return PASS_TRANSMISSION_COLOR;
		case BL::RenderPass::type_SUBSURFACE_COLOR:
			return PASS_SUBSURFACE_COLOR;

		case BL::RenderPass::type_EMIT:
			return PASS_EMISSION;
		case BL::RenderPass::type_ENVIRONMENT:
			return PASS_BACKGROUND;
		case BL::RenderPass::type_AO:
			return PASS_AO;
		case BL::RenderPass::type_SHADOW:
			return PASS_SHADOW;

		case BL::RenderPass::type_DIFFUSE:
		case BL::RenderPass::type_COLOR:
		case BL::RenderPass::type_REFRACTION:
		case BL::RenderPass::type_SPECULAR:
		case BL::RenderPass::type_REFLECTION:
			return PASS_NONE;
#ifdef WITH_CYCLES_DEBUG
		case BL::RenderPass::type_DEBUG:
		{
			if(b_pass.debug_type() == BL::RenderPass::debug_type_BVH_TRAVERSAL_STEPS)
				return PASS_BVH_TRAVERSAL_STEPS;
			if(b_pass.debug_type() == BL::RenderPass::debug_type_BVH_TRAVERSED_INSTANCES)
				return PASS_BVH_TRAVERSED_INSTANCES;
			if(b_pass.debug_type() == BL::RenderPass::debug_type_RAY_BOUNCES)
				return PASS_RAY_BOUNCES;
			break;
		}
#endif
	}

	return PASS_NONE;
}

RenderBuffers* BlenderSync::get_render_buffer(Device *device,
                                              BL::RenderLayer& b_rl,
                                              BL::RenderResult& b_rr,
                                              int samples)
{
	BufferParams params;
	params.width  = params.full_width  = params.final_width  = b_rr.resolution_x();
	params.height = params.full_height = params.final_height = b_rr.resolution_y();

	params.full_x = params.full_y = 0;

	BL::RenderLayer::passes_iterator b_pass;

	int denoising_passes = 0;
	for(b_rl.passes.begin(b_pass); b_pass != b_rl.passes.end(); ++b_pass) {
		PassType type = get_pass_type(*b_pass);
		if(type != PASS_NONE)
			Pass::add(type, params.passes);

		int extended_type = b_pass->extended_type();
		if(extended_type) {
			denoising_passes |= extended_type;
			if(extended_type == EX_TYPE_DENOISE_CLEAN)
				params.selective_denoising = true;
		}
	}
	params.denoising_passes = ((~denoising_passes & EX_TYPE_DENOISE_REQUIRED) == 0);

	RenderBuffers *buffer = new RenderBuffers(device);
	buffer->reset(device, params);

	int4 rect = make_int4(0, 0, params.width, params.height);

	/* Some passes are divided by another pass when exporting to a RenderPass.
	 * Therefore, these passes need to be multiplied when importing, so some passes must be imported before others. */
	PassType import_first_array[] = {PASS_DIFFUSE_COLOR, PASS_GLOSSY_COLOR, PASS_TRANSMISSION_COLOR, PASS_SUBSURFACE_COLOR, PASS_MOTION_WEIGHT};
	std::set<PassType> import_first(import_first_array, import_first_array + 5);
	for(b_rl.passes.begin(b_pass); b_pass != b_rl.passes.end(); ++b_pass) {
		if(b_pass->extended_type()) continue;

		PassType type = get_pass_type(*b_pass);
		if(!import_first.count(type)) continue;

		BL::DynamicArray<float> b_rect = b_pass->rect();
		buffer->get_pass_rect(type, 1.0f, samples, b_pass->channels(), rect, b_rect.data, true);
	}

	for(b_rl.passes.begin(b_pass); b_pass != b_rl.passes.end(); ++b_pass) {
		int extended_type = b_pass->extended_type();
		PassType type = get_pass_type(*b_pass);
		if(import_first.count(type)) continue;

		BL::DynamicArray<float> b_rect = b_pass->rect();
		if(extended_type)
			buffer->get_denoising_rect(extended_type, 1.0f, samples, b_pass->channels(), rect, b_rect.data, true);
		else
			buffer->get_pass_rect(type, 1.0f, samples, b_pass->channels(), rect, b_rect.data, true);
	}

	buffer->copy_to_device();

	return buffer;
}

CCL_NAMESPACE_END

