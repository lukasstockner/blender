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
#include <algorithm>
#include "GridMesh.h"

static bool debug = 1;
float GridMesh::tolerance = 1e-5;

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
			int iv3 = iv1+2;
			int iv4 = iv1+3;
			v1->next = iv2; v2->prev = iv1; v1->first = iv1; v1->corner = 1;
			v2->next = iv3; v3->prev = iv2; v2->first = iv1; v2->corner = 2;
			v3->next = iv4; v4->prev = iv3; v3->first = iv1; v3->corner = 3;
			v4->next = iv1; v1->prev = iv4; v4->first = iv1; v4->corner = 4;
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
	while (v[v2].next) {
		int next = v[v2].next;
		if (v[v2].first==v2) return v2;
		v2 = next;
	}
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
	} while (vert && vert!=poly);
	*cx = sum_x/n;
	*cy = sum_y/n;
}

struct rgbcolor {unsigned char r,g,b;};
void GridMesh::poly_draw(int poly, float shrinkby) {
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
	// Find the center so that we can shrink towards it
	float cx,cy;
	poly_center(poly, &cx, &cy);
	// Draw the polygon
	glBegin(GL_LINES);
	glColor3ub(color.r, color.g, color.b);
	int v1 = poly;
	do {
		int v2 = v[v1].next;
		glVertex2f((1-shrinkby)*v[v1].x+shrinkby*cx, (1-shrinkby)*v[v1].y+shrinkby*cy);
		glVertex2f((1-shrinkby)*v[v2].x+shrinkby*cx, (1-shrinkby)*v[v2].y+shrinkby*cy);
		v1 = v2;
	} while (v1 != poly);
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
	} while (v1 != poly);
	glEnd();
}
#endif



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
	if (x0>x1) { // Ensure order is left to right
		flipped_left_right = true;
		std::swap(x0,x1);
		std::swap(y0,y1);
	}
	int cx0=xs_FloorToInt(x0), cy0=xs_FloorToInt(y0), cx1=xs_FloorToInt(x1), cy1=xs_FloorToInt(y1);
	// Line segments smaller than a cell's minimum dimension should always hit these trivial cases
	if (cy0==cy1) { //Horizontal or single-cell
		if (integer_cells) {
			for (int i=cx0; i<=cx1; i++)
				integer_cells->push_back(std::make_pair(i,cy0));
		}
		if (left_edges) {
			for (int i=cx0+1; i<=cx1; i++)
				left_edges->push_back(std::make_pair(i,cy0));
		}
	} else if (cx0==cx1) { // Vertical
		if (integer_cells) {
			if (cy0<cy1) {
				for (int i=cy0; i<=cy1; i++)
					integer_cells->push_back(std::make_pair(cx0,i));
			} else {
				for (int i=cy1; i<=cy0; i++)
					integer_cells->push_back(std::make_pair(cx1,i));
			}
		}
		if (bottom_edges) {
			if (cy0<cy1) {
				for (int i=cy0+1; i<=cy1; i++)
					bottom_edges->push_back(std::make_pair(cx0,i));
			}
			else {
				for (int i=cy1+1; i<=cy0; i++)
					bottom_edges->push_back(std::make_pair(cx1,i));
			}
		}
	} else {
		// Line segments that make us think :)
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
	}
	if (flipped_left_right) {
		if (integer_cells) std::reverse(integer_cells->begin(), integer_cells->end());
		if (bottom_edges) std::reverse(bottom_edges->begin(), bottom_edges->end());
		if (left_edges) std::reverse(left_edges->begin(), left_edges->end());
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
	std::vector<std::vector<IntersectingEdge>> intersections;
	mpoly = poly_first_vert(mpoly);
	// Step 1: find all the intersections
	int v1 = mpoly;
	float v1x=v[v1].x, v1y=v[v1].y;
	int verts_added = 0;
	while (v[v1].next) {
		int v2 = v[v1].next;
		float v2x=v[v2].x, v2y=v[v2].y;
		printf("(%f,%f)---line--(%f,%f)\n",v1x,v1y,v2x,v2y);
		integer_cells.clear();
		find_cell_line_intersections(v1x,v1y,v2x,v2y,nullptr,nullptr,&integer_cells);
		std::vector<IntersectingEdge> isect;
		for (size_t i=0,l=integer_cells.size(); i<l; i++) {
			std::pair<int,int> j = integer_cells[i];
			int cell_poly = poly_for_cell(j.first, j.second);
			if (!cell_poly) continue;
			std::vector<IntersectingEdge> isect_tmp = edge_poly_intersections(v1, cell_poly);
			for (IntersectingEdge& e : isect_tmp) {
				printf("(%i,%i)",j.first,j.second);
				e.cellidx = int(i);
			}
			printf("\n");
			isect.insert(isect.end(),isect_tmp.begin(),isect_tmp.end());
		}
		std::stable_sort(isect.begin(),isect.end(),intersection_edge_order);
		intersections.push_back(isect);
		verts_added += isect.size();
		v1=v2; v1x=v2x; v1y=v2y;
		if (v1==mpoly) break;
	}
	// Step 2: insert them
	v1 = mpoly;
	v1x=v[v1].x; v1y=v[v1].y;
	std::vector<std::vector<IntersectingEdge>>::iterator it = intersections.begin();
	while (v[v1].next) {
		int v2 = v[v1].next;
		float v2x=v[v2].x, v2y=v[v2].y;
		integer_cells.clear();
		find_integer_cell_line_intersections(v1x,v1y,v2x,v2y,nullptr,nullptr,&integer_cells);
		std::vector<IntersectingEdge>& isect = *it++;
		for (IntersectingEdge ie : isect) {
			v1 = insert_vert(v1, v2, ie.e2, v[ie.e2].next, ie.x, ie.y);
		}
		v1=v2; v1x=v2x; v1y=v2y;
		if (v1==mpoly) break;
	}
	return verts_added;
}

void GridMesh::label_interior_cell(int x, int y, int poly, bool ll, bool *lr, bool*ul) {
	int cell = poly_for_cell(x,y);
	bool interior = ll;
	bool first_is_interior = interior;
	int vert = cell;
	do {
		v[vert].is_interior = interior;
		if (v[vert].corner==2&&lr) *lr = interior;
		if (v[vert].corner==4&&ul) *ul = interior;
		if (v[vert].is_intersection) interior = !interior;
		vert = v[vert].next;
	} while (vert!=cell);
	if (!interior==first_is_interior&&debug) {
		printf("inconsistent interior call!\n");
	}
}

void GridMesh::label_interior(int poly) {
	bool ll_next_row = point_in_polygon(0, 0, poly);
	for (int j=0; j<ny; j++) {
		bool ll;
		label_interior_cell(0, j, poly, ll_next_row, &ll, &ll_next_row);
		for (int i=1; i<nx; i++) {
			label_interior_cell(i, j, poly, ll, &ll, nullptr);
		}
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