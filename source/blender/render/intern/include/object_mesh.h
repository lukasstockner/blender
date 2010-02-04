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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __RENDER_OBJECT_MESH_H__
#define __RENDER_OBJECT_MESH_H__

struct Material;
struct MCol;
struct MTFace;
struct Material;
struct ObjectInstanceRen;
struct ObjectRen;
struct VertRen;
struct VlakRen;

/* Find/Add/Copy Verts and Faces */

struct VertRen *render_object_vert_get(struct ObjectRen *obr, int nr);
struct VlakRen *render_object_vlak_get(struct ObjectRen *obr, int nr);

struct VertRen *render_object_vert_copy(struct ObjectRen *obr, struct VertRen *ver);
struct VlakRen *render_object_vlak_copy(struct ObjectRen *obr, struct VlakRen *vlr);

/* Vertex Texture Coordinates */

float *render_vert_get_sticky(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *render_vert_get_stress(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *render_vert_get_rad(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *render_vert_get_strand(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *render_vert_get_tangent(struct ObjectRen *obr, struct VertRen *ver, int verify);
float *render_vert_get_winspeed(struct ObjectInstanceRen *obi, struct VertRen *ver, int verify);

/* Face Texture Coordinates */

struct MTFace *render_vlak_get_tface(struct ObjectRen *obr, struct VlakRen *ren, int n, char **name, int verify);
struct MCol *render_vlak_get_mcol(struct ObjectRen *obr, struct VlakRen *ren, int n, char **name, int verify);
float *render_vlak_get_surfnor(struct ObjectRen *obr, struct VlakRen *ren, int verify);
float *render_vlak_get_nmap_tangent(struct ObjectRen *obr, struct VlakRen *ren, int verify);
int render_vlak_get_normal(struct ObjectInstanceRen *obi, struct VlakRen *vlr, float *nor);

/* Conversion */

void init_render_object_data(struct Render *re, struct ObjectRen *obr, int timeoffset);
void finalize_render_object(struct Render *re, struct ObjectRen *obr, int timeoffset);

/* Structs */

typedef struct VertTableNode {
	struct VertRen *vert;
	float *rad;
	float *sticky;
	float *strand;
	float *tangent;
	float *stress;
	float *winspeed;
} VertTableNode;

typedef struct VlakTableNode {
	struct VlakRen *vlak;
	struct MTFace *mtface;
	struct MCol *mcol;
	int totmtface, totmcol;
	float *surfnor;
	float *tangent;
} VlakTableNode;

typedef struct VertRen
{
	float co[3];
	float n[3];
	float *orco;
	short clip;
	unsigned short flag;		/* in use for clipping zbuffer parts, temp setting stuff in convertblender.c */
	float accum;		/* accum for radio weighting, and for strand texco static particles */
	int index;			/* index allows extending vertren with any property */
} VertRen;

typedef struct VlakRen {
	struct VertRen *v1, *v2, *v3, *v4;	/* keep in order for ** addressing */
	float n[3];
	struct Material *mat;
	char puno;
	char flag, ec;
	int index;
} VlakRen;

/* vlakren->flag (vlak = face in dutch) char!!! */
#define R_SMOOTH		1
#define R_UNUSED_FLAG	2
#define R_STRAND		4 /* strand flag, means special handling */
#define R_NOPUNOFLIP	8
#define R_FULL_OSA		16
#define R_FACE_SPLIT	32 /* tells render to divide face other way. */
#define R_DIVIDE_24		64	
#define R_TANGENT		128	/* vertex normals are tangent or view-corrected vector, for hair strands */

/* Defines for Quad Rasterization */

#define RE_QUAD_MASK	0x7FFFFFF
#define RE_QUAD_OFFS	0x8000000

#endif /* __RENDER_OBJECT_MESH_H__ */

