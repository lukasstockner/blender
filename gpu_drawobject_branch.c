#if 0 /* currently unused */
#  define USE_GPU_POINT_LINK
#endif

typedef struct GPUVertPointLink {
#ifdef USE_GPU_POINT_LINK
	struct GPUVertPointLink *next;
#endif
	/* -1 means uninitialized */
	int point_index;
} GPUVertPointLink;


/* add a new point to the list of points related to a particular
 * vertex */
#ifdef USE_GPU_POINT_LINK

static void gpu_drawobject_add_vert_point(GPUDrawObject *gdo, int vert_index, int point_index)
{
	GPUVertPointLink *lnk;

	lnk = &gdo->vert_points[vert_index];

	/* if first link is in use, add a new link at the end */
	if (lnk->point_index != -1) {
		/* get last link */
		for (; lnk->next; lnk = lnk->next) ;

		/* add a new link from the pool */
		lnk = lnk->next = &gdo->vert_points_mem[gdo->vert_points_usage];
		gdo->vert_points_usage++;
	}

	lnk->point_index = point_index;
}

#else

static void gpu_drawobject_add_vert_point(GPUDrawObject *gdo, int vert_index, int point_index)
{
	GPUVertPointLink *lnk;
	lnk = &gdo->vert_points[vert_index];
	if (lnk->point_index == -1) {
		lnk->point_index = point_index;
	}
}

#endif  /* USE_GPU_POINT_LINK */

/* update the vert_points and triangle_to_mface fields with a new
 * triangle */
static void gpu_drawobject_add_triangle(GPUDrawObject *gdo,
                                        int base_point_index,
                                        int face_index,
                                        int v1, int v2, int v3)
{
	int i, v[3] = {v1, v2, v3};
	for (i = 0; i < 3; i++)
		gpu_drawobject_add_vert_point(gdo, v[i], base_point_index + i);
	gdo->triangle_to_mface[base_point_index / 3] = face_index;
}

/* for each vertex, build a list of points related to it; these lists
 * are stored in an array sized to the number of vertices */
static void gpu_drawobject_init_vert_points(GPUDrawObject *gdo, MFace *f, int totface, int totmat)
{
	GPUBufferMaterial *mat;
	int i, *mat_orig_to_new;

	mat_orig_to_new = MEM_callocN(sizeof(*mat_orig_to_new) * totmat,
	                              "GPUDrawObject.mat_orig_to_new");
	/* allocate the array and space for links */
	gdo->vert_points = MEM_mallocN(sizeof(GPUVertPointLink) * gdo->totvert,
	                               "GPUDrawObject.vert_points");
#ifdef USE_GPU_POINT_LINK
	gdo->vert_points_mem = MEM_callocN(sizeof(GPUVertPointLink) * gdo->tot_triangle_point,
	                                   "GPUDrawObject.vert_points_mem");
	gdo->vert_points_usage = 0;
#endif

	/* build a map from the original material indices to the new
	 * GPUBufferMaterial indices */
	for (i = 0; i < gdo->totmaterial; i++)
		mat_orig_to_new[gdo->materials[i].mat_nr] = i;

	/* -1 indicates the link is not yet used */
	for (i = 0; i < gdo->totvert; i++) {
#ifdef USE_GPU_POINT_LINK
		gdo->vert_points[i].link = NULL;
#endif
		gdo->vert_points[i].point_index = -1;
	}

	for (i = 0; i < totface; i++, f++) {
		mat = &gdo->materials[mat_orig_to_new[f->mat_nr]];

		/* add triangle */
		gpu_drawobject_add_triangle(gdo, mat->start + mat->totpoint,
		                            i, f->v1, f->v2, f->v3);
		mat->totpoint += 3;

		/* add second triangle for quads */
		if (f->v4) {
			gpu_drawobject_add_triangle(gdo, mat->start + mat->totpoint,
			                            i, f->v3, f->v4, f->v1);
			mat->totpoint += 3;
		}
	}

	/* map any unused vertices to loose points */
	for (i = 0; i < gdo->totvert; i++) {
		if (gdo->vert_points[i].point_index == -1) {
			gdo->vert_points[i].point_index = gdo->tot_triangle_point + gdo->tot_loose_point;
			gdo->tot_loose_point++;
		}
	}

	MEM_freeN(mat_orig_to_new);
}

/* see GPUDrawObject's structure definition for a description of the
 * data being initialized here */
GPUDrawObject *GPU_drawobject_new(DerivedMesh *dm)
{
	GPUDrawObject *gdo;
	MFace *mface;
	int totmat = dm->totmat;
	int *points_per_mat;
	int i, curmat, curpoint, totface;

	/* object contains at least one material (default included) so zero means uninitialized dm */
	BLI_assert(totmat != 0);

	mface = dm->getTessFaceArray(dm);
	totface = dm->getNumTessFaces(dm);

	/* get the number of points used by each material, treating
	 * each quad as two triangles */
	points_per_mat = MEM_callocN(sizeof(*points_per_mat) * totmat, "GPU_drawobject_new.mat_orig_to_new");
	for (i = 0; i < totface; i++)
		points_per_mat[mface[i].mat_nr] += mface[i].v4 ? 6 : 3;

	/* create the GPUDrawObject */
	gdo = MEM_callocN(sizeof(GPUDrawObject), "GPUDrawObject");
	gdo->totvert = dm->getNumVerts(dm);
	gdo->totedge = dm->getNumEdges(dm);

	/* count the number of materials used by this DerivedMesh */
	for (i = 0; i < totmat; i++) {
		if (points_per_mat[i] > 0)
			gdo->totmaterial++;
	}

	/* allocate an array of materials used by this DerivedMesh */
	gdo->materials = MEM_mallocN(sizeof(GPUBufferMaterial) * gdo->totmaterial,
	                             "GPUDrawObject.materials");

	/* initialize the materials array */
	for (i = 0, curmat = 0, curpoint = 0; i < totmat; i++) {
		if (points_per_mat[i] > 0) {
			gdo->materials[curmat].start = curpoint;
			gdo->materials[curmat].totpoint = 0;
			gdo->materials[curmat].mat_nr = i;

			curpoint += points_per_mat[i];
			curmat++;
		}
	}

	/* store total number of points used for triangles */
	gdo->tot_triangle_point = curpoint;

	gdo->triangle_to_mface = MEM_mallocN(sizeof(int) * (gdo->tot_triangle_point / 3),
	                                     "GPUDrawObject.triangle_to_mface");

	gpu_drawobject_init_vert_points(gdo, mface, totface, totmat);
	MEM_freeN(points_per_mat);

	return gdo;
}

