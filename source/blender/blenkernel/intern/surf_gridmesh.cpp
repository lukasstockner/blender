//
//  GridMesh.cpp
//  PolyTest
//
//  Created by Jonathan deWerd on 6/20/14.
//  Copyright (c) 2014 a.b.c. All rights reserved.
//

#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <algorithm>
#include <limits>
#include <stdio.h>
#include "surf_gridmesh.h"

//#define NURBS_TESS_DEBUG
#if defined(NURBS_TESS_DEBUG)
#define NURBS_TESS_PRINTF(...) printf(__VA_ARGS__)
#else
#define NURBS_TESS_PRINTF(...)
#endif

float GridMesh::tolerance = 1e-5;

/* The public GridMeshIterator::next() function only loops over valid polygons.
 * this function loops over invalid polygons too and relies on the caller to discard
 * them.
 * returns: is the polygon gmi->cell at the end of the call valid?
 */
static bool gmi_next(GridMeshIterator *gmi) {
	GridMesh *gm = gmi->gm;
	std::vector<GridMeshVert> &v = gm->v;
	if (gmi->cell==0) return bool(v[gmi->poly].next);
	GridMeshVert *vert = &v[gmi->poly];
	// Outer loop: move from cell 1 to cell last_cell (inclusive)
	// Inner loop: keep moving to next_poly
	if (vert->next_poly) {
		gmi->poly = vert->next_poly;
		return bool(v[gmi->poly].next);
	} /* have: vert->next_poly == 0 */
	gmi->cell = gmi->poly = gmi->cell+4;
	if (gmi->cell > gmi->last_cell) {
		gmi->cell = gmi->poly = 0;
		return false;
	}
	return bool(v[gmi->poly].next);
}

GridMeshIterator::GridMeshIterator(GridMesh *gm) {
	this->gm = gm;
	this->cell = 1;
	this->poly = 1;
	this->last_cell =gm->poly_for_cell(gm->nx-1, gm->ny-1);
	if (gm->v[this->poly].next) return;
	while (this->poly && !gmi_next(this));
}

void GridMeshIterator::next() {
	while (this->poly && !gmi_next(this));
}

GridMeshIterator GridMesh::begin() {
	return GridMeshIterator(this);
}

// Fast float->int, courtesy of http://stereopsis.com/sree/fpu2006.html
// 5x faster on x86. It's not in the hot loop anymore so it probably
// doesn't really matter. Todo: test and see.
const double _xs_doublemagic             = 6755399441055744.0;               //2^52 * 1.5,  uses limited precisicion to floor
const double _xs_doublemagicdelta        = (1.5e-8);                         //almost .5f = .5f + 1e^(number of exp bit)
const double _xs_doublemagicroundeps     = (.5f-_xs_doublemagicdelta);       //almost .5f = .5f - 1e^(number of exp bit)
inline static int xs_CRoundToInt(double val) {
    val = val + _xs_doublemagic;
    return ((int*)&val)[0]; // 0 for little endian (ok), 1 for big endian (?)
	//    return int32(floor(val+.5)); //Alternative implementation if the trick is buggy
}
inline static int xs_FloorToInt(double val) {
    return xs_CRoundToInt(val-_xs_doublemagicroundeps);
	//return floor(val); //Alternative implementation if the trick is buggy
}

static void print_kc(known_corner_t kc) {
	if (kc&KNOWN_CORNER_LL)
		NURBS_TESS_PRINTF("LL%c",(kc&KNOWN_CORNER_LL_EXTERIOR)?'e':'i');
	if (kc&KNOWN_CORNER_UL)
		NURBS_TESS_PRINTF("UL%c",(kc&KNOWN_CORNER_UL_EXTERIOR)?'e':'i');
	if (kc&KNOWN_CORNER_LR)
		NURBS_TESS_PRINTF("LR%c",(kc&KNOWN_CORNER_LR_EXTERIOR)?'e':'i');
	if (kc&KNOWN_CORNER_UR)
		NURBS_TESS_PRINTF("UR%c",(kc&KNOWN_CORNER_UR_EXTERIOR)?'e':'i');
}

GridMeshVert::GridMeshVert() :	next(0), prev(0),
								next_poly(0), neighbor(0), first(0),
								is_intersection(false), is_interior(true), is_entry(false),
								is_used(false), corner(0), tmp(0), is_pristine(0),
								owns_coords(0), coord_idx(0)
{}

GridMesh::GridMesh() {
	coords = NULL;
	coords_len = coords_reserved_len = 0;
	mallocN = NULL;
	reallocN = NULL;
	recorded_AND = NULL;
	recorded_SUB = NULL;
}

GridMesh::~GridMesh() {
	if (coords) free(coords);
}

void GridMesh::coords_reserve(int new_reserved_len) {
	if (coords_reserved_len>=new_reserved_len) return;
	new_reserved_len *= 2;
	if (!coords) {
		if (mallocN)
			coords = (GridMeshCoord*)mallocN(sizeof(*coords)*new_reserved_len,"NURBS_gridmesh");
		else
			coords = (GridMeshCoord*)malloc(sizeof(*coords)*new_reserved_len);
	} else if (coords_reserved_len<new_reserved_len){
		if (reallocN)
			coords = (GridMeshCoord*)reallocN(coords, sizeof(*coords)*new_reserved_len, "NURBS_gridmesh");
		else
			coords = (GridMeshCoord*)realloc(coords, sizeof(*coords)*new_reserved_len);
	}
	coords_reserved_len = new_reserved_len;
}

void GridMesh::coords_import(GridMeshCoord *c, int len) {
	if (coords) printf("WARNING: coords should be imported *before* init\n");
	coords = c;
	coords_len = len;
}

GridMeshCoord *GridMesh::coords_export(int *len) {
	GridMeshCoord *ret = coords;
	if (len) *len = coords_len;
	coords = NULL;
	return ret;
}

void GridMesh::set_ll_ur(double lowerleft_x, double lowerleft_y,
						 double upperright_x, double upperright_y) {
	llx = lowerleft_x; lly = lowerleft_y;
	urx = upperright_x; ury = upperright_y;
}

