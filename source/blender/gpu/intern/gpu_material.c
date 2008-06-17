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
 * The Original Code is Copyright (C) 2006 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_lamp_types.h"
#include "DNA_listBase.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"

#include "BKE_colortools.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "GPU_extensions.h"
#include "GPU_material.h"

#include "gpu_codegen.h"

#include <string.h>

struct GPUMaterial {
	Material *ma;
	int profile;

	/* for creating the material */
	ListBase nodes;
	GPUNodeLink *outlink;

	/* for binding the material */
	GPUPass *pass;
	GPUVertexAttribs attribs;
	int alpha;
};

/* Functions */

static GPUMaterial *GPU_material_construct_begin(Material *ma, int profile)
{
	GPUMaterial *material = MEM_callocN(sizeof(GPUMaterial), "GPUMaterial");

	material->ma= ma;
	material->profile = profile;

	return material;
}

static void gpu_material_create_vertex_attributes(GPUMaterial *material)
{
	GPU_nodes_create_vertex_attributes(&material->nodes, &material->attribs);
}

static void gpu_material_set_attrib_id(GPUMaterial *material)
{
	GPUVertexAttribs *attribs;
	GPUShader *shader;
	GPUPass *pass;
	char name[32];
	int a, b;

	attribs= &material->attribs;
	pass= material->pass;
	if(!pass) {
		attribs->totlayer = 0;
		return;
	}
	
	shader= GPU_pass_shader(pass);
	if(!shader) {
		attribs->totlayer = 0;
		return;
	}

	/* convert from attribute number to the actual id assigned by opengl,
	 * in case the attrib does not get a valid index back, it was probably
	 * removed by the glsl compiler by dead code elimination */

	for(a=0, b=0; a<attribs->totlayer; a++) {
		sprintf(name, "att%d", attribs->layer[a].glindex);
		attribs->layer[a].glindex = GPU_shader_get_attribute(shader, name);

		if(attribs->layer[a].glindex >= 0) {
			attribs->layer[b] = attribs->layer[a];
			b++;
		}
	}

	attribs->totlayer = b;
}

static int GPU_material_construct_end(GPUMaterial *material)
{
	if (material->outlink) {
		GPUNodeLink *outlink;

		gpu_material_create_vertex_attributes(material);

		outlink = material->outlink;
		material->pass = GPU_generate_pass(&material->nodes, outlink, 1, material->profile);

		if(!material->pass)
			return 0;

		if(material->profile == GPU_PROFILE_DERIVEDMESH)
			gpu_material_set_attrib_id(material);
		return 1;
	}

	return 0;
}

void GPU_material_free(GPUMaterial *material)
{
	if(material->pass)
		GPU_pass_free(material->pass);

	GPU_nodes_free(&material->nodes);
	MEM_freeN(material);
}

void GPU_material_bind(GPUMaterial *material)
{
	if(material->pass) {
		GPU_pass_bind(material->pass);

		if(material->alpha) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		}
	}
}

void GPU_material_bind_uniforms(GPUMaterial *material, float obmat[][4], float viewmat[][4])
{
	if(material->pass) {
		GPUShader *shader = GPU_pass_shader(material->pass);

		GPU_shader_uniform_vector(shader, "unfobmat", 16, 1, (float*)obmat);
		GPU_shader_uniform_vector(shader, "unfviewmat", 16, 1, (float*)viewmat);
	}
}

void GPU_material_unbind(GPUMaterial *material)
{
	if (material->pass) {
		if(material->alpha)
			glDisable(GL_BLEND);

		GPU_pass_unbind(material->pass);
	}
}

void GPU_material_vertex_attributes(GPUMaterial *material, GPUVertexAttribs *attribs)
{
	*attribs = material->attribs;
}

void GPU_material_output_link(GPUMaterial *material, GPUNodeLink *link)
{
	if(!material->outlink)
		material->outlink= link;
}

void GPU_material_enable_alpha(GPUMaterial *material)
{
	material->alpha= 1;
}

void gpu_material_add_node(GPUMaterial *material, GPUNode *node)
{
	BLI_addtail(&material->nodes, node);
}

/* Code generation */