// ---------------------

static void copy_mcol_uc3(unsigned char *v, unsigned char *col)
{
	v[0] = col[3];
	v[1] = col[2];
	v[2] = col[1];
}

/* treat varray_ as an array of MCol, four MCol's per face */
static void GPU_buffer_copy_mcol(DerivedMesh *dm, float *varray_, int *index, int *mat_orig_to_new, void *user)
{
	int i, totface;
	unsigned char *varray = (unsigned char *)varray_;
	unsigned char *mcol = (unsigned char *)user;
	MFace *f = dm->getTessFaceArray(dm);

	totface = dm->getNumTessFaces(dm);
	for (i = 0; i < totface; i++, f++) {
		int start = index[mat_orig_to_new[f->mat_nr]];

		/* v1 v2 v3 */
		copy_mcol_uc3(&varray[start], &mcol[i * 16]);
		copy_mcol_uc3(&varray[start + 3], &mcol[i * 16 + 4]);
		copy_mcol_uc3(&varray[start + 6], &mcol[i * 16 + 8]);
		index[mat_orig_to_new[f->mat_nr]] += 9;

		if (f->v4) {
			/* v3 v4 v1 */
			copy_mcol_uc3(&varray[start + 9], &mcol[i * 16 + 8]);
			copy_mcol_uc3(&varray[start + 12], &mcol[i * 16 + 12]);
			copy_mcol_uc3(&varray[start + 15], &mcol[i * 16]);
			index[mat_orig_to_new[f->mat_nr]] += 9;
		}
	}
}

static void GPU_buffer_copy_edge(DerivedMesh *dm, float *varray_, int *UNUSED(index), int *UNUSED(mat_orig_to_new), void *UNUSED(user))
{
	MEdge *medge;
	unsigned int *varray = (unsigned int *)varray_;
	int i, totedge;

	medge = dm->getEdgeArray(dm);
	totedge = dm->getNumEdges(dm);

	for (i = 0; i < totedge; i++, medge++) {
		varray[i * 2] = dm->drawObject->vert_points[medge->v1].point_index;
		varray[i * 2 + 1] = dm->drawObject->vert_points[medge->v2].point_index;
	}
}

static void GPU_buffer_copy_uvedge(DerivedMesh *dm, float *varray, int *UNUSED(index), int *UNUSED(mat_orig_to_new), void *UNUSED(user))
{
	MTFace *tf = DM_get_tessface_data_layer(dm, CD_MTFACE);
	int i, j = 0;

	if (!tf)
		return;

	for (i = 0; i < dm->numTessFaceData; i++, tf++) {
		MFace mf;
		dm->getTessFace(dm, i, &mf);

		copy_v2_v2(&varray[j], tf->uv[0]);
		copy_v2_v2(&varray[j + 2], tf->uv[1]);

		copy_v2_v2(&varray[j + 4], tf->uv[1]);
		copy_v2_v2(&varray[j + 6], tf->uv[2]);

		if (!mf.v4) {
			copy_v2_v2(&varray[j + 8], tf->uv[2]);
			copy_v2_v2(&varray[j + 10], tf->uv[0]);
			j += 12;
		}
		else {
			copy_v2_v2(&varray[j + 8], tf->uv[2]);
			copy_v2_v2(&varray[j + 10], tf->uv[3]);

			copy_v2_v2(&varray[j + 12], tf->uv[3]);
			copy_v2_v2(&varray[j + 14], tf->uv[0]);
			j += 16;
		}
	}
}

typedef enum {
	GPU_BUFFER_VERTEX = 0,
	GPU_BUFFER_NORMAL,
	GPU_BUFFER_COLOR,
	GPU_BUFFER_UV,
	GPU_BUFFER_UV_TEXPAINT,
	GPU_BUFFER_EDGE,
	GPU_BUFFER_UVEDGE,
} GPUBufferType;

typedef struct {
	GPUBufferCopyFunc copy;
	GLenum gl_buffer_type;
	int vector_size;
} GPUBufferTypeSettings;

const GPUBufferTypeSettings gpu_buffer_type_settings[] = {
	{GPU_buffer_copy_vertex, GL_ARRAY_BUFFER_ARB, 3},
	{GPU_buffer_copy_normal, GL_ARRAY_BUFFER_ARB, 3},
	{GPU_buffer_copy_mcol, GL_ARRAY_BUFFER_ARB, 3},
	{GPU_buffer_copy_uv, GL_ARRAY_BUFFER_ARB, 2},
	{GPU_buffer_copy_uv_texpaint, GL_ARRAY_BUFFER_ARB, 4},
	{GPU_buffer_copy_edge, GL_ELEMENT_ARRAY_BUFFER_ARB, 2},
	{GPU_buffer_copy_uvedge, GL_ELEMENT_ARRAY_BUFFER_ARB, 4}
};

