#include <GLUT/glut.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>


bool debug = false;
float intersect_check_tol = .001; //Maximum Euclidean dist between intersect pts

// GLUT coords. Format: (x,y)
// (1,1)(w,1)   |----->
//              |
// (1,h)(w,h)   v

// GL coords. Format: (x,y) Specified via gluOrtho2D
// (-1,1)   (1,1)  ^
//                 |
// (-1,-1)  (1,-1) |------>


/***************************** MATH *****************************/
#include "surf_gridmesh.h"

/***************************** DEFAULT SCENE *****************************/
GridMesh *gm;
int max_drawn_edges=0; // Number of edges to draw per poly (for figuring out order). 0 disables.
#define GRIDMESH_GEOM_TEST_6
//#define RASTER_VIZ

#if defined(GRIDMESH_GEOM_TEST_1)
// Look for GRIDMESH_GEOM_TEST_1.pdf for a vertex id map
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {.2,.2,  1.8,.2,  1.8,1.8,  .2,1.8};
std::vector<float> subj0 = {.8,.8,  1.2,.8,  1.2,1.2,  .8,1.2};
std::vector<std::vector<float>> subj_polys = {subj0};
std::vector<float> inout_pts = {};
float gm_llx=0,gm_lly=0,gm_urx=2,gm_ury=2; // GridMesh params
int gm_nx=2, gm_ny=2;
#endif

#if defined(GRIDMESH_GEOM_TEST_2)
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {.2,.2,  1.8,.2,  1.8,1.8,  .2,1.8};
std::vector<float> subj0 = {.8,.7,  1.2,.7,  1.2,.9,  .8,.9};
std::vector<std::vector<float>> subj_polys = {subj0};
std::vector<float> inout_pts = {};
float gm_llx=0,gm_lly=0,gm_urx=2,gm_ury=2; // GridMesh params
int gm_nx=2, gm_ny=2;
#endif

#if defined(GRIDMESH_GEOM_TEST_3)
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {0.200000,0.200000, 4.436000,-0.268000, 4.460000,3.356000, 0.284000,4.292000};
std::vector<float> subj0 = {0.800000,0.800000, 1.200000,0.800000, 1.200000,1.200000, 0.800000,1.200000};
std::vector<std::vector<float>> subj_polys = {subj0};
std::vector<float> inout_pts = {};
float gm_llx=0,gm_lly=0,gm_urx=4,gm_ury=4; // GridMesh params
int gm_nx=3, gm_ny=3;
#endif

#if defined(GRIDMESH_GEOM_TEST_4)
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {0.201000,0.201000, 4.436000,-0.268000, 4.460000,3.356000, 0.284000,4.292000};
std::vector<std::vector<float>> subj_polys = {};
std::vector<float> inout_pts = {};
float gm_llx=0,gm_lly=0,gm_urx=1,gm_ury=1; // GridMesh params
int gm_nx=4, gm_ny=4;
#endif

#if defined(GRIDMESH_GEOM_TEST_5)
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {0.201000,0.201000, 4.436000,-0.268000, 4.460000,3.356000, 0.284000,4.292000};
std::vector<float> subj0 = {0.512000,0.938000, 0.374000,0.950000, 0.248000,0.908000, 0.170000,0.866000, 0.092000,0.740000, 0.092000,0.602000, 0.092000,0.440000, 0.116000,0.260000, 0.254000,0.110000, 0.476000,0.074000, 0.746000,0.092000, 0.836000,0.206000, 0.848000,0.422000, 0.812000,0.644000, 0.716000,0.686000, 0.614000,0.734000, 0.488000,0.728000, 0.386000,0.710000, 0.260000,0.626000, 0.272000,0.476000, 0.350000,0.338000, 0.482000,0.278000, 0.632000,0.308000, 0.644000,0.404000, 0.638000,0.494000, 0.590000,0.572000, 0.494000,0.584000, 0.422000,0.518000, 0.458000,0.392000, 0.548000,0.398000, 0.506000,0.506000, 0.572000,0.506000, 0.596000,0.386000, 0.566000,0.338000, 0.470000,0.338000, 0.368000,0.434000, 0.374000,0.608000, 0.578000,0.656000, 0.680000,0.644000, 0.740000,0.554000, 0.782000,0.308000, 0.758000,0.224000, 0.548000,0.164000, 0.338000,0.224000, 0.212000,0.374000, 0.170000,0.626000, 0.236000,0.764000, 0.368000,0.824000, 0.524000,0.836000, 1.184000,0.848000};
std::vector<std::vector<float>> subj_polys = {subj0};
std::vector<float> inout_pts = {};
float gm_llx=0,gm_lly=0,gm_urx=1,gm_ury=1; // GridMesh params
int gm_nx=4, gm_ny=4;
#endif