static GPUNodeLink *lamp_get_visibility(GPUMaterial *mat, Object *lampob, Lamp *la, GPUNodeLink **lv, GPUNodeLink **dist)
{
	GPUNodeLink *visifac, *inpr;
	float *lampco, *lampvec;
	float lampimat[3][3], lampspotsi, lampspotbl;

	/* from add_render_lamp */
	lampvec= lampob->obmat[2];
	lampco= lampob->obmat[3];

	lampspotsi= la->spotsize;
	if(la->mode & LA_HALO)
		if(lampspotsi > 170.0)
			lampspotsi = 170.0;
	lampspotsi= cos(M_PI*lampspotsi/360.0);
	lampspotbl= (1.0 - lampspotsi)*la->spotblend;

	/* TODO */
	Mat3One(lampimat);

	/* from get_lamp_visibility */
	if(la->type==LA_SUN || la->type==LA_HEMI) {
		GPU_link(mat, "lamp_visibility_sun_hemi", GPU_dynamic_uniform(lampvec), lv, dist, &visifac);
		return visifac;
	}
	else {
		GPU_link(mat, "lamp_visibility_other", GPU_dynamic_uniform(lampco), lv, dist, &visifac);

		if(la->type==LA_AREA)
			return visifac;

		switch(la->falloff_type)
		{
			case LA_FALLOFF_CONSTANT:
				break;
			case LA_FALLOFF_INVLINEAR:
				GPU_link(mat, "lamp_falloff_invlinear", GPU_uniform(&la->dist), *dist, &visifac);
				break;
			case LA_FALLOFF_INVSQUARE:
				GPU_link(mat, "lamp_falloff_invsquare", GPU_uniform(&la->dist), *dist, &visifac);
				break;
			case LA_FALLOFF_SLIDERS:
				GPU_link(mat, "lamp_falloff_sliders", GPU_uniform(&la->dist), GPU_uniform(&la->att1), GPU_uniform(&la->att2), *dist, &visifac);
				break;
			case LA_FALLOFF_CURVE:
				{
					float *array;
					int size;

					curvemapping_table_RGBA(la->curfalloff, &array, &size);
					GPU_link(mat, "lamp_falloff_curve", GPU_uniform(&la->dist), GPU_texture(size, array), *dist, &visifac);
				}
				break;
		}

		if(la->mode & LA_SPHERE)
			GPU_link(mat, "lamp_visibility_sphere", GPU_uniform(&la->dist), *dist, visifac, &visifac);

		if(la->type == LA_SPOT) {
			if(la->mode & LA_SQUARE)
				GPU_link(mat, "lamp_visibility_spot_square", GPU_dynamic_uniform(lampvec), GPU_uniform((float*)lampimat), *lv, &inpr);
			else
				GPU_link(mat, "lamp_visibility_spot_circle", GPU_dynamic_uniform(lampvec), *lv, &inpr);
			
			GPU_link(mat, "lamp_visibility_spot", GPU_uniform(&lampspotsi), GPU_uniform(&lampspotbl), inpr, visifac, &visifac);
		}

		GPU_link(mat, "lamp_visibility_clamp", visifac, &visifac);

		return visifac;
	}
}

#if 0
static void area_lamp_vectors(LampRen *lar)
{
    float xsize= 0.5*lar->area_size, ysize= 0.5*lar->area_sizey, multifac;

    /* make it smaller, so area light can be multisampled */
    multifac= 1.0f/sqrt((float)lar->ray_totsamp);
    xsize *= multifac;
    ysize *= multifac;

    /* corner vectors */
    lar->area[0][0]= lar->co[0] - xsize*lar->mat[0][0] - ysize*lar->mat[1][0];
    lar->area[0][1]= lar->co[1] - xsize*lar->mat[0][1] - ysize*lar->mat[1][1];
    lar->area[0][2]= lar->co[2] - xsize*lar->mat[0][2] - ysize*lar->mat[1][2];

    /* corner vectors */
    lar->area[1][0]= lar->co[0] - xsize*lar->mat[0][0] + ysize*lar->mat[1][0];
    lar->area[1][1]= lar->co[1] - xsize*lar->mat[0][1] + ysize*lar->mat[1][1];
    lar->area[1][2]= lar->co[2] - xsize*lar->mat[0][2] + ysize*lar->mat[1][2];

    /* corner vectors */
    lar->area[2][0]= lar->co[0] + xsize*lar->mat[0][0] + ysize*lar->mat[1][0];
    lar->area[2][1]= lar->co[1] + xsize*lar->mat[0][1] + ysize*lar->mat[1][1];
    lar->area[2][2]= lar->co[2] + xsize*lar->mat[0][2] + ysize*lar->mat[1][2];

    /* corner vectors */
    lar->area[3][0]= lar->co[0] + xsize*lar->mat[0][0] - ysize*lar->mat[1][0];
    lar->area[3][1]= lar->co[1] + xsize*lar->mat[0][1] - ysize*lar->mat[1][1];
    lar->area[3][2]= lar->co[2] + xsize*lar->mat[0][2] - ysize*lar->mat[1][2];
    /* only for correction button size, matrix size works on energy */
    lar->areasize= lar->dist*lar->dist/(4.0*xsize*ysize);
}
#endif

