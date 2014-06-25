//
//  GridMesh.h
//  PolyTest
//
//  Created by Jonathan deWerd on 6/20/14.
//  Copyright (c) 2014 a.b.c. All rights reserved.
//

#ifndef __PolyTest__GridMesh__
#define __PolyTest__GridMesh__

#define ENABLE_GLUT_DEMO

#include <iostream>
#include <vector>
#if defined(ENABLE_GLUT_DEMO)
#include <GLUT/glut.h>
#endif

struct GreinerV2f {
	float x,y;
	int first, prev, next; // First,prev,next verts in the *same* polygon
	int next_poly;   // First vertex of the *next* polygon
	float alpha; // If this vertex came from an affine comb, this is the mixing factor
	bool is_intersection; // True if this vertex was added at an intersection
	bool is_interior;
	bool is_trivial; // True if this polygon has four vertices corresponding precisely to its cell bounds
	int neighbor; // Corresp. vertex at same {x,y} in different polygon
	
	GreinerV2f() :	next(0), prev(0),
					next_poly(0), neighbor(0), first(0),
					is_intersection(false) {};
};

struct GridMesh {
	static float tolerance;
	// Vertex storage. Example: "int prev" in a GreinerV2f refers to v[prev].
	// v[0] is defined to be invalid and filled with the telltale location (-1234,-1234)
	GreinerV2f *v;
	long v_capacity;
	long v_count; // Includes the "bad" vertex #0
	double llx, lly, urx, ury; // Coordinates of lower left and upper right grid corners
	double dx, dy; // Width of a cell in the x, y directions
	double inv_dx, inv_dy; // 1/(width of a cell), 1/(height of a cell)
	int nx, ny; // Number of cells in the x and y directions
	
	GridMesh(double lowerleft_x, double lowerleft_y,
			 double upperright_x, double upperright_y,
			 int num_x_cells, int num_y_cells);
	void set_ll_ur(double lowerleft_x, double lowerleft_y,
				   double upperright_x, double upperright_y);
	
	// Basic vertex and polygon manipulation
	GreinerV2f *vert_new();
	GreinerV2f *vert_new(GreinerV2f *prev, GreinerV2f *next); // Make a new vert in the middle of an existing poly
	int vert_id(GreinerV2f *vert) {return vert?int(vert-v):0;}
	GreinerV2f *vert_neighbor_on_poly(GreinerV2f *vert, GreinerV2f *poly);
	void vert_add_neighbor(GreinerV2f *vert, GreinerV2f *new_neighbor);
	GreinerV2f *poly_for_cell(int x, int y);
	GreinerV2f *poly_for_cell(float x, float y);
	GreinerV2f *poly_first_vert(GreinerV2f *anyvert);
	GreinerV2f *poly_last_vert(GreinerV2f *anyvert);
	GreinerV2f *poly_next(GreinerV2f *anyvert);
	GreinerV2f *poly_vert_at(GreinerV2f *anyvert, float x, float y);
	bool poly_is_cyclic(GreinerV2f *poly);
	void poly_set_cyclic(GreinerV2f *poly, bool cyclic);
	int poly_num_edges(GreinerV2f *poly);
	
	// Intersection
	bool point_in_polygon(float x, float y, GreinerV2f* poly);
	int insert_vert_poly_gridmesh(GreinerV2f *poly);
	int insert_vert_edge_poly(GreinerV2f *e, GreinerV2f *p);
	int insert_vert_if_line_line(GreinerV2f *e1, GreinerV2f *e2);
	GreinerV2f* insert_vert_line_line(GreinerV2f* poly1left,
									  GreinerV2f* poly1right,
									  float alpha1,
									  GreinerV2f* poly2left,
									  GreinerV2f* poly2right,
									  float alpha2
									  );
	void find_cell_line_intersections(double x0, double y0, double x1, double y1,
									  std::vector<std::pair<int,int>> *bottom_edges,
									  std::vector<std::pair<int,int>> *left_edges,
									  std::vector<std::pair<int,int>> *integer_cells);
#if defined(ENABLE_GLUT_DEMO)
	// Draw
	void poly_center(GreinerV2f *poly, float *cx, float *cy);
	void poly_draw(GreinerV2f *poly, float shrinkby);
#endif
};

// Backend
void find_integer_cell_line_intersections(double x0, double y0, double x1, double y1,
										  std::vector<std::pair<int,int>> *bottom_edges,
										  std::vector<std::pair<int,int>> *left_edges,
										  std::vector<std::pair<int,int>> *integer_cells);

bool get_line_seg_intersection(GreinerV2f* poly1left,
							   GreinerV2f* poly1right,
							   float* alpha1,
							   GreinerV2f* poly2left,
							   GreinerV2f* poly2right,
							   float* alpha2
							   );


#endif
