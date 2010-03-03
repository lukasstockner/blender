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

#ifndef __RENDER_LAMP_H__
#define __RENDER_LAMP_H__

struct Group;
struct GroupObject;
struct Lamp;
struct LampRen;
struct ListBase;
struct Object;
struct Render;
struct ShadeInput;

/* Lamp create/free */

struct GroupObject *lamp_create(struct Render *re, struct Object *ob);
void lamp_free(struct LampRen *lar);

struct ListBase *lamps_get(struct Render *re, struct ShadeInput *shi);

/* Lightgroup create/free */

void lightgroup_create(struct Render *re, struct Group *group, int exclusive);
void lightgroup_free(struct Render *re);

/* Test to see if lamp has any influence on the current shading point,
   on returning 1 the lamp should be skipped entirely. */

int lamp_skip(struct Render *re, struct LampRen *lar, struct ShadeInput *shi);

/* Sample a point on lamp (or use center if r == NULL), and return unshadowed
   influence, shadow and vector from lamp (for surface only at the moment).
   unit of influence is lux (lm/m^2) */

int lamp_sample(float lv[3], float lainf[3], float lashdw[3],
	struct Render *re, struct LampRen *lar, struct ShadeInput *shi,
	float co[3], float r[2]);

/* Lamp shadow */

void lamp_shadow(float lashdw[3],
	struct Render *re, struct LampRen *lar, struct ShadeInput *shi,
	float from[3], float to[3], float lv[3], int do_real);

/* Visibility factor from shading point to point on the lamp. */

int lamp_visibility(struct LampRen *lar, float co[3], float vn[3],
	float lco[3], float r_vec[3], float *r_dist, float *r_fac);

/* Spot Halo */

void lamp_spothalo_render(struct Render *re, struct ShadeInput *shi,
	float *col, float alpha);

/* Structs
 *
 * For each lamp in a scene, a LampRen is created. It determines
 * the properties of a light source. */

#include "DNA_material_types.h"
#include "BLI_threads.h"

struct MTex;
struct RayObject;

typedef struct LampShadowSubSample {
	int samplenr;
	float lashdw[3];	/* rgb shadow */
} LampShadowSubSample;

typedef struct LampShadowSample {
	LampShadowSubSample s[16];	/* MAX OSA */
} LampShadowSample;

typedef struct LampRen {
	struct LampRen *next, *prev;
	
	float xs, ys, dist;
	float co[3];
	short type;
	int mode;
	float r, g, b, k;
	float shdwr, shdwg, shdwb;
	float power, haint, energy;
	int lay;
	float spotsi,spotbl;
	float vec[3];
	float xsp, ysp, distkw, inpr;
	float halokw, halo;
	
	short falloff_type;
	float falloff_smooth;
	struct CurveMapping *curfalloff;

	/* copied from Lamp, to decouple more rendering stuff */
	/** Size of the shadowbuffer */
	short bufsize;
	/** Number of samples for the shadows */
	short samp;
	/** Softness factor for shadow */
	float soft;
	/** amount of subsample buffers and type of filter for sampling */
	short buffers, filtertype;
	/** shadow buffer type (regular, irregular) */
	short buftype;
	/** autoclip */
	short bufflag;
	/** shadow plus halo: detail level */
	short shadhalostep;
	/** Near clip of the lamp */
	float clipsta;
	/** Far clip of the lamp */
	float clipend;
	/** A small depth offset to prevent self-shadowing. */
	float bias;
	/* Compression threshold for deep shadow maps */
	float compressthresh;
	
	short ray_samp, ray_samp_method, ray_samp_type, area_shape, ray_totsamp;
	float area_size, area_sizey;
	float adapt_thresh;

	/* sun/sky */
	struct SunSky *sunsky;
	
	struct ShadBuf *shb;
	
	float imat[3][3];
	float spottexfac;
	float sh_invcampos[3], sh_zfac;	/* sh_= spothalo */
	
	float mat[3][3];	/* 3x3 part from lampmat x viewmat */
	float area[8][3], areasize;
	
	/* passes & node shader support: all shadow info for a pixel */
	LampShadowSample *shadsamp;
		
	/* yafray: photonlight params */
	int YF_numphotons, YF_numsearch;
	short YF_phdepth, YF_useqmc, YF_bufsize;
	float YF_causticblur, YF_ltradius;
	float YF_glowint, YF_glowofs;
	short YF_glowtype;
	
	/* ray optim */
	struct RayObject *last_hit[BLENDER_MAX_THREADS];
	
	struct MTex *mtex[MAX_MTEX];

	/* threading */
	int thread_assigned;
	int thread_ready;
} LampRen;

#endif /* __RENDER_LAMP_H__ */