void GridMesh::init_grid(int num_x_cells, int num_y_cells) {
	nx = num_x_cells; ny = num_y_cells;
	double Dx = urx-llx;
	double Dy = ury-lly;
	dx = Dx/nx;
	dy = Dy/ny;
	inv_dx = 1.0/dx;
	inv_dy = 1.0/dy;

	int num_coords = (nx+1)*(ny+1)*2+1;
	coords_reserve(num_coords);
	for (int j=0; j<=ny; j++) {
		for (int i=0; i<=nx; i++) {
			GridMeshCoord& c = coords[gridpt_for_cell(i,j)];
			c.x = llx + i*dx;
			c.y = lly + j*dy;
			//c.z = 0;
		}
	}
	coords_len = (1+nx)*(1+ny)+1;
	v.resize(nx*ny*4*2);
	ie_grid.assign(nx*ny+1,true);
	ie_isect_right.assign(nx*ny+1,false);
	ie_isect_up.assign(nx*ny+1,false);
	vert_set_coord(0, -1234, -1234, -1234);
	for (int j=0; j<ny; j++) {
		for (int i=0; i<nx; i++) {
			int iv1=poly_for_cell(i, j), iv1c=gridpt_for_cell(i, j);
			int iv2=iv1+1, iv2c=iv1c+1;
			int iv3=iv1+2, iv3c=iv1c+nx+2;
			int iv4=iv1+3, iv4c=iv1c+nx+1;
			GridMeshVert *v1 = &v[iv1];
			GridMeshVert *v2 = v1+1;
			GridMeshVert *v3 = v1+2;
			GridMeshVert *v4 = v1+3;
			v1->coord_idx = iv1c;
			v2->coord_idx = iv2c;
			v3->coord_idx = iv3c;
			v4->coord_idx = iv4c;
			v1->next = iv2; v2->prev = iv1; v1->first = iv1; v1->corner = 1;
			v2->next = iv3; v3->prev = iv2; v2->first = iv1; v2->corner = 3;
			v3->next = iv4; v4->prev = iv3; v3->first = iv1; v3->corner = 4;
			v4->next = iv1; v1->prev = iv4; v4->first = iv1; v4->corner = 2;
			v1->is_pristine = 1;
		}
	}
}

void GridMesh::ie_print_grid(int num) {
	std::vector<bool> *grid = NULL;
	if (num==0) {
		puts("ie_grid:");
		grid = &ie_grid;
	} else if (num==1) {
		puts("ie_isect_right:");
		grid = &ie_isect_right;
	} else {
		puts("ie_isect_up:");
		grid = &ie_isect_up;
	}
	for (int y=ny; y>=0; y--) {
		for (int x=0; x<=nx; x++) {
			bool val = grid->operator[](gridpt_for_cell(x, y));
			printf((val)?"1":"0");
		}
		puts("");
	}
}

int GridMesh::vert_new() {
	v.push_back(GridMeshVert());
	return int(v.size()-1);
}

int GridMesh::vert_new(int prev, int next) {
	int ret = vert_new();
	if (prev) {
		v[ret].first = v[prev].first;
		v[ret].prev = prev;
		v[prev].next = ret;
	}
	if (next) {
		v[ret].first = v[next].first;
		v[ret].next = next;
		v[next].prev = ret;
	}
	return ret;
}

int GridMesh::vert_dup(int vert) {
	int ret = vert_new();
	new (&v[ret]) GridMeshVert(v[vert]);
	return ret;
}

void GridMesh::vert_set_coord(int vert, double x, double y, double z) {
	if (v[vert].owns_coords) {
		GridMeshCoord& xyz = coords[v[vert].coord_idx];
		xyz.x=x; xyz.y=y; // xyz.z=z;
		return;
	}
	int idx = coords_len;
	coords_reserve(++coords_len);
	GridMeshCoord *xyz = &coords[idx];
	xyz->x=x; xyz->y=y;// xyz->z=z;
	v[vert].coord_idx = idx;
	v[vert].owns_coords = 1;
}

void GridMesh::vert_get_coord(int vert, double* xyz) {
	GridMeshCoord *gmc = &coords[v[vert].coord_idx];
	xyz[0] = gmc->x;
	xyz[1] = gmc->y;
	//xyz[2] = gmc->z;
}


int GridMesh::poly_new(const float* packed_coords, int len) {
	size_t num_verts = len/2;
	if (!num_verts) return 0;
	int last=0, first=0;
	for (int i=0; i<num_verts; i++) {
		int vert = vert_new(last,0);
		if (!first) first=vert;
		v[vert].first = first;
		vert_set_coord(vert, packed_coords[2*i+0], packed_coords[2*i+1], 0);
		last = vert;
	}
	v[first].prev = last;
	v[last].next = first;
	return first;
}

int GridMesh::poly_first_vert(int vert) {
	int v2 = vert;
	while (v[v2].prev) {
		if (v[v2].first==v2) return v2;
		v2 = v[v2].prev;
	}
	return v2;
}

int GridMesh::poly_last_vert(int vert) {
	int v2 = vert;
	while (v[v2].next && v[v2].next!=v[v2].first) {v2 = v[v2].next;}
	return v2;
}

int GridMesh::poly_next(int anyvert) {
	return v[poly_first_vert(anyvert)].next_poly;
}

int GridMesh::poly_last(int poly) {
	while (v[poly].next_poly) poly = v[poly].next_poly;
	return poly;
}


bool GridMesh::poly_is_cyclic(int poly) {
	if (!v[poly].next) return false;
	return bool(v[poly_first_vert(poly)].prev);
}

void GridMesh::poly_set_cyclic(int poly, bool cyc) {
	if (cyc==poly_is_cyclic(poly)) return;
	int first = poly_first_vert(poly);
	int last = poly_last_vert(poly);
	if (cyc) {
		v[first].prev = last;
		v[last].next = first;
	} else {
		v[first].prev = 0;
		v[last].next = 0;
	}
}

int GridMesh::poly_for_cell(float fx, float fy) {
	int x = floor((fx-llx)*inv_dx);
	if (x<0||x>=nx) return 0;
	int y = floor((fy-lly)*inv_dy);
	if (y<0||y>=ny) return 0;
	return 1+4*(y*nx+x);
}

int GridMesh::poly_num_edges(int poly) {
	poly = poly_first_vert(poly);
	int ret = 0;
	while (v[poly].next) {
		ret++;
		int next = v[poly].next;
		if (v[next].first==next) break;
		poly = next;
	}
	return ret;
}

int GridMesh::poly_vert_at(int anyvert, float x, float y) {
	bool first_iter = true;
	for(int vert = poly_first_vert(anyvert); vert; vert=v[vert].next) {
		double vc[3];
		if (fabs(x-vc[0])+fabs(y-vc[1])<tolerance) return vert;
		if (first_iter) {
			first_iter = false;
		} else {
			if (v[vert].first==vert) break;
		}
	}
	return 0;
}

int GridMesh::vert_neighbor_on_poly(int vert, int poly) {
	int cur_vert = vert;
	while (cur_vert) {
		if (v[cur_vert].first==poly) return cur_vert;
		cur_vert = v[cur_vert].neighbor;
		if (cur_vert==vert) break;
	}
	return 0;
}

void GridMesh::vert_add_neighbor(int v1, int v2) {
	if (!v[v1].neighbor && !v[v2].neighbor) {
		v[v1].neighbor = v2;
		v[v2].neighbor = v1;
		return;
	}
	if (!v[v1].neighbor && v[v2].neighbor) std::swap(v1,v2);
	// v1 has a neighbor, v2 might have a neighbor
	int v1_last_neighbor = v1;
	while (v1_last_neighbor && v[v1_last_neighbor].neighbor != v1) {
		v1_last_neighbor = v[v1_last_neighbor].neighbor;
	}
	if (v[v1].neighbor && v[v2].neighbor) {
		int v2_last_neighbor = v2;
		while (v2_last_neighbor && v[v2_last_neighbor].neighbor != v2) {
			v2_last_neighbor = v[v2_last_neighbor].neighbor;
		}
		v[v1_last_neighbor].neighbor = v2;
		v[v2_last_neighbor].neighbor = v1;
	} else { // v1 has a neighbor, v2 does not
		v[v1_last_neighbor].neighbor = v2;
		v[v2].neighbor = v1;
	}
}

