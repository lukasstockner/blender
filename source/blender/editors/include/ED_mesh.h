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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_MESH_H
#define ED_MESH_H

struct ID;
struct View3D;
struct ARegion;
struct EditMesh;
struct EditVert;
struct EditEdge;
struct EditFace;
struct bContext;
struct wmOperator;
struct wmWindowManager;
struct wmKeyConfig;
struct ReportList;
struct EditSelection;
struct ViewContext;
struct bDeformGroup;
struct MDeformWeight;
struct MDeformVert;
struct Scene;
struct Mesh;
struct MCol;
struct UvVertMap;
struct UvMapVert;
struct CustomData;
struct Material;
struct Object;
struct rcti;

#define EM_FGON_DRAW	1 // face flag
#define EM_FGON			2 // edge and face flag both

/* editbutflag */
#define B_CLOCKWISE			1
#define B_KEEPORIG			2
#define B_BEAUTY			4
#define B_SMOOTH			8
#define B_BEAUTY_SHORT  	0x10
#define B_AUTOFGON			0x20
#define B_KNIFE				0x80
#define B_PERCENTSUBD		0x40
//#define B_MESH_X_MIRROR		0x100 // deprecated, use mesh
#define B_JOINTRIA_UV		0x200
#define B_JOINTRIA_VCOL		0X400
#define B_JOINTRIA_SHARP	0X800
#define B_JOINTRIA_MAT		0X1000
#define B_FRACTAL			0x2000
#define B_SPHERE			0x4000

/* meshtools.c */

intptr_t	mesh_octree_table(struct Object *ob, struct EditMesh *em, float *co, char mode);
long		mesh_mirrtopo_table(struct Object *ob, char mode);

struct EditVert   *editmesh_get_x_mirror_vert(struct Object *ob, struct EditMesh *em, struct EditVert *eve, float *co, int index);
int			mesh_get_x_mirror_vert(struct Object *ob, int index);
int			*mesh_get_x_mirror_faces(struct Object *ob, struct EditMesh *em);

int			join_mesh_exec(struct bContext *C, struct wmOperator *op);
int			join_mesh_shapes_exec(struct bContext *C, struct wmOperator *op);

/* mesh_ops.c */
void		ED_operatortypes_mesh(void);
void		ED_operatormacros_mesh(void);
void		ED_keymap_mesh(struct wmKeyConfig *keyconf);


/* editmesh.c */

void		ED_spacetypes_init(void);
void		ED_keymap_mesh(struct wmKeyConfig *keyconf);

void		make_editMesh(struct Scene *scene, struct Object *ob);
void		load_editMesh(struct Scene *scene, struct Object *ob);
void		remake_editMesh(struct Scene *scene, struct Object *ob);
void		free_editMesh(struct EditMesh *em);

void		recalc_editnormals(struct EditMesh *em);

void		EM_init_index_arrays(struct EditMesh *em, int forVert, int forEdge, int forFace);
void		EM_free_index_arrays(void);
struct EditVert	*EM_get_vert_for_index(int index);
struct EditEdge	*EM_get_edge_for_index(int index);
struct EditFace	*EM_get_face_for_index(int index);
int			EM_texFaceCheck(struct EditMesh *em);
int			EM_vertColorCheck(struct EditMesh *em);

void		undo_push_mesh(struct bContext *C, char *name);


/* editmesh_lib.c */

struct EditFace	*EM_get_actFace(struct EditMesh *em, int sloppy);
void             EM_set_actFace(struct EditMesh *em, struct EditFace *efa);
float            EM_face_area(struct EditFace *efa);

void		EM_select_edge(struct EditEdge *eed, int sel);
void		EM_select_face(struct EditFace *efa, int sel);
void		EM_select_face_fgon(struct EditMesh *em, struct EditFace *efa, int val);
void		EM_select_swap(struct EditMesh *em);
void		EM_toggle_select_all(struct EditMesh *em);
void		EM_select_all(struct EditMesh *em);
void		EM_deselect_all(struct EditMesh *em);
void		EM_selectmode_flush(struct EditMesh *em);
void		EM_deselect_flush(struct EditMesh *em);
void		EM_selectmode_set(struct EditMesh *em);
void		EM_select_flush(struct EditMesh *em);
void		EM_convertsel(struct EditMesh *em, short oldmode, short selectmode);
void		EM_validate_selections(struct EditMesh *em);
void		EM_selectmode_to_scene(struct Scene *scene, struct Object *obedit);

			/* exported to transform */