static void ramp_blend(GPUMaterial *mat, GPUNodeLink *fac, GPUNodeLink *col1, GPUNodeLink *col2, int type, GPUNodeLink **outcol)
{
	static char *names[] = {"mix_blend", "mix_add", "mix_mult", "mix_sub",
		"mix_screen", "mix_div", "mix_diff", "mix_dark", "mix_light",
		"mix_overlay", "mix_dodge", "mix_burn", "mix_hue", "mix_sat",
		"mix_val", "mix_color"};

	GPU_link(mat, names[type], fac, col1, col2, outcol);
}

static void do_colorband_blend(GPUMaterial *mat, ColorBand *coba, GPUNodeLink *fac, float rampfac, int type, GPUNodeLink *incol, GPUNodeLink **outcol)
{
	GPUNodeLink *tmp, *alpha, *col;
	float *array;
	int size;

	/* do colorband */
	colorband_table_RGBA(coba, &array, &size);
	GPU_link(mat, "valtorgb", fac, GPU_texture(size, array), &col, &tmp);

	/* use alpha in fac */
	GPU_link(mat, "mtex_alpha_from_col", col, &alpha);
	GPU_link(mat, "math_multiply", alpha, GPU_uniform(&rampfac), &fac);

	/* blending method */
	ramp_blend(mat, fac, incol, col, type, outcol);
}

static void ramp_diffuse_result(GPUShadeInput *shi, GPUNodeLink **diff)
{
	Material *ma= shi->mat;
	GPUMaterial *mat= shi->gpumat;
	GPUNodeLink *fac;

	if(ma->ramp_col) {
		if(ma->rampin_col==MA_RAMP_IN_RESULT) {
			GPU_link(mat, "ramp_rgbtobw", *diff, &fac);
			
			/* colorband + blend */
			do_colorband_blend(mat, ma->ramp_col, fac, ma->rampfac_col, ma->rampblend_col, *diff, diff);
		}
	}
}

static void add_to_diffuse(GPUMaterial *mat, Material *ma, GPUShadeInput *shi, GPUNodeLink *is, GPUNodeLink *rgb, GPUNodeLink **diff)
{
	GPUNodeLink *fac, *tmp, *addcol;
	
	if(ma->ramp_col && (ma->mode & MA_RAMP_COL)) {
		/* MA_RAMP_IN_RESULT is exceptional */
		if(ma->rampin_col==MA_RAMP_IN_RESULT) {
			addcol = shi->rgb;
		}
		else {
			/* input */
			switch(ma->rampin_col) {
			case MA_RAMP_IN_ENERGY:
				GPU_link(mat, "ramp_rgbtobw", rgb, &fac);
				break;
			case MA_RAMP_IN_SHADER:
				fac= is;
				break;
			case MA_RAMP_IN_NOR:
				GPU_link(mat, "vec_math_dot", shi->view, shi->vn, &tmp, &fac);
				break;
			default:
				GPU_link(mat, "set_value_zero", &fac);
				break;
			}

			/* colorband + blend */
			do_colorband_blend(mat, ma->ramp_col, fac, ma->rampfac_col, ma->rampblend_col, shi->rgb, &addcol);
		}
	}
	else
		addcol = shi->rgb;

	/* output to */
	GPU_link(mat, "shade_madd", *diff, rgb, addcol, diff);
}

static void ramp_spec_result(GPUShadeInput *shi, GPUNodeLink **spec)
{
	Material *ma= shi->mat;
	GPUMaterial *mat= shi->gpumat;
	GPUNodeLink *fac;

	if(ma->ramp_spec && ma->rampin_spec==MA_RAMP_IN_RESULT) {
		GPU_link(mat, "ramp_rgbtobw", *spec, &fac);
		
		/* colorband + blend */
		do_colorband_blend(mat, ma->ramp_spec, fac, ma->rampfac_spec, ma->rampblend_spec, *spec, spec);
	}
}