std::pair<int,int> GridMesh::cell_for_vert(int vert) {
	// vert = 1+4*(y*nx+x)
	int ynx_plus_x = (vert-1)/4;
	int y = ynx_plus_x/nx;
	int x = ynx_plus_x%nx;
	return std::make_pair(x,y);
}

void GridMesh::begin_recording() {
	recorded_AND = new std::vector<int>();
	recorded_SUB = new std::vector<int>();
}

void GridMesh::dump_poly(int poly) {
	printf("{");
	int vert=poly; do {
		int next_v = v[vert].next;
		GridMeshCoord &gmc = coords[v[vert].coord_idx];
		printf((next_v==poly)?"%f,%f}":"%f,%f, ", gmc.x, gmc.y);
		vert = next_v;
	} while (vert!=poly);
}

void GridMesh::dump_recording() {
	puts("#if defined(GRIDMESH_GEOM_TEST_6)");
	if (recorded_AND->size()) {
		printf("std::vector<float> clip_verts = ");
		dump_poly(recorded_AND->at(0));
		printf(";\n");
	} else {
		printf("std::vector<float> clip_verts = {.2,.2,  1.8,.2,  1.8,1.8,  .2,1.8};\n");
	}
	int num_SUB = (int)recorded_SUB->size();
	for (int i=0; i<num_SUB; i++) {
		printf("std::vector<float> subj%i = ",i);
		dump_poly(recorded_SUB->at(i));
		printf(";\n");
	}
	printf("std::vector<std::vector<float>> subj_polys = {");
	for (int i=0; i<num_SUB; i++) {
		printf((i==num_SUB-1)?"subj%i}":"subj%i,",i);
	}
	printf(";\n");
	printf("float gm_llx=%f,gm_lly=%f,gm_urx=%f,gm_ury=%f; // GridMesh params\n",llx,lly,urx,ury);
	printf("int gm_nx=%i, gm_ny=%i;\n",nx,ny);
	puts("std::vector<float> inout_pts = {};");
	puts("bool clip_cyclic = true; // Required for initialization");
	puts("bool subj_cyclic = true;");
	puts("#endif");
	delete recorded_AND;
	delete recorded_SUB;
}

void GridMesh::poly_grid_BB(int poly, int *bb) { //int bb[4] = {minx,maxx,miny,maxy}
	int first = poly_first_vert(poly);
	int vert = first;
	float minx=std::numeric_limits<float>::max(), maxx=std::numeric_limits<float>::min();
	float miny=std::numeric_limits<float>::max(), maxy=std::numeric_limits<float>::min();
	do {
		double xyz[3]; vert_get_coord(vert, xyz);
		minx = fmin(xyz[0],minx);
		maxx = fmax(xyz[0],maxx);
		miny = fmin(xyz[1],miny);
		maxy = fmax(xyz[1],maxy);
		vert = v[vert].next;
	} while (vert && vert!=first);
	bb[0] = xs_FloorToInt((minx-llx)*inv_dx);
	bb[1] = xs_FloorToInt((maxx-llx)*inv_dx);
	bb[2] = xs_FloorToInt((miny-lly)*inv_dy);
	bb[3] = xs_FloorToInt((maxy-lly)*inv_dy);
}

// Sets is_interior flag of all vertices of poly and all vertices
// of polygons connected to poly's next_poly linked list
void GridMesh::poly_set_interior(int poly, bool interior) {
	poly = poly_first_vert(poly);
	for (; poly; poly=v[poly].next_poly) {
		int first = poly_first_vert(poly);
		int vert=first;
		do {
			v[vert].is_interior = interior;
			vert = v[vert].next;
		} while (vert&&vert!=first);
	}
}

#if defined(ENABLE_GLUT_DEMO)
void GridMesh::poly_center(int poly, float *cx, float *cy) {
	int vert = poly;
	double sum_x=0, sum_y=0;
	int n=0;
	do {
		double xyz[3]; vert_get_coord(vert, xyz);
		sum_x += xyz[0];
		sum_y += xyz[1];
		n += 1;
		vert = v[vert].next;
	} while (vert && vert!=poly && vert!=v[poly].first);
	*cx = sum_x/n;
	*cy = sum_y/n;
}

struct rgbcolor {unsigned char r,g,b;};
void GridMesh::poly_draw(int poly, float shrinkby, int maxedges) {
	// Generate a random but consistent color for each polygon
	rgbcolor color = {0,0,0};
	static std::map<int,rgbcolor> colormap;
	std::map<int,rgbcolor>::iterator it = colormap.find(poly);
	if (it==colormap.end()) {
		while (color.r<50) {color.r=rand();}
		while (color.g<50) {color.g=rand();}
		while (color.b<50) {color.b=rand();}
		colormap[poly] = color;
	} else {
		color = it->second;
	}
	for (; poly; poly=v[poly].next_poly) {
		if (v[poly].next==0) continue;
		NURBS_TESS_PRINTF("Poly %i: ",poly);
		// Find the center so that we can shrink towards it
		float cx,cy;
		poly_center(poly, &cx, &cy);
		// Draw the polygon
		glBegin(GL_LINES);
		glColor3ub(color.r, color.g, color.b);
		int v1 = poly;
		int num_drawn_edges = 0;
		do {
			int v2 = v[v1].next;
			NURBS_TESS_PRINTF("%i-%i, ",v1,v2);
			double v1xyz[3]; vert_get_coord(v1, v1xyz);
			double v2xyz[3]; vert_get_coord(v2, v2xyz);
			glVertex2f((1-shrinkby)*v1xyz[0]+shrinkby*cx, (1-shrinkby)*v1xyz[1]+shrinkby*cy);
			glVertex2f((1-shrinkby)*v2xyz[0]+shrinkby*cx, (1-shrinkby)*v2xyz[1]+shrinkby*cy);
			++num_drawn_edges;
			if (maxedges && num_drawn_edges>=maxedges)
				break;
			v1 = v2;
		} while (v1!=poly && v1!=v[v1].first);
		NURBS_TESS_PRINTF("\n");
		glEnd();
		// Draw the polygon verts
		glPointSize(3);
		glBegin(GL_POINTS);
		glColor3ub(color.r, color.g, color.b);
		v1 = poly;
		do {
			if (v[v1].is_interior) {
				glColor3ub(255,255,0);
			} else {
				glColor3ub(0,0,255);
			}
			double v1xyz[3]; vert_get_coord(v1, v1xyz);
			float x=v1xyz[0], y=v1xyz[1];
			float cx, cy; poly_center(v[v1].first, &cx, &cy);
			x = (1.0-shrinkby)*x + shrinkby*cx;
			y = (1.0-shrinkby)*y + shrinkby*cy;
			glVertex2f(x,y);
			v1 = v[v1].next;
		} while (v1!=poly && v1!=v[v1].first);
		glEnd();
	}
}
#endif