int			EM_get_actSelection(struct EditMesh *em, struct EditSelection *ese);
void		EM_editselection_normal(float *normal, struct EditSelection *ese);
void		EM_editselection_plane(float *plane, struct EditSelection *ese);
void		EM_editselection_center(float *center, struct EditSelection *ese);			

struct UvVertMap *EM_make_uv_vert_map(struct EditMesh *em, int selected, int do_face_idx_array, float *limit);
struct UvMapVert *EM_get_uv_map_vert(struct UvVertMap *vmap, unsigned int v);
void              EM_free_uv_vert_map(struct UvVertMap *vmap);

void		EM_add_data_layer(struct EditMesh *em, struct CustomData *data, int type);
void		EM_free_data_layer(struct EditMesh *em, struct CustomData *data, int type);

void		EM_make_hq_normals(struct EditMesh *em);
void		EM_solidify(struct EditMesh *em, float dist);

int			EM_deselect_nth(struct EditMesh *em, int nth);

/* editmesh_mods.c */
extern unsigned int em_vertoffs, em_solidoffs, em_wireoffs;

void		EM_cache_x_mirror_vert(struct Object *ob, struct EditMesh *em);
int			mouse_mesh(struct bContext *C, short mval[2], short extend);
int			EM_check_backbuf(unsigned int index);
int			EM_mask_init_backbuf_border(struct ViewContext *vc, short mcords[][2], short tot, short xmin, short ymin, short xmax, short ymax);
void		EM_free_backbuf(void);
int			EM_init_backbuf_border(struct ViewContext *vc, short xmin, short ymin, short xmax, short ymax);
int			EM_init_backbuf_circle(struct ViewContext *vc, short xs, short ys, short rads);

void		EM_hide_mesh(struct EditMesh *em, int swap);
void		EM_reveal_mesh(struct EditMesh *em);

void		EM_select_by_material(struct EditMesh *em, int index);
void		EM_deselect_by_material(struct EditMesh *em, int index); 

void		EM_automerge(struct Scene *scene, struct Object *obedit, int update);

/* editface.c */
struct MTFace	*EM_get_active_mtface(struct EditMesh *em, struct EditFace **act_efa, struct MCol **mcol, int sloppy);
int face_select(struct bContext *C, struct Object *ob, short mval[2], int extend);
void face_borderselect(struct bContext *C, struct Object *ob, struct rcti *rect, int select, int extend);
void selectall_tface(struct Object *ob, int action);
void select_linked_tfaces(struct bContext *C, struct Object *ob, short mval[2], int mode);
int minmax_tface(struct Object *ob, float *min, float *max);

/* object_vgroup.c */

#define WEIGHT_REPLACE 1
#define WEIGHT_ADD 2
#define WEIGHT_SUBTRACT 3

struct bDeformGroup		*ED_vgroup_add(struct Object *ob);
struct bDeformGroup		*ED_vgroup_add_name(struct Object *ob, char *name);
void					ED_vgroup_select_by_name(struct Object *ob, char *name);
void					ED_vgroup_data_create(struct ID *id);
int						ED_vgroup_give_array(struct ID *id, struct MDeformVert **dvert_arr, int *dvert_tot);
int						ED_vgroup_copy_array(struct Object *ob, struct Object *ob_from);
void					ED_vgroup_mirror(struct Object *ob, int mirror_weights, int flip_vgroups);

void		ED_vgroup_vert_add(struct Object *ob, struct bDeformGroup *dg, int vertnum,  float weight, int assignmode);
void		ED_vgroup_vert_remove(struct Object *ob, struct bDeformGroup *dg, int vertnum);
float		ED_vgroup_vert_weight(struct Object *ob, struct bDeformGroup *dg, int vertnum);

