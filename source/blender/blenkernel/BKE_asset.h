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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BKE_asset.h
 *  \ingroup bke
 */

#ifndef __BKE_ASSET_H__
#define __BKE_ASSET_H__

#ifdef __cplusplus
extern "C" {
#endif

struct AssetEngine;
struct AssetEngineType;
struct ExtensionRNA;
struct ListBase;

#if 0
#include "DNA_scene_types.h"
#include "RE_bake.h"
#include "RNA_types.h"


struct bNode;
struct bNodeTree;
struct Object;
struct Render;
struct RenderData;
struct RenderLayer;
struct RenderResult;
struct ReportList;
struct Scene;
struct BakePixel;

/* External Engine */

/* RenderEngineType.flag */
#define RE_INTERNAL				1
#define RE_GAME					2
#define RE_USE_PREVIEW			4
#define RE_USE_POSTPROCESS		8
#define RE_USE_SHADING_NODES	16
#define RE_USE_EXCLUDE_LAYERS	32
#define RE_USE_SAVE_BUFFERS		64
#define RE_USE_TEXTURE_PREVIEW		128

/* RenderEngine.flag */
#define RE_ENGINE_ANIMATION		1
#define RE_ENGINE_PREVIEW		2
#define RE_ENGINE_DO_DRAW		4
#define RE_ENGINE_DO_UPDATE		8
#define RE_ENGINE_RENDERING		16
#define RE_ENGINE_HIGHLIGHT_TILES	32
#define RE_ENGINE_USED_FOR_VIEWPORT	64

/* RenderEngine.update_flag, used by internal now */
#define RE_ENGINE_UPDATE_MA			1
#define RE_ENGINE_UPDATE_OTHER		2
#define RE_ENGINE_UPDATE_DATABASE	4

#endif

extern ListBase asset_engines;

typedef struct AssetEngineType {
	struct AssetEngineType *next, *prev;

	/* type info */
	char idname[64]; // best keep the same size as BKE_ST_MAXNAME
	char name[64];
	int flag;

#if 0
	void (*update)(struct RenderEngine *engine, struct Main *bmain, struct Scene *scene);
	void (*render)(struct RenderEngine *engine, struct Scene *scene);
	void (*bake)(struct RenderEngine *engine, struct Scene *scene, struct Object *object, const int pass_type, const struct BakePixel *pixel_array, const int num_pixels, const int depth, void *result);

	void (*view_update)(struct RenderEngine *engine, const struct bContext *context);
	void (*view_draw)(struct RenderEngine *engine, const struct bContext *context);

	void (*update_script_node)(struct RenderEngine *engine, struct bNodeTree *ntree, struct bNode *node);
#endif

	/* RNA integration */
	struct ExtensionRNA ext;
} AssetEngineType;

typedef struct AssetEngine {
	AssetEngineType *type;
	void *py_instance;

	int flag;

	struct ReportList *reports;
} AssetEngine;

/* Engine Types */
void BKE_asset_engines_init(void);
void BKE_asset_engines_exit(void);

AssetEngineType *BKE_asset_engines_find(const char *idname);

/* Engine Instances */
AssetEngine *BKE_asset_engine_create(AssetEngineType *type);
void BKE_asset_engine_free(AssetEngine *engine);

#if 0
void RE_layer_load_from_file(struct RenderLayer *layer, struct ReportList *reports, const char *filename, int x, int y);
void RE_result_load_from_file(struct RenderResult *result, struct ReportList *reports, const char *filename);

struct RenderResult *RE_engine_begin_result(RenderEngine *engine, int x, int y, int w, int h, const char *layername);
void RE_engine_update_result(RenderEngine *engine, struct RenderResult *result);
void RE_engine_end_result(RenderEngine *engine, struct RenderResult *result, int cancel, int merge_results);

int RE_engine_test_break(RenderEngine *engine);
void RE_engine_update_stats(RenderEngine *engine, const char *stats, const char *info);
void RE_engine_update_progress(RenderEngine *engine, float progress);
void RE_engine_update_memory_stats(RenderEngine *engine, float mem_used, float mem_peak);
void RE_engine_report(RenderEngine *engine, int type, const char *msg);
void RE_engine_set_error_message(RenderEngine *engine, const char *msg);

int RE_engine_render(struct Render *re, int do_all);

bool RE_engine_is_external(struct Render *re);

void RE_engine_frame_set(struct RenderEngine *engine, int frame, float subframe);

void RE_engine_get_current_tiles(struct Render *re, int *total_tiles_r, rcti **tiles_r);
struct RenderData *RE_engine_get_render_data(struct Render *re);
void RE_bake_engine_set_engine_parameters(struct Render *re, struct Main *bmain, struct Scene *scene);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BKE_ASSET_H__ */