void GridMesh::find_cell_line_intersections(double x0, double y0, double x1, double y1,
											std::vector<std::pair<int,int> > *bottom_edges,
											std::vector<std::pair<int,int> > *left_edges,
											std::vector<std::pair<int,int> > *integer_cells) {
	find_integer_cell_line_intersections((x0-llx)*inv_dx,(y0-lly)*inv_dy,
										 (x1-llx)*inv_dx,(y1-lly)*inv_dy,
										 bottom_edges,left_edges,integer_cells);
}

void find_integer_cell_line_intersections(double x0, double y0, double x1, double y1,
											   std::vector<std::pair<int,int> > *bottom_edges,
											   std::vector<std::pair<int,int> > *left_edges,
											   std::vector<std::pair<int,int> > *integer_cells) {
	bool flipped_left_right = false;
	int cx0=xs_FloorToInt(x0), cy0=xs_FloorToInt(y0), cx1=xs_FloorToInt(x1), cy1=xs_FloorToInt(y1);
	// Line segments smaller than a cell's minimum dimension should always hit these trivial cases
	if (cy0==cy1) { //Horizontal or single-cell
		if (integer_cells) {
			if (cx0<cx1) {
				for (int i=cx0; i<=cx1; i++)
					integer_cells->push_back(std::make_pair(i,cy0));
			} else {
				for (int i=cx0; i>=cx1; i--)
					integer_cells->push_back(std::make_pair(i,cy0));
			}
		}
		if (left_edges) {
			if (cx0<cx1) {
				for (int i=cx0+1; i<=cx1; i++)
					left_edges->push_back(std::make_pair(i,cy0));
			} else {
				for (int i=cx0; i>cx1; i--)
					left_edges->push_back(std::make_pair(i,cy0));
			}
		}
		return;
	} else if (cx0==cx1) { // Vertical
		if (integer_cells) {
			if (cy0<cy1) {
				for (int i=cy0; i<=cy1; i++)
					integer_cells->push_back(std::make_pair(cx0,i));
			} else {
				for (int i=cy0; i>=cy1; i--)
					integer_cells->push_back(std::make_pair(cx1,i));
			}
		}
		if (bottom_edges) {
			if (cy0<cy1) {
				for (int i=cy0+1; i<=cy1; i++)
					bottom_edges->push_back(std::make_pair(cx0,i));
			} else {
				for (int i=cy0; i>cy1; i--)
					bottom_edges->push_back(std::make_pair(cx1,i));
			}
		}
		return;
	} else { // Line segments that make us think :)
		if (x0>x1) { // Reduce 4 cases (up and left, up and right, down and left, down and right) to 2
			flipped_left_right = true;
			std::swap(x0,x1);
			std::swap(y0,y1);
			std::swap(cx0,cx1);
			std::swap(cy0,cy1);
		}
		double m = (y1-y0)/(x1-x0);
		double residue_x=(cx0+1)-x0;
		double rhy = y0+residue_x*m; // y coord at the right edge of the cell
		if (cy1>cy0) { //Upwards and to the right
			int j; float jf;
			j=cy0; jf=cy0;
			for (int i=cx0; i<=cx1; i++) {
				if (i==cx1) rhy = y1;
				if (integer_cells) integer_cells->push_back(std::make_pair(i,j));
				while (jf+1<rhy) {
					j+=1; jf+=1.0;
					if (integer_cells) integer_cells->push_back(std::make_pair(i,j));
					if (bottom_edges) bottom_edges->push_back(std::make_pair(i,j));
				}
				if (i!=cx1 && left_edges) left_edges->push_back(std::make_pair(i+1, j));
				rhy += m;
			}
		} else { //Downwards and to the right
			int j; float jf;
			j=cy0; jf=cy0;
			for (int i=cx0; i<=cx1; i++) {
				if (i==cx1) rhy = y1;
				if (integer_cells) integer_cells->push_back(std::make_pair(i,j));
				while (jf>rhy) {
					if (bottom_edges) bottom_edges->push_back(std::make_pair(i,j));
					j-=1; jf-=1.0;
					if (integer_cells) integer_cells->push_back(std::make_pair(i,j));
				}
				if (i!=cx1 && left_edges) left_edges->push_back(std::make_pair(i+1, j));
				rhy += m;
			}
		}
		if (flipped_left_right) {
			if (integer_cells) std::reverse(integer_cells->begin(), integer_cells->end());
			if (bottom_edges) std::reverse(bottom_edges->begin(), bottom_edges->end());
			if (left_edges) std::reverse(left_edges->begin(), left_edges->end());
		}
	}
}

int GridMesh::insert_vert(int poly1left,
						  int poly1right,
						  int poly2left,
						  int poly2right,
						  double x1, double y1
						  ) {
	// Insert an intersection vertex into polygon 1
	int newv1 = vert_new(poly1left,poly1right);
	vert_set_coord(newv1, x1, y1, 0);
	v[newv1].is_intersection = true;
	
	// Insert an intersection vertex into polygon 2
	int newv2 = vert_new(poly2left,poly2right);
	vert_set_coord(newv2, x1, y1, 0);
	v[newv2].is_intersection = true;
	
	// Tell the intersection vertices that they're stacked on top of one another
	vert_add_neighbor(newv1, newv2);
	
	return newv1;
}

// gridmesh -> gridmesh (intersection) poly2
void GridMesh::bool_AND(int poly2) {
	if (recorded_AND) {recorded_AND->push_back(poly2); return;}
	int bb[4];
	poly_grid_BB(poly2, bb);
	int num_v, num_e; insert_vert_poly_gridmesh(poly2, &num_v, &num_e);
	bool add_poly_after_end = false;
	double p2xyz[3]; vert_get_coord(poly2, p2xyz);
	if (num_v==0 && num_e==0) {
		int p = poly_for_cell((float)p2xyz[0], (float)p2xyz[1]);
		if (p) {
			for (int subpoly=p; subpoly; subpoly=v[subpoly].next_poly) {
				if (point_in_polygon(p2xyz[0], p2xyz[1], subpoly)) {
					add_poly_after_end = true;
					break;
				}
			}
		}
	}
	// If we found intersections, the chalk-cart algo suffices
	label_interior_freepoly(poly2);
	label_interior_AND(poly2,false,bb);
	trim_to_odd();
	if (add_poly_after_end) {
		int p = poly_for_cell(float(p2xyz[0]), float(p2xyz[1]));
		v[p].next_poly = poly2;
	}
}

// gridmesh -> gridmesh (intersection) ~poly2
void GridMesh::bool_SUB(int poly2) {
	if (recorded_SUB) {recorded_SUB->push_back(poly2); return;}
	int bb[4];
	poly_grid_BB(poly2, bb);
	int num_v, num_e; insert_vert_poly_gridmesh(poly2, &num_v, &num_e);
	double p2xyz[3]; vert_get_coord(poly2, p2xyz);
	if (num_v==0 && num_e==0) {
		int p = poly_for_cell(float(p2xyz[0]), float(p2xyz[1]));
		double p2xyz[3]; vert_get_coord(poly2, p2xyz);
		for (int containing_poly=p; containing_poly; containing_poly=v[containing_poly].next_poly) {
			if (point_in_polygon(p2xyz[0], p2xyz[1], containing_poly)) {
				// We were in a polygon after all.
				punch_hole(containing_poly, poly2);
				break;
			}
		}
	} else {
		label_interior_freepoly(poly2);
		label_interior_AND(poly2,true,bb);
		trim_to_odd(bb);
	}
}

