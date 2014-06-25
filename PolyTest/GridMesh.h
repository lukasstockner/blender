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
	int vert_new();
	int vert_new(int prev, int next); // Make a new vert in the middle of an existing poly
	int vert_id(GreinerV2f *vert) {return vert?int(vert-v):0;}
	int vert_neighbor_on_poly(int vert, int poly);
	void vert_add_neighbor(int vert, int new_neighbor);
	int poly_for_cell(int x, int y);
	int poly_for_cell(float x, float y);
	int poly_first_vert(int anyvert);
	int poly_last_vert(int anyvert);
	int poly_next(int anyvert);
	int poly_vert_at(int anyvert, float x, float y);
	int poly_num_edges(int poly);
	bool poly_is_cyclic(int poly);
	void poly_set_cyclic(int poly, bool cyclic);
	
	// Intersection
	bool point_in_polygon(float x, float y, int poly);
	int insert_vert_poly_gridmesh(int poly); // Returns # of vertices inserted.
	int insert_vert_edge_poly(int e, int p); // Returns # of vertices inserted.
	int insert_vert_if_line_line(int e1, int e2); // Returns # of vertices inserted.
	int insert_vert_line_line(int poly1left,
							  int poly1right,
							  float alpha1,
							  int poly2left,
							  int poly2right,
							  float alpha2
							  );
	void find_cell_line_intersections(double x0, double y0, double x1, double y1,
									  std::vector<std::pair<int,int>> *bottom_edges,
									  std::vector<std::pair<int,int>> *left_edges,
									  std::vector<std::pair<int,int>> *integer_cells);
#if defined(ENABLE_GLUT_DEMO)
	// Draw
	void poly_center(int poly, float *cx, float *cy);
	void poly_draw(int poly, float shrinkby);
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