#if defined(GRIDMESH_GEOM_TEST_6)
std::vector<float> clip_verts = {.2,.2,  1.8,.2,  1.8,1.8,  .2,1.8};
std::vector<float> subj0 = {25.882751,5.770961, 25.278633,5.331344, 24.569326,4.988698, 23.765833,4.740355, 22.882767,4.594546, 21.951714,4.563889, 21.047365,4.651808, 20.202778,4.849863, 19.430487,5.155091, 18.765156,5.559959, 18.260668,6.029275, 17.921791,6.543593, 17.753057,7.083594, 17.755810,7.631595, 17.929544,8.170758, 18.273809,8.684216, 18.768650,9.140267, 19.385557,9.520200, 20.122162,9.822577, 20.963339,10.031810, 21.880264,10.131394, 22.793755,10.111599, 23.670691,9.979074, 24.484474,9.741599, 25.226538,9.396900, 25.848886,8.957220, 26.300838,8.459513, 26.577435,7.922859, 26.673948,7.366679, 26.589247,6.810863, 26.324949,6.273124, 25.882751,5.770961};
std::vector<std::vector<float>> subj_polys = {subj0};
float gm_llx=0.000000,gm_lly=0.000000,gm_urx=50.305569,gm_ury=13.931460; // GridMesh params
int gm_nx=20, gm_ny=10;
std::vector<float> inout_pts = {};
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
#endif