void GridMesh::poly_translate(int poly, double x, double y) {
	int vert=poly; do {
		double vxyz[3]; vert_get_coord(vert, vxyz);
		vxyz[0] += x;
		vxyz[1] += y;
		vert_set_coord(vert, vxyz[0], vxyz[1], vxyz[2]);
		vert = v[vert].next;
	} while (vert!=poly);
}

double GridMesh::poly_signed_area(int poly) {
	double a=0;
	int v0=poly; double v0xyz[3]; vert_get_coord(v0, v0xyz);
	int v1=v[poly].next; double v1xyz[3]; vert_get_coord(v1, v1xyz);
	int v2=v[v1].next; double v2xyz[3]; vert_get_coord(v2, v2xyz);
	while (v2 && v2!=poly) {
		double v01x=v1xyz[0]-v0xyz[0], v01y=v1xyz[1]-v0xyz[1];
		double v02x=v2xyz[0]-v0xyz[0], v02y=v2xyz[1]-v0xyz[1];
		a += v01x*v02y - v02x*v01y;
		v1=v2; v1xyz[0]=v2xyz[0]; v1xyz[1]=v2xyz[1];
		v2=v[v2].next;
		vert_get_coord(v2, v2xyz);
	}
	return a*0.5;
}

void GridMesh::poly_flip_winding_direction(int poly) {
	int vert=poly;
	do {
		int old_prev=v[vert].prev, old_next=v[vert].next;
		v[vert].prev=old_next;
		v[vert].next=old_prev;
		vert = old_next;
	} while (vert!=poly);
}


void GridMesh::punch_hole(int exterior, int hole) {
	double a_ext=poly_signed_area(exterior), a_int=poly_signed_area(hole);
	if ((a_ext>0&&a_int>0)  ||  (a_ext<0&&a_int<0)) {
		poly_flip_winding_direction(hole);
	}
	int v1=exterior, v2=hole;
	bool v1v2_intersection_free = false;
	while (!v1v2_intersection_free) {
		double v1xyz[3]; vert_get_coord(v1, v1xyz);
		double v2xyz[3]; vert_get_coord(v2, v1xyz);
		v1v2_intersection_free = true;
		std::vector<IntersectingEdge> isect_ext = edge_poly_intersections(v1xyz[0],v1xyz[1],v2xyz[0],v2xyz[1], exterior);
		for (std::vector<IntersectingEdge>::iterator ie=isect_ext.begin(); ie!=isect_ext.end(); ie++) {
			if (ie->alpha1>tolerance && ie->alpha1<(1-tolerance)) {
				v1v2_intersection_free = false;
				v1 = ie->e2;
				break;
			}
		}
		if (!v1v2_intersection_free) continue;
		std::vector<IntersectingEdge> isect_hole = edge_poly_intersections(v1xyz[0],v1xyz[1],v2xyz[0],v2xyz[1], hole);
		for (std::vector<IntersectingEdge>::iterator ie=isect_hole.begin(); ie!=isect_hole.end(); ie++) {
			if (ie->alpha1>tolerance && ie->alpha1<(1-tolerance)) {
				v1v2_intersection_free = false;
				v2 = ie->e2;
				break;
			}
		}
	}
	int int_c=v2, int_r=v[v2].next;
	int ext_c=v1, ext_r=v[v1].next;
	int int_cc=vert_dup(int_c), ext_cc=vert_dup(ext_c);
	v[int_c].next=ext_cc; v[ext_cc].prev=int_c;
	v[ext_cc].next=ext_r; v[ext_r].prev=ext_cc;
	v[ext_c].next=int_cc; v[int_cc].prev=ext_c;
	v[int_cc].next=int_r; v[int_r].prev=int_cc;
	int first = v[ext_c].first;
	int vert = ext_c; do {
		v[vert].first = first;
		vert = v[vert].next;
	} while (vert!=ext_c);
}


static bool intersection_edge_order(const IntersectingEdge& e1, const IntersectingEdge& e2) {
	double diff = e1.alpha1-e2.alpha1;
	if (abs(diff)<1e-5 && e1.cellidx!=e2.cellidx) {
		return e1.cellidx < e2.cellidx;
	}
	return diff<0;
}
void GridMesh::insert_vert_poly_gridmesh(int mpoly, int *verts_added, int *edges_intersected) {
	std::vector<std::pair<int,int> > bottom_edges, left_edges, integer_cells;
	mpoly = poly_first_vert(mpoly);
	int v1 = mpoly;
	double v1xyz[3]; vert_get_coord(v1, v1xyz);
	int verts_added_local = 0;
	int edges_intersected_local = 0;
	while (v[v1].next) {
		int v2 = v[v1].next;
		/**** Step 1: find all intersections of the edge v1,v2 vs the grid ****/
		double v2xyz[3]; vert_get_coord(v2, v2xyz);
		integer_cells.clear();
		bottom_edges.clear();
		left_edges.clear();
		find_cell_line_intersections(v1xyz[0],v1xyz[1],v2xyz[0],v2xyz[1],
									 &bottom_edges,&left_edges,&integer_cells);
		// Step 2: flip the even/odd#intersections indicators on the respective edges
		int num_bottom_edge_isects = int(bottom_edges.size());
		for (int ei_num=0; ei_num<num_bottom_edge_isects; ei_num++) {
			std::pair<int,int> xy = bottom_edges[ei_num];
			int ie_isect_idx = gridpt_for_cell(xy.first, xy.second);
			bool even_odd = ie_isect_right[ie_isect_idx];
			ie_isect_right[ie_isect_idx] = !even_odd;
		}
		int num_left_edge_isects = int(left_edges.size());
		for (int ei_num=0; ei_num<num_left_edge_isects; ei_num++) {
			std::pair<int,int> xy = left_edges[ei_num];
			int ie_isect_idx = gridpt_for_cell(xy.first, xy.second);
			bool even_odd = ie_isect_up[ie_isect_idx];
			ie_isect_up[ie_isect_idx] = !even_odd;
		}
		edges_intersected_local += num_bottom_edge_isects + num_left_edge_isects;
		
		// Step 3: turn "line passed through cell" events from raster algo
		// into actual intersections by intersecting againt every edge in the cell,
		// sorting so that even in the case of coincident edges we leave one
		// polygon before entering the other.
		std::vector<IntersectingEdge> isect;
		for (size_t i=0,l=integer_cells.size(); i<l; i++) {
			std::pair<int,int> j = integer_cells[i];
			int cell_polys = poly_for_cell(j.first, j.second);
			v[cell_polys].is_pristine = 0;
			for (int cell_poly=cell_polys; cell_poly; cell_poly=v[cell_poly].next_poly) {
				if (!cell_poly || !v[cell_poly].next) continue;
				std::vector<IntersectingEdge> isect_tmp = edge_poly_intersections(v1, cell_poly);
				for (std::vector<IntersectingEdge>::iterator e=isect_tmp.begin(); e!=isect_tmp.end(); e++) {
					//NURBS_TESS_PRINTF("(%i,%i)",j.first,j.second);
					e->cellidx = int(i);
				}
				//NURBS_TESS_PRINTF("\n");
				isect.insert(isect.end(),isect_tmp.begin(),isect_tmp.end());
			}
		}
		std::stable_sort(isect.begin(),isect.end(),intersection_edge_order);
		
		/**** Step 4: insert verts at the intersections we discovered ****/
		for (std::vector<IntersectingEdge>::iterator ie=isect.begin(); ie!=isect.end(); ie++) {
			v1 = insert_vert(v1, v2, ie->e2, v[ie->e2].next, ie->x, ie->y);
		}
		verts_added_local += isect.size();
		v1=v2; v1xyz[0]=v2xyz[0]; v1xyz[1]=v2xyz[1];
		if (v1==mpoly) break;
	}
	if (verts_added) *verts_added = verts_added_local;
	if (edges_intersected) *edges_intersected = edges_intersected_local;
}

