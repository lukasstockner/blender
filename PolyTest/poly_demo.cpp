#include <GLUT/glut.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>


// Which demo is this?
#define DEMO_LINE_INTERSECTION


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
#include "poly.cpp"

/***************************** DEFAULT SCENE *****************************/
//int clip_n = 2;
//float clip_verts[] = {0,0, 0.5,0.5};
//int subj_n = 2;
//float subj_verts[] = {-.5,-.5, .5,-.5};
bool clip_cyclic = true; // Required for initialization
bool subj_cyclic = true;
std::vector<float> clip_verts = {-0.460000,-0.004000, 0.360000,-0.072000, 0.388000,-0.744000, -0.440000,-0.720000};
std::vector<float> subj_verts = {-0.500000,-0.500000, 0.500000,-0.500000};
std::vector<float> inout_pts = {};



GreinerV2f *clip;
GreinerV2f *subj;

int win_width = 500;
int win_height = 500;

void glut_coords_2_scene(float gx, float gy, float* sx, float* sy) {
	gx /= win_width;
	gy /= win_height;
	*sx = 2*gx-1;
	*sy = 2*(1-gy)-1;
}

void init_default_scene() {
	GreinerV2f *last = nullptr;
	size_t clip_n = clip_verts.size()/2;
	size_t subj_n = subj_verts.size()/2;
	for (int i=0; i<clip_n; i++) {
		GreinerV2f *v = new GreinerV2f();
		v->x = clip_verts[2*i+0];
		v->y = clip_verts[2*i+1];
		v->prev = last;
		if (last) last->next = v;
		else      clip = v;
		last = v;
	}
	if (clip_cyclic) {
		clip->prev = last;
		last->next = clip;
	}
	last = nullptr;
	for (int i=0; i<subj_n; i++) {
		GreinerV2f *v = new GreinerV2f();
		v->x = subj_verts[2*i+0];
		v->y = subj_verts[2*i+1];
		v->prev = last;
		if (last) last->next = v;
		else      subj = v;
		last = v;
	}
	if (subj_cyclic) {
		subj->prev = last;
		last->next = subj;
	}
}

void GLUT_init(){
    glClearColor(0,0,0,0);
    glMatrixMode(GL_PROJECTION);
	// Defines the view box
	// left,right,bottom,top
    gluOrtho2D(-1,1,-1,1);
	init_default_scene();
}


/***************************** DRAW *****************************/
GreinerV2f *intersect_pt = nullptr;