#if defined(GRIDMESH_TIME_TEST)
// Use this for timing runs
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {0.201000,0.201000, 4.436000,-0.268000, 4.460000,3.356000, 0.284000,4.292000};
std::vector<float> subj0 = {1.016000,2.708000, 2.084000,0.848000, 3.272000,2.756000};
std::vector<float> subj1 = {0.908000,2.780000, 1.568000,3.020000, 3.356000,3.068000, 3.152000,0.932000, 1.424000,0.488000};
std::vector<float> subj2 = {0.752000,2.864000, 1.244000,3.200000, 1.988000,3.272000, 3.128000,3.164000, 3.752000,2.888000, 3.632000,1.928000, 3.428000,0.800000, 2.648000,0.392000, 1.316000,0.308000, 0.740000,1.172000};
std::vector<float> subj3 = {0.416000,2.360000, 0.524000,2.960000, 0.908000,3.500000, 1.448000,3.596000, 1.976000,3.572000, 2.360000,3.560000, 2.744000,3.500000, 3.116000,3.392000, 3.560000,3.248000, 3.800000,3.020000, 3.848000,2.672000, 3.836000,1.952000, 3.764000,1.376000, 3.668000,0.848000, 3.440000,0.380000, 2.912000,0.260000, 2.324000,0.212000, 1.856000,0.152000, 1.340000,0.188000, 1.028000,0.260000, 0.764000,0.344000, 0.560000,0.680000, 0.476000,0.872000, 0.308000,1.172000, 0.164000,1.472000, 0.140000,1.688000, 0.344000,1.772000, 0.524000,1.928000, 0.320000,2.036000, 0.548000,2.204000};
std::vector<float> subj4 = {0.368000,3.392000, 0.332000,3.128000, 0.332000,2.996000, 0.344000,2.756000, 0.344000,2.588000, 0.332000,2.384000, 0.320000,2.228000, 0.296000,2.024000, 0.296000,1.772000, 0.296000,1.508000, 0.284000,1.220000, 0.284000,0.968000, 0.308000,0.704000, 0.416000,0.512000, 0.620000,0.440000, 0.776000,0.536000, 0.872000,0.752000, 0.908000,1.040000, 0.932000,1.364000, 0.956000,1.712000, 0.968000,1.964000, 0.992000,2.312000, 1.016000,2.612000, 1.076000,2.876000, 1.196000,3.056000, 1.400000,3.128000, 1.640000,2.972000, 1.688000,2.852000, 1.760000,2.432000, 1.808000,2.000000, 1.856000,1.556000, 1.868000,1.148000, 1.880000,0.752000, 1.952000,0.452000, 2.204000,0.176000, 2.696000,0.200000, 3.128000,0.452000, 3.248000,0.800000, 3.308000,1.028000, 3.332000,1.400000, 3.356000,1.592000, 3.428000,1.856000, 3.488000,2.120000, 3.536000,2.360000, 3.572000,2.648000, 3.596000,2.936000, 3.548000,3.212000, 3.332000,3.380000, 3.164000,3.164000, 3.140000,2.756000, 3.140000,2.516000, 3.140000,2.288000, 3.104000,1.820000, 3.044000,1.568000, 2.924000,1.184000, 2.732000,0.884000, 2.504000,0.764000, 2.348000,0.872000, 2.324000,1.208000, 2.252000,1.652000, 2.240000,1.880000, 2.216000,2.228000, 2.168000,2.420000, 2.108000,2.756000, 2.060000,2.960000, 1.964000,3.188000, 1.808000,3.320000, 1.364000,3.380000, 1.136000,3.296000, 0.896000,2.972000, 0.764000,2.624000, 0.752000,2.348000, 0.728000,1.976000, 0.728000,1.844000, 0.764000,1.496000, 0.740000,1.196000, 0.740000,0.980000, 0.704000,0.740000, 0.572000,0.644000, 0.476000,0.800000, 0.464000,0.932000, 0.464000,1.172000, 0.500000,1.412000, 0.500000,1.616000, 0.596000,2.060000, 0.584000,2.480000, 0.644000,3.368000, 0.740000,3.644000, 1.112000,3.764000, 1.364000,3.764000, 1.664000,3.740000, 1.880000,3.728000, 2.276000,3.728000, 2.660000,3.716000, 2.912000,3.812000, 2.264000,3.800000, 1.568000,3.836000, 1.088000,3.836000, 0.776000,3.836000, 0.416000,3.836000};
std::vector<std::vector<float>> subj_polys = {subj0,subj1,subj2,subj3,subj4};
std::vector<float> inout_pts = {};
float gm_llx=0,gm_lly=0,gm_urx=4,gm_ury=4; // GridMesh params
int gm_nx=20, gm_ny=30;
static FILE *time_noclip;
static FILE *time_subj[100];
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

int clip = 0; // Vertex index of the first vertex of the clip polygon
int subj = 0;

int win_width = 500;
int win_height = 500;

void glut_coords_2_scene(float gx, float gy, float* sx, float* sy) {
	gx /= win_width;
	gy /= win_height;
	*sx =(gm_llx-1)*(1-gx) + (gm_urx+1)*gx;
	*sy =(gm_ury+1)*(1-gy) + (gm_lly-1)*gy;
}

void init_default_scene() {
	// Create the gridmesh
	gm = new GridMesh();
	gm->set_ll_ur(gm_llx,gm_lly,gm_urx,gm_ury);
	gm->init_grid(gm_nx,gm_ny);
	// Import the clip polygon into the linked-list datastructure
	clip = gm->poly_new(&clip_verts[0], int(clip_verts.size()));
	// Import the subject polygons into the linked list datastructure
	int last = 0;
	for (std::vector<float> poly_verts : subj_polys) {
		int np = gm->poly_new(&poly_verts[0], int(poly_verts.size()));
		if (last)
			gm->v[last].next_poly = np;
		else
			subj = np;
		last = np;
	}
}

