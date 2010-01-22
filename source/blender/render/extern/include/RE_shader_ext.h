/**
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
 * The Original Code is Copyright (C) 2006 by Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef RE_SHADER_EXT_H
#define RE_SHADER_EXT_H

#include "RE_raytrace.h" /* For RE_RAYCOUNTER */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
/* this include is for shading and texture exports            */
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

/* localized texture result data */
/* note; tr tg tb ta has to remain in this order */
typedef struct TexResult {
	float tin, tr, tg, tb, ta;
	int talpha;
	float *nor;
} TexResult;

/* localized shade result data */
typedef struct ShadeResult 
{
	float combined[4];
	float col[4];
	float alpha, mist, z;
	float diff[3];		/* no ramps, shadow, etc */
	float spec[3];
	float shad[3];
	float ao[3];
	float refl[3];
	float refr[3];
	float nor[3];
	float winspeed[4];
	float rayhits[4];
	float uv[3];
	float indexob;
	float sss[4];
	float emit[3];
} ShadeResult;

typedef struct ShadeInputUV {
	float dxuv[3], dyuv[3], uv[3];
	char *name;
} ShadeInputUV;

typedef struct ShadeInputCol {
	float col[3];
	char *name;
} ShadeInputCol;

typedef struct ShadeGeometry {
	float co[3];			/* location */
	float vn[3];			/* shading normal */
	float view[3];			/* view vector */
	float tang[3];			/* tangent */

	/* coordinates */
	float camera_co[3];
	float dxco[3], dyco[3]; /* derivatives of co */
	float scanco[3];		/* original scanline coordinate without jitter */
	int xs, ys;				/* pixel to be rendered */

	/* normals */
	float facenor[3];		/* copy of flat face normal */
	float vno[3];			/* normal before bump/normal mapping */

	short flippednor;		/* is facenor flipped? */
	short tangentvn;		/* is vn tangent? */

	float dxno[3], dyno[3];	/* derivatives of vn */

	/* stored copy of original face normal (facenor) 
	 * before flipping. Used in Front/back output on geometry node */
	float orignor[3];

	/* strand normal */
	float surfnor[3], surfdist;
	
	/* interpolation weights */
	float u, v;
	float dx_u, dx_v, dy_u, dy_v;
	
	/* view derivatives */
	float dxview, dyview;

	/* derivatives enabled? */
	short osatex;
} ShadeGeometry;

typedef struct ShadeMaterial {
	struct Material *mat;

	/* copy from material, keep synced so we can do memcopy */
	/* current size: 21*sizeof(float) */
	float r, g, b;
	float specr, specg, specb;
	float mirr, mirg, mirb, pad;
	
	float amb, emit, ang, spectra, ray_mirror;
	float alpha, refl, spec, zoffs, add;
	float translucency;
	/* end direct copy from material */
	
	/* individual copies: */
	int har; /* hardness */

	float vcol[4];
	float refcol[4];

	int mode;						/* base material mode (OR-ed result of entire node tree) */

	struct Group *light_override;
	struct Material *mat_override;
} ShadeMaterial;

typedef struct ShadeTexco {
	/* texture coordinates */
	float lo[3], gl[3], ref[3], orn[3], winco[3], sticky[3];
	float displace[3];
	float strandco, nmaptang[3], stress, winspeed[4];
	float duplilo[3], dupliuv[3];

	ShadeInputUV uv[8];   /* 8 = MAX_MTFACE */
	ShadeInputCol col[8]; /* 8 = MAX_MCOL */
	int totuv, totcol, actuv, actcol;
	
	/* dx/dy OSA coordinates */
	float dxlo[3], dylo[3], dxgl[3], dygl[3];
	float dxref[3], dyref[3], dxorn[3], dyorn[3];
	float dxlv[3], dylv[3];
	float dxwin[3], dywin[3];
	float dxsticky[3], dysticky[3];
	float dxrefract[3], dyrefract[3];
	float dxstrand, dystrand;
} ShadeTexco;

typedef struct ShadePrimitive {
	struct VlakRen *vlr;
	struct StrandRen *strand;
	struct ObjectInstanceRen *obi;
	struct ObjectRen *obr;

	int facenr;

	struct VertRen *v1, *v2, *v3;	/* vertices can be in any order for quads... */
	short i1, i2, i3;				/* original vertex indices */

	float n1[3], n2[3], n3[3];		/* vertex normals, corrected */
} ShadePrimitive;

/* localized renderloop data */
typedef struct ShadeInput
{
	/* copy from face, also to extract tria from quad */
	/* XXX note it mirrors a struct above for quick copy */
	ShadeGeometry geometry;
	ShadeMaterial material;
	ShadeTexco texture;
	ShadePrimitive primitive;

	struct {
		/* AO is a pre-process now */
		float ao[3];
		float indirect[3];
		
		int mask;				/* subsample mask */

		int samplenr;			/* sample counter, to detect if we should do shadow again */
		int depth;				/* 1 or larger on raytrace shading */
		int volume_depth;		/* number of intersections through volumes */

		unsigned int lay;
		int layflag, passflag, combinedflag;

		/* from initialize, part or renderlayer */
		short do_preview;		/* for nodes, in previewrender */
		short thread, sample;	/* sample: ShadeSample array index */
		short nodes;			/* indicate node shading, temp hack to prevent recursion */
		
#ifdef RE_RAYCOUNTER
		RayCounter raycounter;
#endif
	} shading;
	
} ShadeInput;


/* node shaders... */
struct Tex;
int	multitex_ext(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres);
int	multitex_thread(struct Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, struct TexResult *texres, short thread, short which_output);

/* shaded view and bake */
struct Render;
struct Image;
struct Object;

int RE_bake_shade_all_selected(struct Render *re, int type, struct Object *actob, short *do_update);
struct Image *RE_bake_shade_get_image(struct Render *re);

#endif /* RE_SHADER_EXT_H */

