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

#ifndef __LIGHT_H__
#define __LIGHT_H__

#include "kernel_types.h"

#include "util_types.h"
#include "util_vector.h"
#include "util_ies.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Progress;
class Scene;

class Light {
public:
	Light();

	LightType type;
	float3 co;

	float3 dir;
	float size;

	float3 axisu;
	float sizeu;
	float3 axisv;
	float sizev;

	Transform tfm;

	int map_resolution;

	float spot_angle;
	float spot_smooth;

	bool cast_shadow;
	bool use_mis;
	bool use_diffuse;
	bool use_glossy;
	bool use_transmission;
	bool use_scatter;

	bool is_portal;

	int shader;
	int samples;
	int max_bounces;

	void tag_update(Scene *scene);

	/* Check whether the light has contribution the the scene. */
	bool has_contribution(Scene *scene);
};

class LightManager {
public:
	bool use_light_visibility;
	bool need_update;

	LightManager();
	~LightManager();

	void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_free(Device *device, DeviceScene *dscene, Scene *scene);

	void tag_update(Scene *scene);
	int add_ies_from_file(const string& filename);
	int add_ies(const string& content);
	void remove_ies(int offset);

protected:
	void device_update_points(Device *device, DeviceScene *dscene, Scene *scene);
	void device_update_distribution(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_background(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
	void device_update_ies(DeviceScene *dscene, Scene *scene);

	struct IESEntry {
		uint hash;
		IESLight *ieslight;
		size_t offset;
		int users;
	};
	vector<IESEntry> ies_lights;
	size_t ies_table_offset;
};

CCL_NAMESPACE_END

#endif /* __LIGHT_H__ */