void GridMesh::label_interior_AND(int poly2, bool invert_poly2, int *bb) {
	// Step 1: Label cells that are definitely in the exterior of the boolean result.
	int bb_local[4];
	if (!bb) {
		bb = bb_local;
		poly_grid_BB(poly2, bb);
	}
	int minx=bb[0], maxx=bb[1], miny=bb[2], maxy=bb[3];
	if (!invert_poly2)
		label_exterior_cells(poly2, false, bb);
	// Step 2: Ensure that the lower left corner is labeled correctly
	int ll_gridpt = gridpt_for_cell(0, 0);
	if (ie_grid[ll_gridpt]) { // false => not in interior. true => anything's possible
		std::pair<float,float> llxy = cell_ll_corner(0,0);
		ie_grid[1] = point_in_polygon(llxy.first, llxy.second, poly2) ^ invert_poly2;
	}
	/* Step 3: propagate the label to all other gridpoints
	for (int y=0; y<=ny; y++) {
		bool cur_ie = false;
		int below_idx=gridpt_for_cell(0, y-1), cur_idx=gridpt_for_cell(0, y);
		if (y!=0) {
			cur_ie = ie_grid[cur_idx] = ie_grid[below_idx] ^ ie_isect_up[below_idx];
		} else {
			cur_ie = ie_grid[cur_idx];
		}
		for (int x=0; x<nx; x++) {
			cur_ie = cur_ie ^ ie_isect_right[cur_idx];
			ie_grid[++cur_idx] = cur_ie;
		}
	}
	// Step 4: Use interior/exterior calls on gridpt verts to label all verts
	for (int y=miny; y<=maxy; y++) {
		for (int x=minx; x<=maxx; x++) {
			known_corner_t known_verts = KNOWN_CORNER_ALL;
			if (!ie_grid[gridpt_for_cell(x,y)])     known_verts += KNOWN_CORNER_LL_EXTERIOR;
			if (!ie_grid[gridpt_for_cell(x+1,y)])   known_verts += KNOWN_CORNER_LR_EXTERIOR;
			if (!ie_grid[gridpt_for_cell(x+1,y+1)]) known_verts += KNOWN_CORNER_UR_EXTERIOR;
			if (!ie_grid[gridpt_for_cell(x,y+1)])   known_verts += KNOWN_CORNER_UL_EXTERIOR;
			label_interior_cell(poly_for_cell(x, y), poly2, invert_poly2, known_verts);
		}
	}*/
	// Alternative Step 3+4
	known_corner_t known_verts_x0=0, known_verts_xsweep;
	for (int y=miny; y<=maxy; y++) {
		known_verts_x0 = KNOWN_CORNER_NEXTY(known_verts_x0);
		known_verts_x0 = label_interior_cell(poly_for_cell(minx, y),
											 poly2,
											 invert_poly2,
											 known_verts_x0);
		known_verts_xsweep = known_verts_x0;
		for (int x=minx+1; x<=maxx; x++) {
			known_verts_xsweep = KNOWN_CORNER_NEXTX(known_verts_xsweep);
			known_verts_xsweep = label_interior_cell(poly_for_cell(x, y),
													 poly2,
													 invert_poly2,
													 known_verts_xsweep);
		}
	}
}

void GridMesh::label_interior_SUB(int poly2, int *bb) {
	label_interior_AND(poly2, true, bb);
}

void GridMesh::label_exterior_cells(int poly, bool interior_lbl, int* bb) {
	int bb_local[4]; //int bb[4] = {minx,maxx,miny,maxy}
	if (!bb) {
		bb = bb_local;
		poly_grid_BB(poly, bb);
	}
	int minx=bb[0], maxx=bb[1], miny=bb[2], maxy=bb[3];
	for (int y=0; y<ny; y++) { // Left of poly
		for (int x=0,xlim=std::min(nx,minx); x<xlim; x++) {
			poly_set_interior(poly_for_cell(x, y), interior_lbl);
		}
	}
	for (int y=0; y<ny; y++) { // Right of poly
		for (int x=maxx+1; x<nx; x++) {
			poly_set_interior(poly_for_cell(x, y), interior_lbl);
		}
	}
	for (int y=0; y<miny; y++) { // Bottom of poly
		for (int x=minx; x<=maxx; x++) {
			poly_set_interior(poly_for_cell(x, y), interior_lbl);
		}
	}
	for (int y=maxy+1; y<ny; y++) { // Top of poly
		for (int x=minx; x<=maxx; x++) {
			poly_set_interior(poly_for_cell(x, y), interior_lbl);
		}
	}

}

// cell's next_poly list is considered
// poly2's next_poly list is ignored
known_corner_t GridMesh::label_interior_cell(int cell, int poly2, bool bool_SUB, known_corner_t kin) {
	NURBS_TESS_PRINTF("%i kin:%i=",cell,int(kin)); print_kc(kin);
	NURBS_TESS_PRINTF("\n");
	bool interior = false;
	known_corner_t ret=0;
	for (int poly=cell; poly; poly=v[poly].next_poly) {
		NURBS_TESS_PRINTF("   subpoly:%i DEG=%i\n",poly,int(v[poly].next==0));
		if (v[poly].next==0) continue; // Skip degenerate polys
		// First, try to find a known corner
		bool found_known_corner = false;
		int kc_vert=poly;
		if (kin) {
			do {
				char k = v[kc_vert].corner;
				if (k && kin&KNOWN_CORNER(k-1)) {
					found_known_corner = true;
					interior = !(kin&KNOWN_CORNER_EXTERIOR(k-1));
					NURBS_TESS_PRINTF("   %i k_propagate->%i.interior:%i sub:%i\n",poly, kc_vert, int(interior),int(bool_SUB));
					break;
				}
				kc_vert = v[kc_vert].next;
			} while (kc_vert && kc_vert!=poly);
		}
		// If using a known corner didn't work, use the slow PIP test to find a known corner
		if (!found_known_corner) {
			double polyxyz[3]; vert_get_coord(poly, polyxyz);
			interior = point_in_polygon(polyxyz[0], polyxyz[1], poly2);
			if (bool_SUB) interior = !interior;
			NURBS_TESS_PRINTF("   %i pip->%i.interior:%i sub:%i\n",poly, poly, int(interior),int(bool_SUB));
		}
		// One way or another, (bool)interior is good now.
		int vert = kc_vert;
		do {
			if (v[vert].is_intersection) {
				v[vert].is_interior = true;
				interior = !interior;
				v[vert].is_entry = interior; // If we were in the interior, this isn't an entry point
			} else {
				v[vert].is_interior = interior;
				char k = v[vert].corner;
				if (k) {
					ret |= KNOWN_CORNER(k-1);
					if (!interior) ret |= KNOWN_CORNER_EXTERIOR(k-1);
				}
			}
			NURBS_TESS_PRINTF("   %i is_interior:%i is_intersection:%i next:%i\n",vert,int(v[vert].is_interior),int(v[vert].is_intersection),v[vert].next);
			vert = v[vert].next;
		} while (vert && vert!=kc_vert);
	}
	return ret;
}