void GLUT_display(){
	glClear(GL_COLOR_BUFFER_BIT);
	// Draw Clip polygon lines
	glBegin(GL_LINES);
	float last_x=clip->x, last_y=clip->y;
	bool is_outside = true;
	if (subj_cyclic) is_outside = point_in_polygon(last_x, last_y, clip);
	for (GreinerV2f *v=clip->next; v; v=v->next) {
		float x=v->x, y=v->y;
		if (is_outside) glColor3f(.8,0,0);
		else glColor3f(1,.6,.6);
		glVertex2f(last_x,last_y);
		glVertex2f(x,y);
		last_x=x; last_y=y;
		if (v==clip) break;
		if (v->isIntersection) is_outside = !is_outside;
	}
	glEnd();

	//Draw Clip polygon verts
	glPointSize(5);
	glBegin(GL_POINTS);
	glColor3f(1,0,0);
	bool first_iter = true;
	for (GreinerV2f *v=clip; v; v=v->next) {
		glVertex2f(v->x,v->y);
		if (!first_iter && v==clip) break;
		first_iter = false;
	}
	glEnd();

	// Draw Subject polygon lines
	glBegin(GL_LINES);
	is_outside = true;
	last_x=subj->x, last_y=subj->y;
	if (clip_cyclic) is_outside = point_in_polygon(last_x, last_y, subj);
	for (GreinerV2f *v=subj->next; v; v=v->next) {
		float x=v->x, y=v->y;
		if (is_outside) glColor3f(0,.8,0);
		else glColor3f(.8,1,.8);
		glVertex2f(last_x,last_y);
		glVertex2f(x,y);
		last_x=x; last_y=y;
		if (v==subj) break;
		if (v->isIntersection) is_outside = !is_outside;
	}
	glEnd();

	// Draw Subject polygon lines
	glPointSize(3);
	glBegin(GL_POINTS);
	glColor3f(0,1,0);
	first_iter = true;
	for (GreinerV2f *v=subj; v; v=v->next) {
		glVertex2f(v->x,v->y);
		if (!first_iter && v==subj) break;
		first_iter = false;
	}
	glEnd();
	
	// Draw intersection point
	if (intersect_pt) {
		glPointSize(5);
		glBegin(GL_POINTS);
		glColor3f(1,1,0);
		glVertex2f(intersect_pt->x, intersect_pt->y);
		glEnd();
	}

	// Draw inclusion/exclusion test points
	if (clip_cyclic) {
		glPointSize(5);
		glBegin(GL_POINTS);
		for (size_t i=0,l=inout_pts.size()/2; i<l; i++) {
			float x=inout_pts[i*2+0], y=inout_pts[i*2+1];
			bool pip = point_in_polygon(x,y,clip);
			if (pip) glColor3f(1,1,0);
			else glColor3f(0, 0, 1);
			glVertex2f(x,y);
		}
		glEnd();
	}


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
	printf("bool subj_cyclic = %s;\n",subj_cyclic?"true":"false");
	printf("std::vector<float> clip_verts = {");
	for (GreinerV2f *v=clip; v; v=v->next) {
		printf((v->next&&v->next!=clip)?"%f,%f, ":"%f,%f};\n",v->x,v->y);
		if (v->next==clip) break;
	}
	printf("std::vector<float> subj_verts = {");
	for (GreinerV2f *v=subj; v; v=v->next) {
		printf((v->next&&v->next!=subj)?"%f,%f, ":"%f,%f};\n",v->x,v->y);
		if (v->next==subj) break;
	}
	printf("std::vector<float> inout_pts = {");
	for (size_t i=0,l=inout_pts.size()/2; i<l; i++) {
		printf((i!=l-1)?"%f,%f, ":"%f,%f};\n",inout_pts[2*i+0],inout_pts[2*i+1]);
	}
}
void toggle_cyclic(GreinerV2f *curve) {
	if (curve->prev) { // is cyclic
		curve->prev->next = nullptr;
		curve->prev = nullptr;
	} else {
		GreinerV2f *last = curve;
		while (last->next) last = last->next;
		last->next = curve;
		curve->prev = last;
	}
	clip_cyclic = bool(clip->prev);
	subj_cyclic = bool(subj->prev);
	glutPostRedisplay();
}
void delete_last_selected_vert() {
	if (!grabbed_vert || grabbed_vert==clip || grabbed_vert==subj) return;
	GreinerV2f *next=grabbed_vert->next, *prev=grabbed_vert->prev;
	if (next && prev) {
		prev->next = next;
		next->prev = prev;
	} else if (next) {
		next->prev = nullptr;
	} else if (prev) {
		prev->next = nullptr;
	}
	delete grabbed_vert;
	grabbed_vert = nullptr;
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
		add_verts_at_intersections(clip,subj);
		glutPostRedisplay();
	}
	if (ch=='1') toggle_cyclic(clip);
	if (ch=='2') toggle_cyclic(subj);
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
	GreinerV2f *last_vert = grabbed_vert;
	while (last_vert->next && last_vert->next!=clip && last_vert->next!=subj)
		last_vert = last_vert->next;
	GreinerV2f *clip_last = clip;
	while (clip_last->next && clip_last->next!=clip) clip_last = clip_last->next;
	GreinerV2f *subj_last = subj;
	while (subj_last->next && subj_last->next!=subj) subj_last = subj_last->next;
	GreinerV2f *v = new GreinerV2f();
	v->x = sx;
	v->y = sy;
	if (last_vert->next) { // is cyclic
		GreinerV2f *first_vert = last_vert->next;
		last_vert->next = v;
		v->prev = last_vert;
		first_vert->prev = v;
		v->next = first_vert;
	} else {
		last_vert->next = v;
		v->prev = last_vert;
	}
	grabbed_vert = v; // Let's drag the new vert we just made
	glutPostRedisplay();
}
void initiate_pt_drag_if_near_pt(float sx, float sy) {
	float closest_dist = 1e50;
	GreinerV2f *closest_vert = nullptr;
	for (GreinerV2f *v=clip; v; v=v->next) {
		float dx = v->x - sx;
		float dy = v->y - sy;
		float dist = sqrt(dx*dx + dy*dy);
		if (dist<closest_dist) {
			closest_dist = dist;
			closest_vert = v;
		}
		if (v->next==clip) break;
	}
	for (GreinerV2f *v=subj; v; v=v->next) {
		float dx = v->x - sx;
		float dy = v->y - sy;
		float dist = sqrt(dx*dx + dy*dy);
		if (dist<closest_dist) {
			closest_dist = dist;
			closest_vert = v;
		}
		if (v->next==subj) break;
	}
	if (debug) printf("Nearest point to mousedown (%f)\n",closest_dist);
	grabbed_vert = (closest_dist<.025) ? closest_vert : nullptr;
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
#if defined(DEMO_LINE_INTERSECTION)
	if (clip && clip->next && subj && subj->next) {
		float alpha1, alpha2;
		bool does_intersect = get_line_seg_intersection(clip,clip->next,&alpha1, subj,subj->next,&alpha2);
		if (does_intersect) {
			if (!intersect_pt) intersect_pt = new GreinerV2f();
			intersect_pt->x = (1-alpha1)*clip->x + alpha1*clip->next->x;
			intersect_pt->y = (1-alpha1)*clip->y + alpha1*clip->next->y;
		} else {
			intersect_pt = nullptr;
		}
	} else {
		intersect_pt = nullptr;
	}
#endif
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