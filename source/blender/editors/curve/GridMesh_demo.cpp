#include <GLUT/glut.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>

bool debug = true;
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
#include "GridMesh.h"

/***************************** DEFAULT SCENE *****************************/
GridMesh *gm;
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {2.360000,8.648000, 3.176000,5.888000, 0.200000,5.000000, 1.304000,0.416000, 4.208000,2.096000, 8.600000,0.536000, 9.632000,4.736000, 5.960000,5.936000, 6.896000,8.696000};
std::vector<float> subj0 = {2.816000,4.784000, 5.024000,5.000000, 5.672000,4.808001, 5.720000,4.160000, 5.384000,3.248000, 4.496000,2.840000, 4.832000,2.456000, 5.696000,3.056000, 6.104000,3.896000, 6.104000,5.024000, 5.072000,5.672000};
std::vector<float> subj1 = {3.992000,3.344000, 5.048000,3.320000, 5.264000,3.896000, 5.120000,4.160000, 4.832000,4.232000, 4.472000,4.256001, 4.160000,4.256001, 3.896000,4.160000};
std::vector<std::vector<float>> subj_polys = {subj0,subj1};
std::vector<float> inout_pts = {};

int clip = 0; // Vertex index of the first vertex of the clip polygon
int subj = 0;

int win_width = 500;
int win_height = 500;

void glut_coords_2_scene(float gx, float gy, float* sx, float* sy) {
	gx /= win_width;
	gy /= win_height;
	*sx =-1.0*(1-gx) + 11.0*gx;
	*sy =11.0*(1-gy) + -1.0*gy;
}

void init_default_scene() {
	// Create the gridmesh
	gm = new GridMesh(0,0,10,10,11,11);
	// Import the clip polygon into the linked-list datastructure
	int last = 0;
	size_t clip_n = clip_verts.size()/2;
	for (int i=0; i<clip_n; i++) {
		int v = gm->vert_new(last,0);
		if (!clip) clip = v;
		gm->v[v].first = clip;
		gm->v[v].x = clip_verts[2*i+0];
		gm->v[v].y = clip_verts[2*i+1];
		last = v;
	}
	if (clip_cyclic) {
		gm->v[clip].prev = last;
		gm->v[last].next = clip;
	}
	// Import the subject polygons into the linked list datastructure
	GreinerV2f *v = gm->v;
	last = 0;
	for (std::vector<float> poly_verts : subj_polys) {
		// Different subject polygons are stored in
		// subj, subj->nextPoly, subj->nextPoly->nextPoly etc
		int newpoly_first_vert = gm->vert_new();
		v[newpoly_first_vert].first = newpoly_first_vert;
		if (!subj) {
			subj = newpoly_first_vert;
		} else {
			v[last].next_poly = newpoly_first_vert;
		}
		last = newpoly_first_vert;
		// Fill in the vertices of the polygon we just finished hooking up
		// to the polygon list
		int last_inner = 0;
		for (size_t i=0,l=poly_verts.size()/2; i<l; i++) {
			int vert;
			if (i==0) {
				vert = newpoly_first_vert;
			} else {
				vert = gm->vert_new();
			}
			v[vert].x = poly_verts[2*i+0];
			v[vert].y = poly_verts[2*i+1];
			v[vert].prev = last_inner;
			v[vert].first = last;
			if (last_inner) v[last_inner].next = vert;
			last_inner = vert;
		}
		gm->poly_set_cyclic(newpoly_first_vert, subj_cyclic);
	}
}

void GLUT_init(){
    glClearColor(0,0,0,0);
    glMatrixMode(GL_PROJECTION);
	// Defines the view box
	// left,right,bottom,top
    gluOrtho2D(-1,11,-1,11);
	init_default_scene();
}


