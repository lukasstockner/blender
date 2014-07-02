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
#include "GridMesh.h"

static bool debug = 1;
float GridMesh::tolerance = 1e-5;


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


void GridMesh::set_ll_ur(double lowerleft_x, double lowerleft_y,
						 double upperright_x, double upperright_y) {
	llx = lowerleft_x; lly = lowerleft_y;
	urx = upperright_x; ury = upperright_y;
	double Dx = urx-llx;
	double Dy = ury-lly;
	dx = Dx/nx;
	dy = Dy/ny;
	inv_dx = 1.0/dx;
	inv_dy = 1.0/dy;
}

GridMesh::GridMesh(double lowerleft_x, double lowerleft_y,
				   double upperright_x, double upperright_y,
				   int num_x_cells, int num_y_cells) {
	nx = num_x_cells; ny = num_y_cells;
	set_ll_ur(lowerleft_x, lowerleft_y, upperright_x, upperright_y);
	v_capacity = nx*ny*4 + 256;
	v_count = nx*ny*4+1;
	v = (GreinerV2f*)malloc(sizeof(GreinerV2f)*v_capacity);
	new (v) GreinerV2f();
	v->x = v->y = -1234;
	for (int j=0; j<ny; j++) {
		double b = lly + j*dy;
		double t = (j==ny-1)? ury : lly + (j+1)*dy;
		for (int i=0; i<nx; i++) {
			double l = llx + i*dx;
			double r = (i==nx-1)? urx : llx + (i+1)*dx;
			GreinerV2f *v1 = &v[poly_for_cell(i, j)];
			GreinerV2f *v2 = v1+1;
			GreinerV2f *v3 = v1+2;
			GreinerV2f *v4 = v1+3;
			new (v1) GreinerV2f(); v1->x=l; v1->y=b;
			new (v2) GreinerV2f(); v2->x=r; v2->y=b;
			new (v3) GreinerV2f(); v3->x=r; v3->y=t;
			new (v4) GreinerV2f(); v4->x=l; v4->y=t;
			int iv1 = vert_id(v1);
			int iv2 = iv1+1;
			int iv3 = iv1+2;                                 // 13   + 1
			int iv4 = iv1+3;                                 // 02
			v1->next = iv2; v2->prev = iv1; v1->first = iv1; v1->corner = 1;
			v2->next = iv3; v3->prev = iv2; v2->first = iv1; v2->corner = 3;
			v3->next = iv4; v4->prev = iv3; v3->first = iv1; v3->corner = 4;
			v4->next = iv1; v1->prev = iv4; v4->first = iv1; v4->corner = 2;
		}
	}
}

