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
* along with this program; if not, write to the Free Software  Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2006 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Ben Batt <benbatt@gmail.com>
*
* ***** END GPL LICENSE BLOCK *****
*/ 

/* CustomData interface, see also DNA_customdata_types.h. */

#ifndef BKE_CUSTOMDATA_H
#define BKE_CUSTOMDATA_H

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct CustomData;
struct CustomDataLayer;
struct CustomDataMultires;
typedef unsigned int CustomDataMask;

extern const CustomDataMask CD_MASK_BAREMESH;
extern const CustomDataMask CD_MASK_MESH;
extern const CustomDataMask CD_MASK_EDITMESH;
extern const CustomDataMask CD_MASK_DERIVEDMESH;
extern const CustomDataMask CD_MASK_BMESH;
extern const CustomDataMask CD_MASK_FACECORNERS;

/* for ORIGINDEX layer type, indicates no original index for this element */
#define ORIGINDEX_NONE -1

/* initialises a CustomData object with the same layer setup as source and
 * memory space for totelem elements. mask must be an array of length
 * CD_NUMTYPES elements, that indicate if a layer can be copied. */

/* add/copy/merge allocation types */
#define CD_ASSIGN    0  /* use the data pointer */
#define CD_CALLOC    1  /* allocate blank memory */
#define CD_DEFAULT   2  /* allocate and set to default */
#define CD_REFERENCE 3  /* use data pointers, set layer flag NOFREE */
#define CD_DUPLICATE 4  /* do a full copy of all layers, only allowed if source
						   has same number of elements */

/* initialises a CustomData object with the same layer setup as source.
 * mask is a bitfield where (mask & (1 << (layer type))) indicates
 * if a layer should be copied or not. alloctype must be one of the above. */
void CustomData_copy(const struct CustomData *source, struct CustomData *dest,
					 CustomDataMask mask, int alloctype, int totelem);

/* same as the above, except that this will preserve existing layers, and only
 * add the layers that were not there yet */
void CustomData_merge(const struct CustomData *source, struct CustomData *dest,
					  CustomDataMask mask, int alloctype, int totelem);

/* frees data associated with a CustomData object (doesn't free the object
 * itself, though)
 */
void CustomData_free(struct CustomData *data, int totelem);

/* frees all layers with CD_FLAG_TEMPORARY */
void CustomData_free_temporary(struct CustomData *data, int totelem);

/* adds a data layer of the given type to the CustomData object, optionally
 * backed by an external data array. the different allocation types are
 * defined above. returns the data of the layer.
 *
 * in editmode, use EM_add_data_layer instead of this function
 */
void *CustomData_add_layer(struct CustomData *data, int type, int alloctype,
						   void *layer, int totelem);
/*same as above but accepts a name */
void *CustomData_add_layer_named(struct CustomData *data, int type, int alloctype,
						   void *layer, int totelem, const char *name);

/* adds a layer and puts it at the specified offset */
void *CustomData_add_layer_at_offset(struct CustomData *data, int type, int alloctype,
				     void *layerdata, int totelem, int offset);

/* frees the layer at index.
 * returns 1 on succes, 0 if no layer with the given type is found
 *
 * in editmode, use EM_free_data_layer instead of this function
 */
int CustomData_free_layer(struct CustomData *data, int type, int totelem, int index);

/* frees the active layer of the give type.
 * returns 1 on succes, 0 if no layer with the given type is found
 *
 * in editmode, use EM_free_data_layer instead of this function
 */
int CustomData_free_layer_active(struct CustomData *data, int type, int totelem);

/* same as above, but free all layers with type */
void CustomData_free_layers(struct CustomData *data, int type, int totelem);

/* returns 1 if a layer with the specified type exists */
int CustomData_has_layer(const struct CustomData *data, int type);

/* returns the number of layers with this type */
int CustomData_number_of_layers(const struct CustomData *data, int type);

/* duplicate data of a layer with flag NOFREE, and remove that flag.
 * returns the layer data */
void *CustomData_duplicate_referenced_layer(struct CustomData *data, int type);
void *CustomData_duplicate_referenced_layer_named(struct CustomData *data,
												  int type, char *name);

/* set the CD_FLAG_NOCOPY flag in custom data layers where the mask is
 * zero for the layer type, so only layer types specified by the mask
 * will be copied
 */
void CustomData_set_only_copy(const struct CustomData *data,
							  CustomDataMask mask);

/* copies data from one CustomData object to another
 * objects need not be compatible, each source layer is copied to the
 * first dest layer of correct type (if there is none, the layer is skipped)
 * return 1 on success, 0 on failure
 */
void CustomData_copy_data(const struct CustomData *source,
						  struct CustomData *dest, int source_index,
						  int dest_index, int count);