/***************************** DRAW *****************************/
void GLUT_display(){
	float contraction = .04; // Move polygon edges and verts closer to their center
	GreinerV2f *v = gm->v;
	glClear(GL_COLOR_BUFFER_BIT);
	// Draw Clip polygon lines
	glLineWidth(1);
	glBegin(GL_LINES);
	glColor3f(.8,0,0);
	float last_x=v[clip].x, last_y=v[clip].y;
	for (int vert=v[clip].next; vert; vert=v[vert].next) {
		float x=v[vert].x, y=v[vert].y;
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
		float x=v[vert].x, y=v[vert].y;
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
		last_x=v[curpoly].x, last_y=v[curpoly].y;
		for (int vert=v[curpoly].next; vert; vert=v[vert].next) {
			float x=v[vert].x, y=v[vert].y;
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
		last_x=v[curpoly].x, last_y=v[curpoly].y;
		for (int vert=v[curpoly].next; vert; vert=v[vert].next) {
			float x=v[vert].x, y=v[vert].y;
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
			gm->poly_draw(gm->poly_for_cell(i,j), contraction);
		}
	}
	
	// Draw inclusion/exclusion test points
	if (clip_cyclic) {
		glPointSize(5);
		glBegin(GL_POINTS);
		for (size_t i=0,l=inout_pts.size()/2; i<l; i++) {
			float x=inout_pts[i*2+0], y=inout_pts[i*2+1];
			bool pip = gm->point_in_polygon(x,y,clip);
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
	
	
	glFlush();
}

/***************************** INTERACTION *****************************/
int grabbed_vert = 0;

void GLUT_reshape(int w, int h){
	glViewport(0,0,w,h);
	win_width = w;
	win_height = h;
	if (debug){
		printf("GLUT_reshape w:%d h:%d\n",w,h);
	}
}
void dump_polys_to_stdout() {
	GreinerV2f *v = gm->v;
	printf("bool clip_cyclic = %s; // Required for initialization\n",clip_cyclic?"true":"false");
	printf("bool subj_cyclic = %s;\n",subj_cyclic?"true":"false");
	printf("std::vector<float> clip_verts = {");
	for (int vert=clip; vert; vert=v[vert].next) {
		printf((v[vert].next&&v[vert].next!=clip)?"%f,%f, ":"%f,%f};\n",v[vert].x,v[vert].y);
		if (v[vert].next==clip) break;
	}
	int subj_poly_num = 0;
	for (int subj_poly=subj; subj_poly; subj_poly=v[subj_poly].next_poly) {
		printf("std::vector<float> subj%i = {", subj_poly_num);
		for (int vert=subj_poly; vert; vert=v[vert].next) {
			bool is_last_vert = !v[vert].next || v[vert].next==subj_poly;
			printf((!is_last_vert)?"%f,%f, ":"%f,%f};\n",v[vert].x,v[vert].y);
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
	if (ch=='i') {
		gm->insert_vert_poly_gridmesh(clip);
		for (int poly=subj; poly; poly=gm->v[poly].next_poly) {
			gm->insert_vert_poly_gridmesh(poly);
		}
		glutPostRedisplay();
	}
	if (ch=='t') {
		gm->label_interior(clip);
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
}
void create_new_poly(float sx, float sy) {
	GreinerV2f *v = gm->v;
	int last_backbone = subj;
	while (v[last_backbone].next_poly) last_backbone = v[last_backbone].next_poly;
	int newpoly = gm->vert_new();
	v[newpoly].x = sx; v[newpoly].y = sy;
	v[last_backbone].next_poly = newpoly;
	glutPostRedisplay();
}
void create_pt(float sx, float sy) {
	if (!grabbed_vert) return;
	int last_vert = gm->poly_last_vert(grabbed_vert);
	int v = gm->vert_new(last_vert, gm->v[last_vert].next);
	gm->v[v].x = sx;
	gm->v[v].y = sy;
	grabbed_vert = v; // Let's drag the new vert we just made
	glutPostRedisplay();
}
void initiate_pt_drag_if_near_pt(float sx, float sy) {
	GreinerV2f *v = gm->v; // Vertex info array
	float closest_dist = 1e50;
	int closest_vert = 0;
	for (int vert=clip; vert; vert=v[vert].next) {
		float dx = v[vert].x - sx;
		float dy = v[vert].y - sy;
		float dist = sqrt(dx*dx + dy*dy);
		if (dist<closest_dist) {
			closest_dist = dist;
			closest_vert = vert;
		}
		if (v[vert].next==clip) break;
	}
	for (int poly=subj; poly; poly=v[poly].next_poly) {
		for (int vert=poly; vert; vert=v[vert].next) {
			float dx = v[vert].x - sx;
			float dy = v[vert].y - sy;
			float dist = sqrt(dx*dx + dy*dy);
			if (dist<closest_dist) {
				closest_dist = dist;
				closest_vert = vert;
			}
			if (v[vert].next==poly) break;
		}
	}
	if (debug) printf("Nearest point to mousedown (%f)\n",closest_dist);
	grabbed_vert = (closest_dist<.1) ? closest_vert : 0;
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
		gm->v[grabbed_vert].x = sx;
		gm->v[grabbed_vert].y = sy;
		glutPostRedisplay();
	}
}


/***************************** MAIN *****************************/
int main(int argc, char **argv){
    glutInit(& argc, argv);
    glutInitDisplayMode(GLUT_SINGLE|GLUT_RGB);
    glutInitWindowSize(win_width,win_height);
    glutInitWindowPosition(200,200);
    glutCreateWindow("Polygon Split Viz");
    glutDisplayFunc(GLUT_display);
	glutReshapeFunc(GLUT_reshape);
	glutMouseFunc(GLUT_mouse);
	glutMotionFunc(GLUT_motion); // Mouse is dragged. Callback args: (int x, int y)
	glutPassiveMotionFunc(NULL); // Mouse is moved. Callback args: (int x, int y)
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
    return 0;
}