static void do_specular_ramp(GPUShadeInput *shi, GPUNodeLink *is, GPUNodeLink *t, GPUNodeLink **spec)
{
	Material *ma= shi->mat;
	GPUMaterial *mat= shi->gpumat;
	GPUNodeLink *fac, *tmp;

	*spec = shi->specrgb;

	/* MA_RAMP_IN_RESULT is exception */
	if(ma->ramp_spec && (ma->rampin_spec!=MA_RAMP_IN_RESULT)) {
		
		/* input */
		switch(ma->rampin_spec) {
		case MA_RAMP_IN_ENERGY:
			fac = t;
			break;
		case MA_RAMP_IN_SHADER:
			fac = is;
			break;
		case MA_RAMP_IN_NOR:
			GPU_link(mat, "vec_math_dot", shi->view, shi->vn, &tmp, &fac);
			break;
		default:
			GPU_link(mat, "set_value_zero", &fac);
			break;
		}
		
		/* colorband + blend */
		do_colorband_blend(mat, ma->ramp_spec, fac, ma->rampfac_spec, ma->rampblend_spec, *spec, spec);
	}
}

static void shade_one_light(GPUShadeInput *shi, GPUShadeResult *shr, Object *lampob, Lamp *la)
{
	Material *ma= shi->mat;
	GPUMaterial *mat= shi->gpumat;
	GPUNodeLink *lv, *dist, *visifac, *is, *inp, *i, *vn, *view;
	GPUNodeLink *outcol, *specfac, *t;
	float energy, lampcol[3];
	float *lampvec, *lampco;

	if((la->mode & LA_ONLYSHADOW) && !(ma->mode & MA_SHADOW))
		return;

	lampvec= lampob->obmat[2];
	lampco= lampob->obmat[3];

	vn= shi->vn;
	view= shi->view;

	visifac= lamp_get_visibility(mat, lampob, la, &lv, &dist);

	energy= la->energy;
	if(la->mode & LA_NEG) energy= -energy;

	lampcol[0]= la->r*energy;
	lampcol[1]= la->g*energy;
	lampcol[2]= la->b*energy;

	/*if(ma->mode & MA_TANGENT_V)
		GPU_link(mat, "shade_tangent_v", lv, GPU_attribute(CD_TANGENT, ""), &vn);*/
	
	GPU_link(mat, "shade_inp", vn, lv, &inp);

	if(la->mode & LA_NO_DIFF) {
		GPU_link(mat, "shade_is_no_diffuse", &is);
	}
	else if(la->type == LA_HEMI) {
		GPU_link(mat, "shade_is_hemi", inp, &is);
	}
	else {
		if(la->type == LA_AREA) {
			float area[4][4], areasize;

			memset(&area, 0, sizeof(area));
			memset(&areasize, 0, sizeof(areasize));
			GPU_link(mat, "shade_inp_area", GPU_dynamic_uniform(lampco), GPU_dynamic_uniform(lampvec), vn, GPU_uniform((float*)area),
				GPU_uniform(&areasize), GPU_uniform(&la->k), &inp);
		}

		if(ma->diff_shader==MA_DIFF_ORENNAYAR)
			GPU_link(mat, "shade_diffuse_oren_nayer", inp, vn, lv, view, GPU_uniform(&ma->roughness), &is);
		else if(ma->diff_shader==MA_DIFF_TOON)
			GPU_link(mat, "shade_diffuse_toon", vn, lv, view, GPU_uniform(&ma->param[0]), GPU_uniform(&ma->param[1]), &is);
		else if(ma->diff_shader==MA_DIFF_MINNAERT)
			GPU_link(mat, "shade_diffuse_minnaert", inp, vn, view, GPU_uniform(&ma->darkness), &is);
		else if(ma->diff_shader==MA_DIFF_FRESNEL)
			GPU_link(mat, "shade_diffuse_fresnel", vn, lv, view, GPU_uniform(&ma->param[0]), GPU_uniform(&ma->param[1]), &is);
		else
			is= inp; /* Lambert */
	}

	if(ma->shade_flag & MA_CUBIC)
		GPU_link(mat, "shade_cubic", is, &is);
	
	i = is;
	GPU_link(mat, "shade_visifac", i, visifac, shi->refl, &i);

	vn = shi->vn;
	/*if(ma->mode & MA_TANGENT_VN)
		GPU_link(mat, "shade_tangent_v_spec", GPU_attribute(CD_TANGENT, ""), &vn);*/
	
	if(!(la->mode & LA_NO_DIFF)) {
		GPUNodeLink *rgb;
		GPU_link(mat, "shade_mul_value", i, GPU_uniform(lampcol), &rgb);
		add_to_diffuse(mat, ma, shi, is, rgb, &shr->diff);
	}

	if(!(la->mode & LA_NO_SPEC) && !(la->mode & LA_ONLYSHADOW)) {
		if(la->type == LA_HEMI) {
			GPU_link(mat, "shade_hemi_spec", vn, lv, view, GPU_uniform(&ma->spec), shi->har, visifac, &t);
			GPU_link(mat, "shade_add_spec", t, GPU_uniform(lampcol), shi->specrgb, &outcol);
			GPU_link(mat, "shade_add", shr->spec, outcol, &shr->spec);
		}
		else {
			if(ma->spec_shader==MA_SPEC_PHONG)
				GPU_link(mat, "shade_phong_spec", vn, lv, view, shi->har, &specfac);
			else if(ma->spec_shader==MA_SPEC_COOKTORR)
				GPU_link(mat, "shade_cooktorr_spec", vn, lv, view, shi->har, &specfac);
			else if(ma->spec_shader==MA_SPEC_BLINN)
				GPU_link(mat, "shade_blinn_spec", vn, lv, view, GPU_uniform(&ma->refrac), shi->har, &specfac);
			else if(ma->spec_shader==MA_SPEC_WARDISO)
				GPU_link(mat, "shade_wardiso_spec", vn, lv, view, GPU_uniform(&ma->rms), &specfac);
			else
				GPU_link(mat, "shade_toon_spec", vn, lv, view, GPU_uniform(&ma->param[2]), GPU_uniform(&ma->param[3]), &specfac);

			if(la->type==LA_AREA)
				GPU_link(mat, "shade_spec_area_inp", specfac, inp, &specfac);

			GPU_link(mat, "shade_spec_t", shi->spec, visifac, specfac, &t);

			if(ma->mode & MA_RAMP_SPEC) {
				GPUNodeLink *spec;
				do_specular_ramp(shi, specfac, t, &spec);
				GPU_link(mat, "shade_add_spec", t, GPU_uniform(lampcol), spec, &outcol);
				GPU_link(mat, "shade_add", shr->spec, outcol, &shr->spec);
			}
			else {
				GPU_link(mat, "shade_add_spec", t, GPU_uniform(lampcol), shi->specrgb, &outcol);
				GPU_link(mat, "shade_add", shr->spec, outcol, &shr->spec);
			}
		}
	}
}