int GridMesh::vert_new() {
	if (v_count>=v_capacity) {
		long newcap = v_capacity*2;
		v = (GreinerV2f*)realloc(v, newcap*sizeof(GreinerV2f));
		v_capacity = newcap;
	}
	new (v+v_count) GreinerV2f();
	return int(v_count++);
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

int GridMesh::poly_for_cell(int x, int y) {
	if (x<0||x>=nx) return 0;
	if (y<0||y>=nx) return 0;
	return 1+4*(y*nx+x);
}

int GridMesh::poly_for_cell(float fx, float fy) {
	int x = floor((fx-llx)*inv_dx);
	if (x<0||x>=nx) return 0;
	int y = floor((fy-lly)*inv_dy);
	if (y<0||y>=nx) return 0;
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
		if (fabs(x-v[vert].x)+fabs(y-v[vert].y)<tolerance) return vert;
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

std::pair<int,int> GridMesh::vert_grid_cell(int vert) {
	// vert = 1+4*(y*nx+x)
	int ynx_plus_x = (vert-1)/4;
	int y = ynx_plus_x/nx;
	int x = ynx_plus_x%nx;
	return std::make_pair(x,y);
}

void GridMesh::poly_grid_BB(int poly, int *bb) { //int bb[4] = {minx,maxx,miny,maxy}
	int first = poly_first_vert(poly);
	int vert = first;
	float minx=std::numeric_limits<float>::max(), maxx=std::numeric_limits<float>::min();
	float miny=std::numeric_limits<float>::max(), maxy=std::numeric_limits<float>::min();
	do {
		GreinerV2f& g = v[vert];
		minx = fmin(g.x,minx);
		maxx = fmax(g.x,maxx);
		miny = fmin(g.y,miny);
		maxy = fmax(g.y,maxy);
		vert = g.next;
	} while (vert && vert!=first);
	bb[0] = xs_FloorToInt(minx);
	bb[1] = xs_FloorToInt(maxx);
	bb[2] = xs_FloorToInt(miny);
	bb[3] = xs_FloorToInt(maxy);
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
		sum_x += v[vert].x;
		sum_y += v[vert].y;
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
		printf("Poly %i: ",poly);
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
			printf("%i-%i, ",v1,v2);
			glVertex2f((1-shrinkby)*v[v1].x+shrinkby*cx, (1-shrinkby)*v[v1].y+shrinkby*cy);
			glVertex2f((1-shrinkby)*v[v2].x+shrinkby*cx, (1-shrinkby)*v[v2].y+shrinkby*cy);
			++num_drawn_edges;
			if (maxedges && num_drawn_edges>=maxedges)
				break;
			v1 = v2;
		} while (v1!=poly && v1!=v[v1].first);
		puts("");
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
			float x=v[v1].x, y=v[v1].y;
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
											std::vector<std::pair<int,int>> *bottom_edges,
											std::vector<std::pair<int,int>> *left_edges,
											std::vector<std::pair<int,int>> *integer_cells) {
	find_integer_cell_line_intersections((x0-llx)*inv_dx,(y0-lly)*inv_dy,
										 (x1-llx)*inv_dx,(y1-lly)*inv_dy,
										 bottom_edges,left_edges,integer_cells);
}

void find_integer_cell_line_intersections(double x0, double y0, double x1, double y1,
											   std::vector<std::pair<int,int>> *bottom_edges,
											   std::vector<std::pair<int,int>> *left_edges,
											   std::vector<std::pair<int,int>> *integer_cells) {
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
	v[newv1].x = x1;
	v[newv1].y = y1;
	v[newv1].is_intersection = true;
	
	// Insert an intersection vertex into polygon 2
	int newv2 = vert_new(poly2left,poly2right);
	v[newv2].x = x1;
	v[newv2].y = y1;
	v[newv2].is_intersection = true;
	
	// Tell the intersection vertices that they're stacked on top of one another
	vert_add_neighbor(newv1, newv2);
	
	return newv1;
}

static bool intersection_edge_order(const IntersectingEdge& e1, const IntersectingEdge& e2) {
	double diff = e1.alpha1-e2.alpha1;
	if (abs(diff)<1e-5 && e1.cellidx!=e2.cellidx) {
		return e1.cellidx < e2.cellidx;
	}
	return diff<0;
}
int GridMesh::insert_vert_poly_gridmesh(int mpoly) {
	std::vector<std::pair<int,int>> bottom_edges, left_edges, integer_cells;
	mpoly = poly_first_vert(mpoly);
	int v1 = mpoly;
	float v1x=v[v1].x, v1y=v[v1].y;
	int verts_added = 0;
	while (v[v1].next) {
		int v2 = v[v1].next;
		// Step 1: find all intersections of the edge v1,v2
		float v2x=v[v2].x, v2y=v[v2].y;
		//printf("(%f,%f)---line--(%f,%f)\n",v1x,v1y,v2x,v2y);
		integer_cells.clear();
		find_cell_line_intersections(v1x,v1y,v2x,v2y,nullptr,nullptr,&integer_cells);
		std::vector<IntersectingEdge> isect;
		for (size_t i=0,l=integer_cells.size(); i<l; i++) {
			std::pair<int,int> j = integer_cells[i];
			int cell_poly = poly_for_cell(j.first, j.second);
			if (!cell_poly) continue;
			std::vector<IntersectingEdge> isect_tmp = edge_poly_intersections(v1, cell_poly);
			for (IntersectingEdge& e : isect_tmp) {
				//printf("(%i,%i)",j.first,j.second);
				e.cellidx = int(i);
			}
			//printf("\n");
			isect.insert(isect.end(),isect_tmp.begin(),isect_tmp.end());
		}
		std::stable_sort(isect.begin(),isect.end(),intersection_edge_order);
		// Step 2: insert them
		for (IntersectingEdge ie : isect) {
			v1 = insert_vert(v1, v2, ie.e2, v[ie.e2].next, ie.x, ie.y);
		}
		verts_added += isect.size();
		v1=v2; v1x=v2x; v1y=v2y;
		if (v1==mpoly) break;
	}
	return verts_added;
}

void GridMesh::label_interior_AND(int poly2, bool invert_poly2) {
	int bb[4];
	poly_grid_BB(poly2, bb);
	int minx=bb[0], maxx=bb[1], miny=bb[2], maxy=bb[3];
	if (!invert_poly2)
		label_exterior_cells(poly2, false, bb);
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

void GridMesh::label_interior_SUB(int poly2) {
	label_interior_AND(poly2, true);
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

// lr,ul = {0=unknown, 1=known_exterior, 2=known_interior}
known_corner_t GridMesh::label_interior_cell(int cell, int poly2, bool bool_SUB, known_corner_t kin) {
	//printf("%i kin:%i\n",cell,int(kin));
	bool interior = false;
	known_corner_t ret=0;
	for (int poly=cell; poly; poly=v[poly].next_poly) {
		if (v[poly].next==0) continue; // Skip degenerate polys
		// First, try to find a known corner
		bool found_known_corner = false;
		if (kin) {
			int vert = cell;
			do {
				char k = v[vert].corner;
				if (k && kin|KNOWN_CORNER(k-1)) {
					found_known_corner = true;
					interior = !(kin&KNOWN_CORNER_EXTERIOR(k-1));
					break;
				}
				vert = v[vert].next;
			} while (vert && vert!=cell);
		}
		// If using a known corner didn't work, use the slow PIP test
		if (!found_known_corner) {
			interior = point_in_polygon(v[poly].x, v[poly].y, poly2);
			//printf("  pip:%i\n",int(interior));
			if (bool_SUB) interior = !interior;
		}
		// One way or another, (bool)interior is good now.
		int vert = cell;
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
			vert = v[vert].next;
		} while (vert && vert!=cell);
	}
	return ret;
}

void GridMesh::trim_to_odd() {
	for (int i=0; i<nx; i++) {
		for (int j=0; j<ny; j++) {
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
	for (int poly=poly0; poly; poly=v[poly].next_poly) {
		trace_origins.push_back(poly);
		while (trace_origins.size()) {
			trace.clear();
			int vert = *trace_origins.rbegin(); trace_origins.pop_back();
			GreinerV2f *vvert = &v[vert];
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
			printf("Poly %i.%i: ",poly,this_trace_poly);
			for (int v : trace) {printf(",%i",v);}
			puts("");
			
			if (trace.size()) {
				// Link the vertices
				// Handle the case of an initial/final double vertex specially
				int first_neighbor=v[*trace.begin()].neighbor, lastv=*trace.rbegin();
				if (first_neighbor==lastv)
					trace.pop_back();
				// Link all but the endpoints, skipping doubles
				for (int i=1,l=int(trace.size()); i<l; i++) {
					int left=trace[i-1], right=trace[i];
					if (v[left].neighbor==right && i+1<l) {
						i += 1;
						int rright = trace[i];
						v[left].next = rright;
						v[rright].prev = left;
					} else {
						v[left].next = right;
						v[right].prev = left;
					}
				}
				for (int i : trace) {
					v[i].first = trace[0];
				}
				// Link the endpoints. Doubles skipped via pop_back
				int first=trace[0], last=trace[trace.size()-1];
				v[first].prev = last;
				v[last].next = first;
				// Hook up the backbone
				if (!previous_trace_poly) {
					v[poly0].next_poly = this_trace_poly;
				} else {
					if (previous_trace_poly==this_trace_poly) printf("Poly-list assembly failed.");
					v[previous_trace_poly].next_poly = this_trace_poly;
				}
				v[this_trace_poly].next_poly = 0;
				previous_trace_poly = this_trace_poly;
			}
		}
	}
}


void GridMesh::label_interior_freepoly(int poly) {
	float x=v[poly].x, y=v[poly].y;
	int over_poly = poly_for_cell(x,y);
	std::set<int> inside; // The set of polygons we are currently inside
	for (int p=over_poly; p; p=v[p].next_poly) {
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
		double ax=v[e1].x, ay=v[e1].y;
		double bx=v[v[e1].next].x, by=v[v[e1].next].y;
		double cx=v[e2].x, cy=v[e2].y;
		double dx=v[v[e2].next].x, dy=v[v[e2].next].y;
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
	double det = a11*a22-a12*a21; //~=0 iff colinear
	double idet = 1/det;
	double b1 = cx-ax;
	double b2 = cy-ay;
	double x1 = (+b1*a22 -b2*a12)*idet; // alpha1
	double x2 = (-b1*a21 +b2*a11)*idet; // alpha2
	*ix = (1.0-x1)*ax + x1*bx;
	*iy = (1.0-x1)*ay + x1*by;
	*alpha1 = x1;
	if (debug) {
		double ix2 = (1.0-x2)*cx + x2*dx;
		double iy2 = (1.0-x2)*cy + x2*dy;
		if (fabs(*ix-ix2)>.001) printf("Bug detected in intersection math.\n");
		if (fabs(*iy-iy2)>.001) printf("Bug detected in intersection math.\n");
	}
	return true;
}

// 1   0
//   v
// 2   3
inline int quadrant(float x, float y, float vx, float vy) {
	if (y>vy) { // Upper half-plane is easy
		return int(x<=vx);
	} else {
		if (y<vy) { // So is lower half-plane
			return 2+int(x>=vx);
		} else { //y==0
			if (x>vx) return 0;
			else if (x<vx) return 0;
			return 99; // x==vx, y==vy
		}
	}
}

bool GridMesh::point_in_polygon(double x, double y, int poly) {
	bool contains_boundary = true;
	float last_x=v[poly].x, last_y=v[poly].y;
	int last_quadrant = quadrant(last_x,last_y,x,y);
	if (last_quadrant==99) return contains_boundary;
	int ccw = 0; // Number of counter-clockwise quarter turns around pt
	for (int vert=v[poly].next; vert; vert=v[vert].next) {
		float next_x=v[vert].x, next_y=v[vert].y;
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