void CustomData_em_copy_data(const struct CustomData *source,
							struct CustomData *dest, void *src_block,
							void **dest_block);
void CustomData_bmesh_copy_data(const struct CustomData *source, 
							struct CustomData *dest,void *src_block, 
							void **dest_block);

/* frees data in a CustomData object
 * return 1 on success, 0 on failure
 */
void CustomData_free_elem(struct CustomData *data, int index, int count);

/* interpolates data from one CustomData object to another
 * objects need not be compatible, each source layer is interpolated to the
 * first dest layer of correct type (if there is none, the layer is skipped)
 * if weights == NULL or sub_weights == NULL, they default to all 1's
 *
 * src_indices gives the source elements to interpolate from
 * weights gives the weight for each source element
 * sub_weights is an array of matrices of weights for sub-elements (matrices
 *     should be source->subElems * source->subElems in size)
 * count gives the number of source elements to interpolate from
 * dest_index gives the dest element to write the interpolated value to
 *
 * returns 1 on success, 0 on failure
 */
void CustomData_interp(const struct CustomData *source, struct CustomData *dest,
					   int *src_indices, float *weights, float *sub_weights,
					   int count, int dest_index);
void CustomData_em_interp(struct CustomData *data,  void **src_blocks,
						  float *weights, float *sub_weights, int count,
						  void *dest_block);
void CustomData_bmesh_interp(struct CustomData *data, void **src_blocks, 
							 float *weights, float *sub_weights, int count, 
							 void *dest_block);


/* swaps the data in the element corners, to new corners with indices as
   specified in corner_indices. for edges this is an array of length 2, for
   faces an array of length 4 */
void CustomData_swap(struct CustomData *data, int index, const int *corner_indices);

/* gets a pointer to the data element at index from the first layer of type
 * returns NULL if there is no layer of type
 */
void *CustomData_get(const struct CustomData *data, int index, int type);
void *CustomData_em_get(const struct CustomData *data, void *block, int type);
void *CustomData_em_get_n(const struct CustomData *data, void *block, int type, int n);
void *CustomData_bmesh_get(const struct CustomData *data, void *block, int type);
void *CustomData_bmesh_get_n(const struct CustomData *data, void *block, int type, int n);

/************************** Get layer data ***********************/
/* return the data from the first layer matching the parameters  */
/* these all return NULL if no such layer is found               */

/* returns the data from the active layer matching type */
void *CustomData_get_layer(const struct CustomData *data, int type);

/* returns the data from the nth layer matching type */
void *CustomData_get_layer_n(const struct CustomData *data, int type, int n);

/* returns the data from the first layer matching type and name */
void *CustomData_get_layer_named(const struct CustomData *data, int type,
								 char *name);

/********************************* Get layer index *********************************/
/* return the index within data.layers of the first layer matching the parameters  */
/* these all return -1 if no such layer is found                                   */

/* returns the index of the first layer matching type */
int CustomData_get_layer_index(const struct CustomData *data, int type);

/* returns the index of the first layer matching type and name */
int CustomData_get_named_layer_index(const struct CustomData *data, int type, char *name);

/* returns the index of the active layer */
int CustomData_get_active_layer_index(const struct CustomData *data, int type);

/* returns the index of the render layer */
int CustomData_get_render_layer_index(const struct CustomData *data, int type);

/* returns the index of the clone layer */
int CustomData_get_clone_layer_index(const struct CustomData *data, int type);

/* returns the index of the stencil/mask layer */
int CustomData_get_stencil_layer_index(const struct CustomData *data, int type);

/************************************ Get layer offset ***********************************/
/* return the offset of the layer from the first layer of the specified type             */
/* to find the index of that layer, add it to the index of the first layer of that type  */
/* these all return -1 if no layer of the specified type is found                        */
int CustomData_get_active_layer(const struct CustomData *data, int type);
int CustomData_get_render_layer(const struct CustomData *data, int type);
int CustomData_get_clone_layer(const struct CustomData *data, int type);
int CustomData_get_stencil_layer(const struct CustomData *data, int type);

/* copies the data from source to the data element at index in the first
 * layer of type
 * no effect if there is no layer of type
 */
void CustomData_set(const struct CustomData *data, int index, int type,
					void *source);
void CustomData_em_set(struct CustomData *data, void *block, int type,
					   void *source);
void CustomData_em_set_n(struct CustomData *data, void *block, int type, int n,
						 void *source);

void CustomData_bmesh_set(const struct CustomData *data, void *block, int type, 
						  void *source);

void CustomData_bmesh_set_n(struct CustomData *data, void *block, int type, int n, 
							void *source);

/* set the pointer of to the first layer of type. the old data is not freed.
 * returns the value of ptr if the layer is found, NULL otherwise
 */
void *CustomData_set_layer(const struct CustomData *data, int type, void *ptr);
void *CustomData_set_layer_n(const struct CustomData *data, int type, int n, void *ptr);