void GridMesh::trim_to_odd(int *bb) {
	//int bb[] = {minx,maxx,miny,maxy}
	int minx=0, maxx=nx-1, miny=0, maxy=ny-1;
	if (bb) {
		minx=bb[0]; maxx=bb[1]; miny=bb[2]; maxy=bb[3];
	}
	for (int j=miny; j<=maxy; j++) {
		for (int i=minx; i<=maxx; i++) {
			NURBS_TESS_PRINTF("tto %i,%i\n",i,j);
			trim_to_odd(poly_for_cell(i,j));
		}
	}
}

void GridMesh::trim_to_odd(int poly0) {
	int previous_trace_poly = 0;
	int this_trace_poly = 0;
	std::vector<int> trace_origins;
	std::vector<int> trace; // Holds verts of the polygon being traced
	trace.reserve(256);
	for (int poly=poly0, next_poly=v[poly0].next_poly;
		 poly;
		 poly=next_poly, next_poly=v[poly].next_poly) {
		if (!v[poly].next) continue;
		NURBS_TESS_PRINTF("   poly %i\n",poly);
		trace_origins.push_back(poly);
		while (trace_origins.size()) {
			trace.clear();
			int vert = *trace_origins.rbegin(); trace_origins.pop_back();
			GridMeshVert *vvert = &v[vert];
			// Move until vert = valid starting vertex
			bool bail=false;
			while (!(    (vvert->is_intersection)
					 || (!vvert->is_intersection && vvert->is_interior))) {
				if (vvert->is_used) {
					bail = true;
					break;
				}
				vvert->is_used = true;
				vert = vvert->next;
				vvert->next = 0; // Trail of destruction: no polygons should be
				vvert->prev = 0; // generated in the excluded region
				vvert = &v[vert];
			}
			if (bail) continue; // No valid starting vertex? Bail!
			
			// We're still sitting at the first valid vertex.
			// Overview: follow its edge, record points into trace,
			// record branches into trace_origins
			bool traverse_foreward = true;
			bool can_next_step_branch = false;
			this_trace_poly = vert;
			while (vert && !v[vert].is_used) {
				trace.push_back(vert);
				int next;
				int neighbor = v[vert].neighbor;
				if (v[vert].first==poly) { // We are tracing an edge of the parent poly
					if (neighbor && can_next_step_branch) {
						trace_origins.push_back(v[vert].next);
						next = neighbor;
						traverse_foreward = v[neighbor].is_entry;
						can_next_step_branch = false;
					} else {
						next = v[vert].next;
						can_next_step_branch = true;
					}
				} else { // We are tracing an edge of a cutting poly
					if (v[neighbor].first==poly && can_next_step_branch) {
						next = neighbor;
						traverse_foreward = true;
						can_next_step_branch = false;
					} else {
						next = (traverse_foreward)? v[vert].next : v[vert].prev;
						can_next_step_branch = true;
					}
				}
				v[vert].is_used = true;
				vert = next;
			}
			
			size_t trace_sz = trace.size();
			if (trace_sz) {
				int first = trace[0];
				NURBS_TESS_PRINTF("   0poly %i.%i: ",poly,this_trace_poly);
				for (std::vector<int>::iterator i=trace.begin(); i!=trace.end(); i++) {
					NURBS_TESS_PRINTF(",%i",i);
				}
				NURBS_TESS_PRINTF("\n");
				// Link all but the endpoints, skipping doubles
				for (int i=1,l=int(trace.size()); i<l; i++) {
					int left=trace[i-1], right=trace[i];
					if (v[left].neighbor==right) {
						if (i==l-1) {
							trace.pop_back();
						} else {
							right = trace[(++i)];
						}
					}
					v[left].next = right;
					v[right].prev = left;
				}
				int last = *trace.rbegin();
				if (v[last].neighbor==first) {
					last = v[last].prev;
				}
				v[last].next = first;
				v[first].prev = last;
#if defined(NURBS_TESS_DEBUG)
				NURBS_TESS_PRINTF("   2poly %i.%i: ",poly,this_trace_poly);
				vert=first; do {
					NURBS_TESS_PRINTF(",%i",vert);
					vert = v[vert].next;
				} while (vert!=first);
				NURBS_TESS_PRINTF("\n");
#endif
				// Hook up the backbone
				if (!previous_trace_poly) {
					v[poly0].next_poly = this_trace_poly;
				} else {
					if (previous_trace_poly==this_trace_poly) NURBS_TESS_PRINTF("Poly-list assembly failed.");
					v[previous_trace_poly].next_poly = this_trace_poly;
				}
				v[this_trace_poly].next_poly = 0;
				previous_trace_poly = this_trace_poly;
			}
		}
		NURBS_TESS_PRINTF("   poly@end:%i\n",poly);
	}
	for (int poly=poly0; poly; poly=v[poly].next_poly) {
		int vert = poly;
		do {
			v[vert].first = poly;
			v[vert].is_intersection = false;
			v[vert].is_interior = true;
			v[vert].is_used = false;
			v[vert].neighbor = 0;
			vert = v[vert].next;
		} while (vert && vert!=poly);
	}
}


void GridMesh::label_interior_freepoly(int poly) {
	double xyz[3]; vert_get_coord(poly, xyz);
	int over_poly = poly_for_cell(float(xyz[0]),float(xyz[1]));
	std::set<int> inside; // The set of polygons we are currently inside
	for (int p=over_poly; p; p=v[p].next_poly) {
		if (!point_in_polygon(xyz[0], xyz[1], p)) continue;
		if (inside.count(p)) {
			inside.erase(p);
		} else {
			inside.insert(p);
		}
	}
	for (int vert=poly; vert; vert=v[vert].next) {
		if (v[vert].is_intersection) {
			int neighbor_poly = v[v[vert].neighbor].first;
			if (inside.count(neighbor_poly)) {
				v[vert].is_entry = false;
				inside.erase(neighbor_poly);
			} else {
				v[vert].is_entry = true;
				inside.insert(neighbor_poly);
			}
		}
		if (v[vert].next==poly) break;
	}
}

