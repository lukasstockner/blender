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
bool clip_cyclic = true; // Required for initialization
std::vector<float> clip_verts = {-0.460000,-0.004000, 0.360000,-0.072000, 0.388000,-0.744000, -0.440000,-0.720000};
GridMesh *gm;
std::vector<float> inout_pts;



GreinerV2f *clip = nullptr;

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
	GreinerV2f *last = nullptr;
	size_t clip_n = clip_verts.size()/2;
	for (int i=0; i<clip_n; i++) {
		GreinerV2f *v = gm->vert_new(last,nullptr);
		if (!clip) clip = v;
		v->first = gm->vert_id(clip);
		v->x = clip_verts[2*i+0];
		v->y = clip_verts[2*i+1];
		last = v;
	}
	if (clip_cyclic) {
		clip->prev = gm->vert_id(last);
		last->next = gm->vert_id(clip);
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
#define next(vrt) (&gm->v[(vrt)->next])
void GLUT_display(){
	glClear(GL_COLOR_BUFFER_BIT);
	// Draw Clip polygon lines
	glLineWidth(1);
	glBegin(GL_LINES);
	glColor3f(.8,0,0);
	float last_x=clip->x, last_y=clip->y;
	for (GreinerV2f *vert=next(clip); vert; vert=next(vert)) {
		float x=vert->x, y=vert->y;
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
	for (GreinerV2f *v=clip; v; v=next(v)) {
		glVertex2f(v->x,v->y);
		if (!first_iter && v==clip) break;
		first_iter = false;
	}
	glEnd();
	
	// Draw Subject polygon lines & verts
	for (int i=0; i<gm->nx; i++) {
		for (int j=0; j<gm->ny; j++) {
			gm->poly_draw(gm->poly_for_cell(i,j), .04);
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
	
	// Draw Grid (with x,y offsets)
	float xo = 1*(12.0/win_width);
	float yo = 1*(12.0/win_height);
	xo=0; yo=0;
	
	//Draw purple grid boxes on cells intersected by subj's first line segment
	glColor3f(.5, 0, .5);
	std::vector<std::pair<int,int>> bottom_edges, left_edges, integer_cells;
	gm->find_cell_line_intersections(clip->x, clip->y,
									 gm->v[clip->next].x, gm->v[clip->next].y,
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
GreinerV2f *grabbed_vert = nullptr;

void GLUT_reshape(int w, int h){
	glViewport(0,0,w,h);
	win_width = w;
	win_height = h;
	if (debug){
		printf("GLUT_reshape w:%d h:%d\n",w,h);
	}
}
void dump_polys_to_stdout() {
	printf("bool clip_cyclic = %s; // Required for initialization\n",clip_cyclic?"true":"false");
	printf("std::vector<float> clip_verts = {");
	for (GreinerV2f *v=clip; gm->vert_id(v)&&v!=clip; v=&gm->v[v->next]) {
		printf((v->next&&&gm->v[v->next]!=clip)?"%f,%f, ":"%f,%f};\n",v->x,v->y);
		if (&gm->v[v->next]==clip) break;
	}
	printf("std::vector<float> inout_pts = {");
	for (size_t i=0,l=inout_pts.size()/2; i<l; i++) {
		printf((i!=l-1)?"%f,%f, ":"%f,%f};\n",inout_pts[2*i+0],inout_pts[2*i+1]);
	}
}
void toggle_cyclic(GreinerV2f *curve) {
	bool iscyc = gm->poly_is_cyclic(curve);
	gm->poly_set_cyclic(curve, !iscyc);
	glutPostRedisplay();
}
void delete_last_selected_vert() {
	// Don't allow #subj verts or #clip verts -> 0
	if (!grabbed_vert || grabbed_vert==clip) return;
	GreinerV2f *next = &gm->v[grabbed_vert->next];
	GreinerV2f *prev = &gm->v[grabbed_vert->next];
	int next_id = gm->vert_id(next);
	int prev_id = gm->vert_id(prev);
	prev->next = next_id;
	next->prev = prev_id;
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
void create_pt(float sx, float sy) {
	if (!grabbed_vert) return;
	GreinerV2f *last_vert = gm->poly_last_vert(grabbed_vert);
	GreinerV2f *v = gm->vert_new(last_vert, &gm->v[last_vert->next]);
	v->x = sx;
	v->y = sy;
	grabbed_vert = v; // Let's drag the new vert we just made
	glutPostRedisplay();
}
void initiate_pt_drag_if_near_pt(float sx, float sy) {
	float closest_dist = 1e50;
	GreinerV2f *closest_vert = nullptr;
	for (GreinerV2f *v=clip; v; v=&gm->v[v->next]) {
		float dx = v->x - sx;
		float dy = v->y - sy;
		float dist = sqrt(dx*dx + dy*dy);
		if (dist<closest_dist) {
			closest_dist = dist;
			closest_vert = v;
		}
		if (&gm->v[v->next]==clip) break;
	}
	if (debug) printf("Nearest point to mousedown (%f)\n",closest_dist);
	grabbed_vert = (closest_dist<.1) ? closest_vert : nullptr;
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
		if (m&GLUT_ACTIVE_CTRL)
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
		grabbed_vert->x = sx;
		grabbed_vert->y = sy;
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