void GLUT_init(){
    glClearColor(0,0,0,0);
    glMatrixMode(GL_PROJECTION);
	// Defines the view box
	// left,right,bottom,top
    gluOrtho2D(gm_llx-1,gm_urx+1,gm_lly-1,gm_ury+1);
	init_default_scene();
}


/***************************** DRAW *****************************/
void GLUT_display(){
	float contraction = .04; // Move polygon edges and verts closer to their center
	std::vector<GridMeshVert>& v = gm->v;
	glClear(GL_COLOR_BUFFER_BIT);
	// Draw Clip polygon lines
	glLineWidth(1);
	glBegin(GL_LINES);
	glColor3f(.8,0,0);
	double xyz[3]; gm->vert_get_coord(clip, xyz);
	float last_x=xyz[0], last_y=xyz[1];
	for (int vert=v[clip].next; vert; vert=v[vert].next) {
		gm->vert_get_coord(vert, xyz);
		float x=xyz[0], y=xyz[1];
		if (v[vert].is_intersection) {
			float cx, cy;
			gm->poly_center(v[v[vert].neighbor].first, &cx, &cy);
			x = (1.0-contraction)*x + contraction*cx;
			y = (1.0-contraction)*y + contraction*cy;
		}
		glVertex2f(last_x,last_y);
		glVertex2f(x,y);
		last_x=x; last_y=y;
		if (vert==clip) break;
	}
	glEnd();
	
	//Draw Clip polygon verts
	glPointSize(5);
	glBegin(GL_POINTS);
	glColor3f(1,0,0);
	bool first_iter = true;
	for (int vert=clip; vert; vert=v[vert].next) {
		gm->vert_get_coord(vert, xyz);
		float x=xyz[0], y=xyz[1];
		if (v[vert].is_intersection) {
			float cx, cy;
			gm->poly_center(v[v[vert].neighbor].first, &cx, &cy);
			x = (1.0-contraction)*x + contraction*cx;
			y = (1.0-contraction)*y + contraction*cy;
		}
		glVertex2f(x,y);
		if (!first_iter && vert==clip) break;
		first_iter = false;
	}
	glEnd();
	
	// Draw Subject polygon lines
	glBegin(GL_LINES);
	for (int curpoly=subj; curpoly; curpoly=v[curpoly].next_poly) {
		gm->vert_get_coord(curpoly, xyz);
		float last_x=xyz[0], last_y=xyz[1];
		for (int vert=v[curpoly].next; vert; vert=v[vert].next) {
			gm->vert_get_coord(vert, xyz);
			float x=xyz[0], y=xyz[1];
			glColor3f(0,.8,0);
			glVertex2f(last_x,last_y);
			glVertex2f(x,y);
			last_x=x; last_y=y;
			if (vert==curpoly) break;
		}
	}
	glEnd();
	
	// Draw Subject polygon verts
	glPointSize(3);
	glBegin(GL_POINTS);
	glColor3f(0,1,0);
	for (int curpoly=subj; curpoly; curpoly=v[curpoly].next_poly) {
		gm->vert_get_coord(curpoly, xyz);
		float last_x=xyz[0], last_y=xyz[1];
		for (int vert=v[curpoly].next; vert; vert=v[vert].next) {
			gm->vert_get_coord(vert, xyz);
			float x=xyz[0], y=xyz[1];
			glColor3f(0,.8,0);
			glVertex2f(x,y);
			last_x=x; last_y=y;
			if (vert==curpoly) break;
		}
	}
	glEnd();
	
	// Draw Grid polygon lines & verts
	for (int i=0; i<gm->nx; i++) {
		for (int j=0; j<gm->ny; j++) {
			gm->poly_draw(gm->poly_for_cell(i,j), contraction, max_drawn_edges);
		}
	}
	
	// Draw inclusion/exclusion test points
	if (clip_cyclic) {
		glPointSize(5);
		glBegin(GL_POINTS);
		for (size_t i=0,l=inout_pts.size()/2; i<l; i++) {
			float x=inout_pts[i*2+0], y=inout_pts[i*2+1];
			bool pip = gm->point_in_polygon(x,y,1);
			if (pip) glColor3f(1,1,0);
			else glColor3f(0, 0, 1);
			glVertex2f(x,y);
		}
		glEnd();
	}
	
	// Vestigal grid variables
	float xo = 1*(12.0/win_width);
	float yo = 1*(12.0/win_height);
	xo=0; yo=0;

#if defined(RASTER_VIZ)
	//Draw purple grid boxes on cells intersected by subj's first line segment
	glColor3f(.5, 0, .5);
	std::vector<std::pair<int,int>> bottom_edges, left_edges, integer_cells;
	gm->find_cell_line_intersections(v[clip].x, v[clip].y,
									 v[v[clip].next].x, v[v[clip].next].y,
									 &bottom_edges, &left_edges, &integer_cells);
	glPointSize(10);
	glBegin(GL_POINTS);
	for (std::pair<int,int> xy : integer_cells) {
		float x=gm->llx + xy.first*gm->dx;
		float y=gm->lly + xy.second*gm->dy;
		glVertex2f(x+0.5*gm->dx, y+0.5*gm->dy);
	}
	glEnd();
	
	//Draw magenta lines on cell edges intersected by subj's first line segment
	glLineWidth(2);
	glBegin(GL_LINES);
	glColor3f(1, 0, 0);
	xo=0; yo=0;
	for (std::pair<int,int> xy : bottom_edges) {
		float x=gm->llx + xy.first*gm->dx;
		float y=gm->lly + xy.second*gm->dy;
		glVertex2f(x+xo,y+yo); glVertex2f(x+gm->dx-xo,y+yo);
	}
	for (std::pair<int,int> xy : left_edges) {
		float x=gm->llx + xy.first*gm->dx;
		float y=gm->lly + xy.second*gm->dy;
		glVertex2f(x+xo,y+gm->dy-yo); glVertex2f(x+xo,y+yo);
	}
	glEnd();
#endif
	
	glFlush();
}