std::vector<IntersectingEdge> GridMesh::edge_poly_intersections(int e1, int p) {
	std::vector<IntersectingEdge> ret;
	bool first_iter = true;
	for (int e2=p; e2!=p||first_iter; e2=v[e2].next) {
		int e1n = v[e1].next;
		int e2n = v[e2].next;
		double e1xyz[3]; vert_get_coord(e1, e1xyz);
		double e1nxyz[3]; vert_get_coord(e1n, e1nxyz);
		double e2xyz[3]; vert_get_coord(e2, e2xyz);
		double e2nxyz[3]; vert_get_coord(e2n, e2nxyz);
		double ax=e1xyz[0], ay=e1xyz[1];
		double bx=e1nxyz[0], by=e1nxyz[1];
		double cx=e2xyz[0], cy=e2xyz[1];
		double dx=e2nxyz[0], dy=e2nxyz[1];
		double ix, iy, alpha1; // Intersection info
		int isect = line_line_intersection(ax, ay, bx, by, cx, cy, dx, dy, &ix, &iy, &alpha1);
		if (isect) {
			ret.push_back(IntersectingEdge(ix,iy,alpha1,e2,0));
		}
		first_iter = false;
	}
	return ret;
}

std::vector<IntersectingEdge> GridMesh::edge_poly_intersections(double ax, double ay, double bx, double by, int p) {
	std::vector<IntersectingEdge> ret;
	bool first_iter = true;
	for (int e2=p; e2!=p||first_iter; e2=v[e2].next) {
		int e2n = v[e2].next;
		double e2xyz[3]; vert_get_coord(e2, e2xyz);
		double e2nxyz[3]; vert_get_coord(e2n, e2nxyz);
		double cx=e2xyz[0], cy=e2xyz[1];
		double dx=e2nxyz[0], dy=e2nxyz[1];
		double ix, iy, alpha1; // Intersection info
		int isect = line_line_intersection(ax, ay, bx, by, cx, cy, dx, dy, &ix, &iy, &alpha1);
		if (isect) {
			ret.push_back(IntersectingEdge(ix,iy,alpha1,e2,0));
		}
		first_iter = false;
	}
	return ret;
}

// Returns true if p1,p2,p3 form a tripple that is in counter-clockwise
// orientation. In other words, if ((p2-p1)x(p3-p2)).z >0
inline bool points_ccw(double x1, double y1, double x2, double y2, double x3, double y3) {
	double z = x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2);
	return z>0;
}

int line_line_intersection(double ax, double ay, // Line 1, vert 1 A
						   double bx, double by, // Line 1, vert 2 B
						   double cx, double cy, // Line 2, vert 1 C
						   double dx, double dy, // Line 2, vert 2 D
						   double *ix, double *iy, // Intersection point
						   double *alpha1
) {
	double shallow_ang_tol = 1e-6; // sin^2th < ang_tol   =>  return false
	double endpt_frac_tol = 1e-6;  // alpha1 or alpha2 < endpt_frac_tol  => ret 0
	// (ax,ay)--------(bx,by)
	//   (cx,cy)------------(dx,dy)
	// Do they intersect? If so, return true and fill alpha{1,2} with the
	// affine interpolation params necessary to find the intersection.
	bool ccw_acd = points_ccw(ax,ay, cx,cy, dx,dy); // could reformulate to extract point-line distances
	bool ccw_bcd = points_ccw(bx,by, cx,cy, dx,dy);
	if (ccw_acd==ccw_bcd) return false;
	bool ccw_abc = points_ccw(ax,ay, bx,by, cx,cy);
	bool ccw_abd = points_ccw(ax,ay, bx,by, dx,dy);
	if (ccw_abc==ccw_abd) return false;
	double a11 = bx - ax;
	double a12 = cx - dx;
	double a21 = by - ay;
	double a22 = cy - dy;
	double ABsq = a11*a11+a21*a21;
	double CDsq = a12*a12+a22*a22;
	double det = a11*a22-a12*a21; //~=0 iff colinear
	if (fabs(det*det)<shallow_ang_tol*ABsq*CDsq) return false; // Almost || means no intersection for our purposesa
	double idet = 1/det;
	double b1 = cx-ax;
	double b2 = cy-ay;
	double x1 = (+b1*a22 -b2*a12)*idet; // alpha1
	double x2 = (-b1*a21 +b2*a11)*idet; // alpha2
	if (   x1<endpt_frac_tol || x1>(1-endpt_frac_tol)
		|| x2<endpt_frac_tol || x2>(1-endpt_frac_tol)) {
		return false;
	}
	*ix = (1.0-x1)*ax + x1*bx;
	*iy = (1.0-x1)*ay + x1*by;
	*alpha1 = x1;
#if defined(NURBS_TESS_DEBUG)
	double ix2 = (1.0-x2)*cx + x2*dx;
	double iy2 = (1.0-x2)*cy + x2*dy;
	if (fabs(*ix-ix2)>.001) printf("Bug detected in intersection math.\n");
	if (fabs(*iy-iy2)>.001) printf("Bug detected in intersection math.\n");
#endif
	return true;
}

//  pi/2<=theta<pi     1   0   0<=theta<pi/2
//                       v
//  pi<=theta<3pi/2    2   3   3pi/2<=theta<2pi
inline int quadrant(float x, float y, float vx, float vy) {
	if (y>vy) { // Upper half-plane is easy
		return x<=vx ? 1 : 0;
	} else { // y<=vy
		if (y<vy) { // So is lower half-plane
			return 2+int(x>=vx);
		} else { // y==0
			if (x>vx) return 0;
			else if (x<vx) return 2;
			return 99; // x==vx, y==vy
		}
	}
}

bool GridMesh::point_in_polygon(double x, double y, int poly) {
	bool contains_boundary = true;
	double xyz[3]; vert_get_coord(poly, xyz);
	float last_x=xyz[0], last_y=xyz[1];
	int last_quadrant = quadrant(last_x,last_y,x,y);
	if (last_quadrant==99) return contains_boundary;
	int ccw = 0; // Number of counter-clockwise quarter turns around pt
	for (int vert=v[poly].next; vert; vert=v[vert].next) {
		vert_get_coord(vert, xyz);
		float next_x=xyz[0], next_y=xyz[1];
		int next_quadrant = quadrant(next_x, next_y, x, y);
		if (next_quadrant==99) return contains_boundary;
		int delta = next_quadrant-last_quadrant;
		if (delta==1 || delta==-3) {
			ccw += 1;
		} else if (delta==-1 || delta==3) {
			ccw -= 1;
		} else if (delta==2 || delta==-2) {
			double a11 = last_x-x;
			double a12 = next_x-x;
			double a21 = last_y-y;
			double a22 = next_y-y;
			double det = a11*a22-a12*a21;
			if (fabs(det)<1e-5) return contains_boundary;
			ccw += (det>0)? 2 : -2;
		}
		last_quadrant = next_quadrant;
		last_x=next_x; last_y=next_y;
		if (vert==poly) break;
	}
	// Note: return is_even to exclude self-intersecting regions
	return ccw!=0;
}