/* sets the nth layer of type as active */
void CustomData_set_layer_active(struct CustomData *data, int type, int n);
void CustomData_set_layer_render(struct CustomData *data, int type, int n);
void CustomData_set_layer_clone(struct CustomData *data, int type, int n);
void CustomData_set_layer_stencil(struct CustomData *data, int type, int n);

/* same as above but works with an index from CustomData_get_layer_index */
void CustomData_set_layer_active_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_render_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_clone_index(struct CustomData *data, int type, int n);
void CustomData_set_layer_stencil_index(struct CustomData *data, int type, int n);

/* adds flag to the layer flags */
void CustomData_set_layer_flag(struct CustomData *data, int type, int flag);
void CustomData_set_layer_offset_flag(struct CustomData *data, int type, int offset, int flag);

/* alloc/free a block of custom data attached to one element in editmode */
void CustomData_em_set_default(struct CustomData *data, void **block);
void CustomData_em_free_block(struct CustomData *data, void **block);

void CustomData_bmesh_set_default(struct CustomData *data, void **block);
void CustomData_bmesh_free_block(struct CustomData *data, void **block);

/* copy custom data to/from layers as in mesh/derivedmesh, to editmesh
   blocks of data. the CustomData's must not be compatible  */
void CustomData_to_em_block(const struct CustomData *source,
							struct CustomData *dest, int index, void **block);
void CustomData_from_em_block(const struct CustomData *source,
							  struct CustomData *dest, void *block, int index);
void CustomData_to_bmesh_block(const struct CustomData *source, 
							struct CustomData *dest, int src_index, void **dest_block);
void CustomData_from_bmesh_block(const struct CustomData *source, 
							struct CustomData *dest, void *src_block, int dest_index);


/* query info over types */
void CustomData_file_write_info(int type, char **structname, int *structnum);
int CustomData_sizeof(int type);

/* get the name of a layer type */
const char *CustomData_layertype_name(int type);

/* make sure the name of layer at index is unique */
void CustomData_set_layer_unique_name(struct CustomData *data, int index);

/* for file reading compatibility, returns false if the layer was freed,
   only after this test passes, layer->data should be assigned */
int CustomData_verify_versions(struct CustomData *data, int index);

/*BMesh specific customdata stuff*/
void CustomData_to_bmeshpoly(struct CustomData *fdata, struct CustomData *pdata, struct CustomData *ldata);
void CustomData_from_bmeshpoly(struct CustomData *fdata, struct CustomData *pdata, struct CustomData *ldata, int total);
void CustomData_bmesh_init_pool(struct CustomData *data, int allocsize);

/* Subsurf grids */

/* return the number of layers of type that have multires data */
int CustomData_get_multires_count(struct CustomData *cd, int type);

/* allocates a list of names of layers that have multires data */
void *CustomData_get_multires_names(struct CustomData *cd, int type);

/* number of floats used per-element for the multires of a customdata type */
int CustomData_multires_type_totfloat(int type);

/* returns the multires data for a layer matching name and type,
   or NULL if no such layer found */
float *CustomData_multires_get_data(struct CustomDataMultires *cdm, int type,
				    char *name);

/* if layer matching type and name exists, free and replace its griddata
   otherwise create the layer and set its griddata */
void CustomData_multires_sync_layer(struct CustomDataMultires *cdm, int type,
				    char *name);

/* insert a multires layer of the specified type and assign griddata */
void CustomData_multires_add_layer_data(struct CustomDataMultires *cdm, int type,
					char *name, float *griddata);

/* insert a multires layer of the specified type int each
   cdm in the array of length count */
void CustomData_multires_add_layers(struct CustomDataMultires *cdm, int count,
				    int type, char *name);

/* remove the multires layer with matching source name from the cdm
   array of length count, returns 1 if succesful, 0 otherwise */
int CustomData_multires_remove_layers(struct CustomDataMultires *cdm, int count,
				      int type, char *name);

/* rename a layer matching type and old_name */
void CustomData_multires_rename(struct CustomDataMultires *cdm, int type,
				char *old_name, char *name);

/* External file storage */

void CustomData_external_add(struct CustomData *data,
	struct ID *id, int type, int totelem, const char *filename);
void CustomData_external_remove(struct CustomData *data,
	struct ID *id, int type, int totelem);
int CustomData_external_test(struct CustomData *data, int type);

void CustomData_external_write(struct CustomData *data,
	struct ID *id, CustomDataMask mask, int totelem, int free);
void CustomData_external_read(struct CustomData *data,
	struct ID *id, CustomDataMask mask, int totelem);
void CustomData_external_reload(struct CustomData *data,
	struct ID *id, CustomDataMask mask, int totelem);

#ifdef __cplusplus
}
#endif

#endif