/*needed by edge slide*/
struct EditVert *editedge_getOtherVert(struct EditEdge *eed, struct EditVert *eve);
struct EditVert *editedge_getSharedVert(struct EditEdge *eed, struct EditEdge *eed2);
int editedge_containsVert(struct EditEdge *eed, struct EditVert *eve);
int editface_containsVert(struct EditFace *efa, struct EditVert *eve);
int editface_containsEdge(struct EditFace *efa, struct EditEdge *eed);
short sharesFace(struct EditMesh *em, struct EditEdge *e1, struct EditEdge *e2);

/* mesh_data.c */

void ED_mesh_geometry_add(struct Mesh *mesh, struct ReportList *reports, int verts, int edges, int faces);
void ED_mesh_transform(struct Mesh *me, float *mat);
void ED_mesh_calc_normals(struct Mesh *me);
void ED_mesh_material_add(struct Mesh *me, struct Material *ma);
void ED_mesh_update(struct Mesh *mesh, struct bContext *C, int calc_edges);

int ED_mesh_uv_texture_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me);
int ED_mesh_uv_texture_remove(struct bContext *C, struct Object *ob, struct Mesh *me);
int ED_mesh_color_add(struct bContext *C, struct Scene *scene, struct Object *ob, struct Mesh *me);
int ED_mesh_color_remove(struct bContext *C, struct Object *ob, struct Mesh *me);

//---ibli

/* Internal for editmesh_xxxx.c functions */
struct bContext;
struct wmOperatorType;
struct wmOperator;


#define UVCOPY(t, s) memcpy(t, s, 2 * sizeof(float));

/* ******************** editface.c */

int edgetag_context_check(struct Scene *scene,struct EditEdge *eed);
void edgetag_context_set(struct Scene *scene, struct EditEdge *eed, int val);
int edgetag_shortest_path(struct Scene *scene,struct EditMesh *em,struct EditEdge *source,struct EditEdge *target);

/* ******************* editmesh.c */

extern void free_editvert(struct  EditMesh *em, struct EditVert *eve);
extern void free_editedge(struct EditMesh *em, struct EditEdge *eed);
extern void free_editface(struct EditMesh *em, struct EditFace *efa);
void free_editMesh(struct EditMesh *em);

extern void free_vertlist(struct EditMesh *em, struct ListBase *edve);
extern void free_edgelist(struct EditMesh *em,struct  ListBase *lb);
extern void free_facelist(struct EditMesh *em,struct  ListBase *lb);

extern void remedge(struct EditMesh *em,struct EditEdge *eed);

extern struct EditVert *addvertlist(struct EditMesh *em, float *vec, struct EditVert *example);
extern struct EditEdge *addedgelist(struct EditMesh *em, struct EditVert *v1, struct EditVert *v2, struct EditEdge *example);
extern struct EditFace *addfacelist(struct EditMesh *em, struct EditVert *v1, struct EditVert *v2, struct EditVert *v3, struct EditVert *v4, struct EditFace *example, struct EditFace *exampleEdges);
extern struct EditEdge *findedgelist(struct EditMesh *em, struct EditVert *v1, struct EditVert *v2);

void em_setup_viewcontext(struct bContext *C, struct ViewContext *vc);

/* ******************* editmesh_lib.c */
void EM_stats_update(struct EditMesh *em);

extern void EM_fgon_flags(struct EditMesh *em);
extern void EM_hide_reset(struct EditMesh *em);

extern int faceselectedOR(struct EditFace *efa, int flag);
extern int faceselectedAND(struct EditFace *efa, int flag);

void EM_remove_selection(struct EditMesh *em, void *data, int type);
void EM_clear_flag_all(struct EditMesh *em, int flag);
void EM_set_flag_all(struct EditMesh *em, int flag);
void EM_set_flag_all_selectmode(struct EditMesh *em, int flag);