/***************************** INTERACTION *****************************/
int grabbed_vert = 0;

void GLUT_reshape(int w, int h){
	glViewport(0,0,w,h);
	win_width = w;
	win_height = h;
}
void dump_polys_to_stdout() {
	GridMeshVert *v = &gm->v[0];
	printf("bool clip_cyclic = %s; // Required for initialization\n",clip_cyclic?"true":"false");
	printf("bool subj_cyclic = %s;\n",subj_cyclic?"true":"false");
	printf("std::vector<float> clip_verts = {");
	for (int vert=clip; vert; vert=v[vert].next) {
		double xyz[3]; gm->vert_get_coord(vert, xyz);
		printf((v[vert].next&&v[vert].next!=clip)?"%f,%f, ":"%f,%f};\n",xyz[0],xyz[1]);
		if (v[vert].next==clip) break;
	}
	int subj_poly_num = 0;
	for (int subj_poly=subj; subj_poly; subj_poly=v[subj_poly].next_poly) {
		printf("std::vector<float> subj%i = {", subj_poly_num);
		for (int vert=subj_poly; vert; vert=v[vert].next) {
			bool is_last_vert = !v[vert].next || v[vert].next==subj_poly;
			double xyz[3]; gm->vert_get_coord(vert, xyz);
			printf((!is_last_vert)?"%f,%f, ":"%f,%f};\n",xyz[0],xyz[1]);
			if (is_last_vert) break;
		}
		subj_poly_num++;
	}
	printf("std::vector<std::vector<float>> subj_polys = {");
	for (int i=0; i<subj_poly_num; i++) {
		printf((i!=subj_poly_num-1)?"subj%i,":"subj%i};\n",i);
	}
	printf("std::vector<float> inout_pts = {");
	for (size_t i=0,l=inout_pts.size()/2; i<l; i++) {
		printf((i!=l-1)?"%f,%f, ":"%f,%f",inout_pts[2*i+0],inout_pts[2*i+1]);
	}
	puts("};\n");
}
void toggle_cyclic(int curve) {
	bool iscyc = gm->poly_is_cyclic(curve);
	gm->poly_set_cyclic(curve, !iscyc);
	glutPostRedisplay();
}
void delete_last_selected_vert() {
	// Don't allow #subj verts or #clip verts -> 0
	if (!grabbed_vert || grabbed_vert==clip) return;
	int next = gm->v[grabbed_vert].next;
	int prev = gm->v[grabbed_vert].next;
	gm->v[prev].next = next;
	gm->v[next].prev = prev;
	glutPostRedisplay();
}
#define GLUT_KEY_ESC 27
#define GLUT_KEY_RETURN 13
#define GLUT_KEY_DELETE 127
void GLUT_keyboard(unsigned char ch, int x, int y ) {
	int m = glutGetModifiers();
	if (debug) {
		char m_str[128]; m_str[0]=0;
		if (m&GLUT_ACTIVE_ALT) strcpy(m_str+strlen(m_str),"ALT,");
		if (m&GLUT_ACTIVE_SHIFT) strcpy(m_str+strlen(m_str),"SHIFT,");
		if (m&GLUT_ACTIVE_CTRL) strcpy(m_str+strlen(m_str),"CTRL,");
		printf("GLUT_keyboard x:%d y:%d ch:%i mod:%x(%s)\n",x,y,(int)ch,m,m_str);
	}
	if (ch==GLUT_KEY_ESC) {
		init_default_scene();
		glutPostRedisplay();
	}
	if (ch==GLUT_KEY_RETURN) {
		dump_polys_to_stdout();
	}
	if (clip && ch=='o') {
		gm->bool_AND(clip);
		clip = 0;
		glutPostRedisplay();
	}
	if (subj && ch=='k') {
		gm->bool_SUB(subj);
		subj = gm->v[subj].next_poly; // Subject was destroyed in trimming process
		glutPostRedisplay();
	}
	if (subj && ch=='i') {
		gm->insert_vert_poly_gridmesh(subj);
		glutPostRedisplay();
	}
	if (subj && ch=='l') {
		gm->label_interior_SUB(subj);
		gm->label_interior_freepoly(subj);
		glutPostRedisplay();
	}
	if (subj && ch=='t') {
		gm->trim_to_odd();
		subj = gm->v[subj].next_poly; // Subject was destroyed in trimming process
		glutPostRedisplay();
	}
	if (ch=='1') toggle_cyclic(clip);
	if (ch==GLUT_KEY_DELETE) delete_last_selected_vert();
}
void GLUT_specialkey(int ch, int x, int y) {
	// CTRL, SHIFT, arrows actually work
	// alt/option, command do not
	if (debug) {
		const char* ch_str = "???";
		if (ch==GLUT_KEY_LEFT) ch_str = "GLUT_KEY_LEFT";
		if (ch==GLUT_KEY_RIGHT) ch_str = "GLUT_KEY_RIGHT";
		if (ch==GLUT_KEY_UP) ch_str = "GLUT_KEY_UP";
		if (ch==GLUT_KEY_DOWN) ch_str = "GLUT_KEY_DOWN";
		printf("GLUT_specialkey x:%d y:%d ch:%x(%s)\n",x,y,ch,ch_str);
	}
	if (ch==GLUT_KEY_UP) {
		max_drawn_edges++;
		glutPostRedisplay();
	}
	if (ch==GLUT_KEY_DOWN) {
		max_drawn_edges--;
		glutPostRedisplay();
	}
}
void create_new_poly(float sx, float sy) {
	GridMeshVert *v = &gm->v[0];
	int newpoly = gm->vert_new();
	gm->vert_set_coord(newpoly, sx, sy, 0);
	v[newpoly].first = newpoly;
	v[newpoly].next = newpoly; v[newpoly].prev = newpoly;
	if (subj) {
		int last_backbone = subj;
		while (v[last_backbone].next_poly) last_backbone = v[last_backbone].next_poly;
		v[last_backbone].next_poly = newpoly;
	} else {
		subj = newpoly;
	}
	grabbed_vert = newpoly;
	printf("Added subj vert. subj = ");
	for (int vert=subj; vert; vert=v[vert].next_poly) printf(",%i",vert);
	puts("");
	glutPostRedisplay();
}
void create_pt(float sx, float sy) {
	if (!grabbed_vert) return;
	int last_vert = gm->poly_last_vert(grabbed_vert);
	int v = gm->vert_new(last_vert, gm->v[last_vert].next);
	gm->vert_set_coord(v, sx, sy, 0);
	grabbed_vert = v; // Let's drag the new vert we just made
	glutPostRedisplay();
}
int closest_vert(float sx, float sy, float *dist) {
	GridMeshVert *v = &gm->v[0];
	float closest_dist = 1e50;
	int nearest_v = 0;
	for (int vert=clip; vert; vert=v[vert].next) {
		double xyz[3]; gm->vert_get_coord(vert, xyz);
		float dx = xyz[0] - sx;
		float dy = xyz[1] - sy;
		float dist = sqrt(dx*dx + dy*dy);
		if (dist<closest_dist) {
			closest_dist = dist;
			nearest_v = vert;
		}
		if (v[vert].next==clip) break;
	}
	for (int poly=subj; poly; poly=v[poly].next_poly) {
		for (int vert=poly; vert; vert=v[vert].next) {
			double xyz[3]; gm->vert_get_coord(vert, xyz);
			float dx = xyz[0] - sx;
			float dy = xyz[1] - sy;
			float dist = sqrt(dx*dx + dy*dy);
			if (dist<closest_dist) {
				closest_dist = dist;
				nearest_v = vert;
			}
			if (v[vert].next==poly) break;
		}
	}
	if (dist) *dist = closest_dist;
	return nearest_v;
}
void initiate_pt_drag_if_near_pt(float sx, float sy) {
	float dist;
	int v = closest_vert(sx,sy,&dist);
	grabbed_vert = (dist<.3) ? v : 0;
}
void terminate_pt_drag() {
	//grabbed_vert = nullptr;
}
void GLUT_mouse( int button, int state, int x, int y) {
	float sx,sy;
	glut_coords_2_scene(x,y,&sx,&sy);
	int m = glutGetModifiers();
	if (debug) {
		const char* state_str = "???";
		if (state==GLUT_DOWN) state_str	= "GLUT_DOWN";
		if (state==GLUT_UP) state_str	= "GLUT_UP";
		const char* button_str = "???";
		if (button==GLUT_LEFT_BUTTON) button_str = "GLUT_LEFT_BUTTON";
		if (button==GLUT_MIDDLE_BUTTON) button_str = "GLUT_MIDDLE_BUTTON";
		if (button==GLUT_RIGHT_BUTTON) button_str = "GLUT_RIGHT_BUTTON";
		printf("GLUT_mouse x:%d y:%d button:%x(%s) state:%x(%s)\n",
			   x,y,button,button_str,state,state_str);
	}
	if (state==GLUT_DOWN && button==GLUT_LEFT_BUTTON) {
		if (m&GLUT_ACTIVE_CTRL && m&GLUT_ACTIVE_SHIFT)
			create_new_poly(sx,sy);
		else if (m&GLUT_ACTIVE_CTRL)
			create_pt(sx,sy);
		else
			initiate_pt_drag_if_near_pt(sx,sy);
	}
	if (state==GLUT_DOWN && button==GLUT_RIGHT_BUTTON) {
		inout_pts.push_back(sx);
		inout_pts.push_back(sy);
		glutPostRedisplay();
	}
	if (state==GLUT_UP) {
		terminate_pt_drag();
	}
}
void GLUT_motion(int x, int y) {
	float sx,sy;
	glut_coords_2_scene(x,y,&sx,&sy);
	if (grabbed_vert) {
		gm->vert_set_coord(grabbed_vert, sx, sy, 0);
		glutPostRedisplay();
	}
}
void GLUT_passive(int x, int y) {
	float sx,sy,dist;
	glut_coords_2_scene(x,y,&sx,&sy);
	int vert = closest_vert(sx,sy,&dist);
	if (dist<.1) {
		GridMeshVert &v = gm->v[vert];
		int ie=v.is_entry, ir=v.is_interior, is=v.is_intersection;
		//printf("Hover vert: %i is_entry:%i is_interior:%i is_intersection:%i\n",vert,ie,ir,is);
	}
}