static void material_lights(GPUShadeInput *shi, GPUShadeResult *shr)
{
	Base *base;
	Object *lampob;
	Lamp *la;
	
	for(base=G.scene->base.first; base; base=base->next) {
		if(base->object->type==OB_LAMP) {
			if(G.vd && base->lay & G.vd->lay) {
				lampob= base->object;
				la= lampob->data;

				shade_one_light(shi, shr, lampob, la);
			}
		}
	}
}

static void texture_rgb_blend(GPUMaterial *mat, GPUNodeLink *tex, GPUNodeLink *out, GPUNodeLink *fact, GPUNodeLink *facg, int blendtype, GPUNodeLink **in)
{
	switch(blendtype) {
	case MTEX_BLEND:
		GPU_link(mat, "mtex_rgb_blend", out, tex, fact, facg, in);
		break;
	case MTEX_MUL:
		GPU_link(mat, "mtex_rgb_mul", out, tex, fact, facg, in);
		break;
	case MTEX_SCREEN:
		GPU_link(mat, "mtex_rgb_screen", out, tex, fact, facg, in);
		break;
	case MTEX_OVERLAY:
		GPU_link(mat, "mtex_rgb_overlay", out, tex, fact, facg, in);
		break;
	case MTEX_SUB:
		GPU_link(mat, "mtex_rgb_sub", out, tex, fact, facg, in);
		break;
	case MTEX_ADD:
		GPU_link(mat, "mtex_rgb_add", out, tex, fact, facg, in);
		break;
	case MTEX_DIV:
		GPU_link(mat, "mtex_rgb_div", out, tex, fact, facg, in);
		break;
	case MTEX_DIFF:
		GPU_link(mat, "mtex_rgb_diff", out, tex, fact, facg, in);
		break;
	case MTEX_DARK:
		GPU_link(mat, "mtex_rgb_dark", out, tex, fact, facg, in);
		break;
	case MTEX_LIGHT:
		GPU_link(mat, "mtex_rgb_light", out, tex, fact, facg, in);
		break;
	case MTEX_BLEND_HUE:
		GPU_link(mat, "mtex_rgb_hue", out, tex, fact, facg, in);
		break;
	case MTEX_BLEND_SAT:
		GPU_link(mat, "mtex_rgb_sat", out, tex, fact, facg, in);
		break;
	case MTEX_BLEND_VAL:
		GPU_link(mat, "mtex_rgb_val", out, tex, fact, facg, in);
		break;
	case MTEX_BLEND_COLOR:
		GPU_link(mat, "mtex_rgb_color", out, tex, fact, facg, in);
		break;
	default:
		GPU_link(mat, "set_rgb_zero", &in);
		break;
	}
}