void EM_data_interp_from_verts(struct EditMesh *em,struct  EditVert *v1, struct EditVert *v2, struct EditVert *eve, float fac);
void EM_data_interp_from_faces(struct EditMesh *em,struct  EditFace *efa1,struct  EditFace *efa2,struct  EditFace *efan, int i1, int i2, int i3, int i4);

int EM_nvertices_selected(struct EditMesh *em);
int EM_nedges_selected(struct EditMesh *em);
int EM_nfaces_selected(struct EditMesh *em);

float EM_face_perimeter(struct EditFace *efa);

void EM_store_selection(struct EditMesh *em, void *data, int type);

extern struct EditFace *exist_face(struct EditMesh *em, struct  EditVert *v1,struct  EditVert *v2,struct  EditVert *v3, struct EditVert *v4);
extern void flipface(struct EditMesh *em, struct  EditFace *efa); // flips for normal direction
extern int compareface(struct EditFace *vl1,struct EditFace *vl2);

/* flag for selection bits, *nor will be filled with normal for extrusion constraint */
/* return value defines if such normal was set */
extern short extrudeflag_face_indiv(struct EditMesh *em, short flag, float *nor);
extern short extrudeflag_verts_indiv(struct EditMesh *em, short flag, float *nor);
extern short extrudeflag_edges_indiv(struct EditMesh *em, short flag, float *nor);
extern short extrudeflag_vert(struct Object *obedit,struct  EditMesh *em, short flag, float *nor, int all);
extern short extrudeflag(struct Object *obedit,struct  EditMesh *em, short flag, float *nor, int all);

extern void adduplicateflag(struct EditMesh *em, int flag);
extern void delfaceflag(struct EditMesh *em, int flag);

extern void rotateflag(struct EditMesh *em, short flag, float *cent, float rotmat[][3]);
extern void translateflag(struct EditMesh *em, short flag, float *vec);

extern int convex(float *v1, float *v2, float *v3, float *v4);

extern struct EditFace *EM_face_from_faces(struct EditMesh *em, struct EditFace *efa1,struct EditFace *efa2, int i1, int i2, int i3, int i4);

extern int EM_view3d_poll(struct bContext *C);

/* ******************* editmesh_loop.c */

#define LOOP_SELECT	1
#define LOOP_CUT	2


extern struct EditEdge *findnearestedge(struct ViewContext *vc, int *dist);
extern void EM_automerge(struct Scene *scene,struct Object *obedit, int update);
void editmesh_select_by_material(struct EditMesh *em, int index);
void EM_recalc_normal_direction(struct EditMesh *em, int inside, int select);	/* makes faces righthand turning */
void EM_select_more(struct EditMesh *em);
void selectconnected_mesh_all(struct EditMesh *em);
void faceloop_select(struct EditMesh *em, struct EditEdge *startedge, int select);

/**
 * findnearestvert
 * 
 * dist (in/out): minimal distance to the nearest and at the end, actual distance
 * sel: selection bias
 * 		if SELECT, selected vertice are given a 5 pixel bias to make them farter than unselect verts
 * 		if 0, unselected vertice are given the bias
 * strict: if 1, the vertice corresponding to the sel parameter are ignored and not just biased 
 */
extern struct EditVert *findnearestvert(struct ViewContext *vc, int *dist, short sel, short strict);


/* ******************* editmesh_tools.c */

#define SUBDIV_SELECT_ORIG      0
#define SUBDIV_SELECT_INNER     1
#define SUBDIV_SELECT_INNER_SEL 2
#define SUBDIV_SELECT_LOOPCUT 3

/* edge subdivide corner cut types */
#define SUBDIV_CORNER_PATH		0
#define SUBDIV_CORNER_INNERVERT	1
#define SUBDIV_CORNER_FAN		2



void join_triangles(struct EditMesh *em);
int removedoublesflag(struct EditMesh *em, short flag, short automerge, float limit);		/* return amount */
void esubdivideflag(struct Object *obedit,struct  EditMesh *em, int flag, float smooth, float fractal, int beautify, int numcuts, int corner_pattern, int seltype); 

#endif /* ED_MESH_H */