#if defined(GRIDMESH_TIME_TEST)
void time_start() {
	time_noclip = fopen("/tmp/noclip_nurbs.txt","w");
	int subj_count=int(subj_polys.size());
	for (int subj_no=0; subj_no<subj_count; subj_no++) {
		char path[50];
		sprintf(path,"/tmp/subj%i_nurbs.txt",subj_no);
		time_subj[subj_no] = fopen(path,"w");
	}
}
void time_clip() {
	int subj_no=0, subj_count=int(subj_polys.size());
	uint64_t start,stop;
	
	start = mach_absolute_time();
	gm = new GridMesh();
	gm->set_ll_ur(gm_llx,gm_lly,gm_urx,gm_ury);
	gm->init_grid(gm_nx,gm_ny);
	delete gm;
	stop = mach_absolute_time();
	fprintf(time_noclip, "%i %i %llu\n",gm_nx*gm_ny,0,stop-start);

	for (subj_no=0; subj_no<subj_count; subj_no++) {
		start = mach_absolute_time();
		gm = new GridMesh();
		gm->set_ll_ur(gm_llx,gm_lly,gm_urx,gm_ury);
		gm->init_grid(gm_nx,gm_ny);
		std::vector<float>& clipvec=subj_polys[subj_no];
		int clip_poly = gm->poly_new(clipvec[0],clipvec.size());
		int cpverts = int(clipvec.size()/2);
		gm->bool_SUB(clip_poly);
		delete gm;
		stop = mach_absolute_time();
		fprintf(time_subj[subj_no], "%i %i %llu\n",gm_nx*gm_ny,cpverts,stop-start);
	}
}
void time_main() {
	time_start();
	for (int res=32; res<=400; res+=8) {
		gm_nx = res; gm_ny = res;
		time_clip();
	}
}
#endif