static void texture_value_blend(GPUMaterial *mat, GPUNodeLink *tex, GPUNodeLink *out, GPUNodeLink *fact, GPUNodeLink *facg, int blendtype, int flip, GPUNodeLink **in)
{
	float flipf = (flip)? 1.0f: 0.0;

	switch(blendtype) {
	case MTEX_BLEND:
		GPU_link(mat, "mtex_value_blend", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_MUL:
		GPU_link(mat, "mtex_value_mul", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_SCREEN:
		GPU_link(mat, "mtex_value_screen", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_SUB:
		GPU_link(mat, "mtex_value_sub", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_ADD:
		GPU_link(mat, "mtex_value_add", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_DIV:
		GPU_link(mat, "mtex_value_div", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_DIFF:
		GPU_link(mat, "mtex_value_diff", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_DARK:
		GPU_link(mat, "mtex_value_dark", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	case MTEX_LIGHT:
		GPU_link(mat, "mtex_value_light", out, tex, fact, facg, GPU_uniform(&flipf), in);
		break;
	default:
		GPU_link(mat, "set_value_zero", &in);
		break;
	}
}

static void do_material_tex(GPUShadeInput *shi)
{
	Material *ma= shi->mat;
	GPUMaterial *mat= shi->gpumat;
	MTex *mtex;
	Tex *tex;
	GPUNodeLink *texco, *tin, *trgb, *tnor, *tcol, *stencil = NULL;
	GPUNodeLink *colfac, *newnor, *varfac, *orn;
	float one = 1.0f;
	int tex_nr, rgbnor, talpha;

	GPU_link(mat, "texco_norm", &orn);
	GPU_link(mat, "set_value", GPU_uniform(&one), &stencil);

	/* go over texture slots */
	for(tex_nr=0; tex_nr<MAX_MTEX; tex_nr++) {
		/* separate tex switching */
		if(ma->septex & (1<<tex_nr)) continue;
		
		if(ma->mtex[tex_nr]) {
			mtex= ma->mtex[tex_nr];
			
			tex= mtex->tex;
			if(tex==0) continue;

			/* which coords */
			if(mtex->texco==TEXCO_ORCO)
				GPU_link(mat, "texco_orco", GPU_attribute(CD_ORCO, ""), &texco);
			else if(mtex->texco==TEXCO_NORM)
				texco= orn;
			else if(mtex->texco==TEXCO_UV)
				GPU_link(mat, "texco_uv", GPU_attribute(CD_MTFACE, mtex->uvname), &texco);
			else
				continue;

			GPU_link(mat, "mtex_2d_mapping", texco, &texco);
			GPU_link(mat, "mtex_mapping", texco, GPU_uniform(mtex->size), GPU_uniform(mtex->ofs), &texco);

			if(tex && tex->type == TEX_IMAGE && tex->ima) {
				GPU_link(mat, "mtex_image", texco, GPU_image(tex->ima, NULL), &tin, &trgb, &tnor);
				rgbnor= TEX_RGB;
				talpha= 1;
		    }
			else continue;

			/* texture output */
			if((rgbnor & TEX_RGB) && (mtex->texflag & MTEX_RGBTOINT)) {
				GPU_link(mat, "mtex_rgbtoint", trgb, &tin);
				rgbnor -= TEX_RGB;
			}

			if(mtex->texflag & MTEX_NEGATIVE) {
				if(rgbnor & TEX_RGB)
					GPU_link(mat, "mtex_rgb_invert", trgb, &trgb);
				else
					GPU_link(mat, "mtex_value_invert", tin, &tin);
			}

			if(mtex->texflag & MTEX_STENCIL) {
				if(rgbnor & TEX_RGB)
					GPU_link(mat, "mtex_rgb_stencil", stencil, trgb, &stencil, &trgb);
				else
					GPU_link(mat, "mtex_value_stencil", stencil, tin, &stencil, &tin);
			}

			/* mapping */
			if(mtex->mapto & (MAP_COL+MAP_COLSPEC)) {
				/* stencil maps on the texture control slider, not texture intensity value */
				GPU_link(mat, "math_multiply", GPU_uniform(&mtex->colfac), stencil, &colfac);

				if((rgbnor & TEX_RGB)==0) {
					GPU_link(mat, "set_rgb", GPU_uniform(&mtex->r), &tcol);
				}
				else {
					GPU_link(mat, "set_rgba", trgb, &tcol);

					if(mtex->mapto & MAP_ALPHA)
						GPU_link(mat, "set_value", stencil, &tin);
					else
						GPU_link(mat, "mtex_alpha_from_col", trgb, &tin);
				}
				
				if(mtex->mapto & MAP_COL)
					texture_rgb_blend(mat, tcol, shi->rgb, tin, colfac, mtex->blendtype, &shi->rgb);
				if(mtex->mapto & MAP_COLSPEC)
					texture_rgb_blend(mat, tcol, shi->specrgb, tin, colfac, mtex->blendtype, &shi->specrgb);
			}

			if(mtex->mapto & MAP_NORM) {
				if(mtex->maptoneg & MAP_NORM) tex->norfac= -mtex->norfac;
				else tex->norfac= mtex->norfac;
				
				if((tex->type==TEX_IMAGE) && (tex->imaflag & TEX_NORMALMAP)) {
					tex->norfac = mtex->norfac;

					if(mtex->maptoneg & MAP_NORM)
						GPU_link(mat, "mtex_negate_texnormal", tnor, &tnor);

					if(mtex->normapspace == MTEX_NSPACE_TANGENT)
						GPU_link(mat, "mtex_nspace_tangent", GPU_attribute(CD_TANGENT, ""), shi->vn, tnor, &newnor);
					else
						newnor = tnor;

					GPU_link(mat, "mtex_blend_normal", GPU_uniform(&mtex->norfac), shi->vn, newnor, &shi->vn);
				}

				GPU_link(mat, "vec_math_negate", shi->vn, &orn);
			}

			if(mtex->mapto & MAP_VARS) {
				GPU_link(mat, "math_multiply", GPU_uniform(&mtex->varfac), stencil, &varfac);

				if(rgbnor & TEX_RGB) {
					if(talpha)
						GPU_link(mat, "mtex_alpha_from_col", trgb, &tin);
					else
						GPU_link(mat, "mtex_rgbtoint", trgb, &tin);
				}

				if(mtex->mapto & MAP_REF) {
					int flip= mtex->maptoneg & MAP_REF;
					texture_value_blend(mat, GPU_uniform(&mtex->def_var), shi->refl, tin, varfac, mtex->blendtype, flip, &shi->refl);
					GPU_link(mat, "mtex_value_clamp_positive", shi->refl, &shi->refl);
				}
				if(mtex->mapto & MAP_SPEC) {
					int flip= mtex->maptoneg & MAP_SPEC;
					texture_value_blend(mat, GPU_uniform(&mtex->def_var), shi->spec, tin, varfac, mtex->blendtype, flip, &shi->spec);
					GPU_link(mat, "mtex_value_clamp_positive", shi->spec, &shi->spec);
				}
				if(mtex->mapto & MAP_EMIT) {
					int flip= mtex->maptoneg & MAP_EMIT;
					texture_value_blend(mat, GPU_uniform(&mtex->def_var), shi->emit, tin, varfac, mtex->blendtype, flip, &shi->emit);
					GPU_link(mat, "mtex_value_clamp_positive", shi->emit, &shi->emit);
				}
				if(mtex->mapto & MAP_HAR) {
					int flip= mtex->maptoneg & MAP_HAR;
					GPU_link(mat, "mtex_har_divide", shi->har, &shi->har);
					texture_value_blend(mat, GPU_uniform(&mtex->def_var), shi->har, tin, varfac, mtex->blendtype, flip, &shi->har);
					GPU_link(mat, "mtex_har_multiply_clamp", shi->har, &shi->har);
				}
				if(mtex->mapto & MAP_ALPHA) {
					int flip= mtex->maptoneg & MAP_ALPHA;
					texture_value_blend(mat, GPU_uniform(&mtex->def_var), shi->alpha, tin, varfac, mtex->blendtype, flip, &shi->alpha);
					GPU_link(mat, "mtex_value_clamp", shi->alpha, &shi->alpha);
				}
				if(mtex->mapto & MAP_AMB) {
					int flip= mtex->maptoneg & MAP_AMB;
					texture_value_blend(mat, GPU_uniform(&mtex->def_var), shi->amb, tin, varfac, mtex->blendtype, flip, &shi->amb);
					GPU_link(mat, "mtex_value_clamp", shi->amb, &shi->amb);
				}
			}
		}
	}
}

void GPU_shadeinput_set(GPUMaterial *mat, Material *ma, GPUShadeInput *shi)
{
	float hard = ma->har;

	memset(shi, 0, sizeof(*shi));

	shi->gpumat = mat;
	shi->mat = ma;

	GPU_link(mat, "set_rgb", GPU_uniform(&ma->r), &shi->rgb);
	GPU_link(mat, "set_rgb", GPU_uniform(&ma->specr), &shi->specrgb);
	GPU_link(mat, "shade_norm", &shi->vn);
	GPU_link(mat, "set_value", GPU_uniform(&ma->alpha), &shi->alpha);
	GPU_link(mat, "set_value", GPU_uniform(&ma->ref), &shi->refl);
	GPU_link(mat, "set_value", GPU_uniform(&ma->spec), &shi->spec);
	GPU_link(mat, "set_value", GPU_uniform(&ma->emit), &shi->emit);
	GPU_link(mat, "set_value", GPU_uniform(&hard), &shi->har);
	GPU_link(mat, "set_value", GPU_uniform(&ma->amb), &shi->amb);
	GPU_link(mat, "shade_view", &shi->view);
}

void GPU_shaderesult_set(GPUShadeInput *shi, GPUShadeResult *shr)
{
	Material *ma= shi->mat;
	GPUMaterial *mat= shi->gpumat;

	memset(shr, 0, sizeof(*shr));

	do_material_tex(shi);

	if(ma->alpha < 1.0f)
		GPU_material_enable_alpha(mat);

	if(ma->mode & MA_SHLESS) {
		shr->combined = shi->rgb;
		shr->alpha = shi->alpha;
	}
	else {
		GPU_link(mat, "shade_mul_value", shi->emit, shi->rgb, &shr->diff);
		GPU_link(mat, "set_rgb_zero", &shr->spec);

		material_lights(shi, shr);

		shr->combined = shr->diff;
		shr->alpha = shi->alpha;

		if(ma->mode & MA_RAMP_COL) ramp_diffuse_result(shi, &shr->combined);
		if(ma->mode & MA_RAMP_SPEC) ramp_spec_result(shi, &shr->spec);

		GPU_link(mat, "shade_add", shr->combined, shr->spec, &shr->combined);
	}

	GPU_link(mat, "mtex_alpha_to_col", shr->combined, shr->alpha, &shr->combined);
}

GPUNodeLink *GPU_blender_material(GPUMaterial *mat, Material *ma)
{
	GPUShadeInput shi;
	GPUShadeResult shr;

	GPU_shadeinput_set(mat, ma, &shi);
	GPU_shaderesult_set(&shi, &shr);

	return shr.combined;
}

GPUMaterial *GPU_material_from_blender(Material *ma, int profile)
{
	GPUMaterial *mat;
	GPUNodeLink *outlink;

	if(ma->gpumaterial)
		return ma->gpumaterial;

	mat = GPU_material_construct_begin(ma, profile);

	if(ma->nodetree && ma->use_nodes) {
		ntreeGPUMaterialNodes(ma->nodetree, mat);
	}
	else {
		outlink = GPU_blender_material(mat, ma);
		GPU_material_output_link(mat, outlink);
	}

	if(!GPU_material_construct_end(mat)) {
		GPU_material_free(mat);
		mat= NULL;
	}

	ma->gpumaterial= mat;
	return ma->gpumaterial;
}

