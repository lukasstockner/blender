/*
 * $Id$
 *
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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_OBJECT_H__
#define __RENDER_OBJECT_H__

struct CustomData;
struct Material;
struct Object;
struct ObjectInstanceRen;
struct ObjectRen;
struct Render;
struct RenderCamera;
struct RenderDB;
struct RenderPart;

/* Object */

struct ObjectRen *render_object_create(struct RenderDB *rdh,
	struct Object *ob, struct Object *par, int index, int psysindex, int lay);
void render_object_free(struct ObjectRen *obr);

/* Instance */

struct ObjectInstanceRen *render_instance_create(struct RenderDB *rdb,
	struct ObjectRen *obr, struct Object *ob, struct Object *par,
	int index, int psysindex, float mat[][4], int lay);
void render_instance_free(struct ObjectInstanceRen *obi);

void render_instances_init(struct RenderDB *rdb);
void render_instances_bound(struct RenderDB *db, float boundbox[2][3]);

/* Data Layers */

int render_object_chunk_get(void **array_r, int *len_r, int nr, size_t size);
void render_object_customdata_set(struct ObjectRen *obr, struct CustomData *data);

/* Material */

struct Material *give_render_material(struct Render *re, struct Object *ob, int nr);

/* Adaptive Subdivision */

void part_subdivide_objects(struct Render *re, struct RenderPart *pa);
void part_subdivide_free(struct RenderPart *pa);
struct ObjectInstanceRen *part_get_instance(struct RenderPart *pa, struct ObjectInstanceRen *obi);


/* Structs */

struct HaloRen;
struct RayFace;
struct RayObject;
struct Scene;
struct StrandBuffer;
struct StrandTableNode;
struct VertTableNode;
struct VlakPrimitive;
struct VlakTableNode;
struct VolumePrecache;

typedef struct ObjectRen {
	struct ObjectRen *next, *prev;
	struct Object *ob, *par;
	struct Scene *sce;
	int index, psysindex, flag, lay;

	struct ObjectRen *lowres;

	float boundbox[2][3];

	int totvert, totvlak, totstrand, tothalo;
	int vertnodeslen, vlaknodeslen, strandnodeslen, blohalen;
	struct VertTableNode *vertnodes;
	struct VlakTableNode *vlaknodes;
	struct StrandTableNode *strandnodes;
	struct HaloRen **bloha;
	struct StrandBuffer *strandbuf;

	char (*mtface)[32];
	char (*mcol)[32];
	int  actmtface, actmcol, bakemtface;

	float obmat[4][4];	/* only used in convertblender.c, for instancing */

	/* used on makeraytree */
	struct RayObject *raytree;
	struct RayFace *rayfaces;
	struct VlakPrimitive *rayprimitives;
	struct ObjectInstanceRen *rayobi;
	
} ObjectRen;

typedef struct ObjectInstanceRen {
	struct ObjectInstanceRen *next, *prev;

	ObjectRen *obr;
	struct Object *ob, *par;
	int index, psysindex, lay;

	float mat[4][4], nmat[3][3]; /* nmat is inverse mat tranposed */
	short flag;

	float dupliorco[3], dupliuv[2];
	float (*duplitexmat)[4];
	
	struct VolumePrecache *volume_precache;
	
	float *vectors;
	int totvector;
	
	/* used on makeraytree */
	struct RayObject *raytree;
	int transform_primitives;

} ObjectInstanceRen;

/* objectren->flag */
#define R_INSTANCEABLE		1
#define R_LOWRES			2
#define R_HIGHRES			4
#define R_TEMP_COPY			8

/* objectinstance->flag */
#define R_DUPLI_TRANSFORMED	1
#define R_ENV_TRANSFORMED	2
#define R_TRANSFORMED		(1|2)
#define R_NEED_VECTORS		4
#define R_HIDDEN			8

/* data layer size */
#define RE_ORCO_ELEMS		3
#define RE_STICKY_ELEMS		2
#define RE_STRESS_ELEMS		1
#define RE_RAD_ELEMS		4
#define RE_STRAND_ELEMS		1
#define RE_TANGENT_ELEMS	3
#define RE_STRESS_ELEMS		1
#define RE_WINSPEED_ELEMS	4
#define RE_MTFACE_ELEMS		1
#define RE_MCOL_ELEMS		4
#define RE_UV_ELEMS			2
#define RE_SURFNOR_ELEMS	3
#define RE_RADFACE_ELEMS	1
#define RE_SIMPLIFY_ELEMS	2
#define RE_FACE_ELEMS		1
#define RE_NMAP_TANGENT_ELEMS	12
#define RE_STRANDCO_ELEMS	1

#endif /* __RENDER_OBJECT_H__ */