/***************************** MAIN *****************************/
void interactive_main(int argc, char**argv) {
	glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE|GLUT_RGB);
    glutInitWindowSize(win_width,win_height);
    glutInitWindowPosition(200,200);
    glutCreateWindow("Polygon Split Viz");
    glutDisplayFunc(GLUT_display);
	glutReshapeFunc(GLUT_reshape);
	glutMouseFunc(GLUT_mouse);
	glutMotionFunc(GLUT_motion); // Mouse is dragged. Callback args: (int x, int y)
	glutPassiveMotionFunc(GLUT_passive); // Mouse is moved. Callback args: (int x, int y)
	glutKeyboardFunc(GLUT_keyboard);
	glutSpecialFunc(GLUT_specialkey);
    GLUT_init();
	puts("Welcome to the polygon demo! This was designed to be a convenient ");
	puts("sandbox for testing polygon trim code designed as a part of blender's ");
	puts(" new NURBS functionality.");
	puts("------ Instructions: ------");
	puts("<LMB down/drag>: move polyline/polygon vertices around");
	puts("<CTRL>+<LMB>: new vertex on last touched polyline");
	puts("<ENTER>: dump vertices in format suitable for copypaste into code");
	puts("<ESC>: reset to default 'scene'");
	puts("i: insert vertices at points of intersection between the two polylines/gons");
	puts("1: toggle clip (red) polyline<->polygon");
	puts("2: toggle subj (green) polyline<->polygon");
	puts("---------------------------");
    glutMainLoop();
}
int main(int argc, char **argv){
#if defined(GRIDMESH_TIME_TEST)
	time_main()
#else
	interactive_main(argc,argv);
#endif
    return 0;
}