// A vertex of a 2D polygon to which the [Greiner,1998]
// clipping/intersection/union/splitting algorithm is to be applied.
struct GreinerV2f {
	float x,y;
	struct GreinerV2f *next, *prev; // Prev,next verts in the *same* polygon
	struct GreinerV2f *nextPoly;   // First vertex of the *next* polygon
	bool isIntersection; // True if this vertex was added at an intersection
	bool isEntry; // True if proceeding along this poly with ->next->next will enter the other polygon when this vertex is passed
	bool isBackbone;
	struct GreinerV2f *neighbour; // Corresp. vertex at same {x,y} in different polygon
	float alpha; // If this vertex came from an affine comb, this is the mixing factor
	GreinerV2f() : next(nullptr), prev(nullptr),
	               nextPoly(nullptr), neighbour(nullptr),
	               isIntersection(false), isBackbone(false) {};
};

GreinerV2f* insert_vert_at_intersect(GreinerV2f* poly1left,
									 GreinerV2f* poly1right,
									 float alpha1,
									 GreinerV2f* poly2left,
									 GreinerV2f* poly2right,
									 float alpha2
									  ) {
	// Interpolate beteen poly1left, poly1right according to alpha
	// to find the location at which we are going to add the intersection
	// vertices (1 for each polygon, each at the same spatial position)
	float x1 = (1-alpha1)*poly1left->x + alpha1*poly1right->x;
	float y1 = (1-alpha1)*poly1left->y + alpha1*poly1right->y;
	if (debug) {
		// If this really is an intersection, we should get the same position
		// by interpolating the poly1 edge as we do by interpolating the poly2
		// edge. If debug==true, check!
		float x2 = (1-alpha2)*poly2left->x + alpha2*poly2right->x;
		float y2 = (1-alpha2)*poly2left->y + alpha2*poly2right->y;
		float xdiff = x1-x2;
		float ydiff = y1-y2;
		if (sqrt(xdiff*xdiff+ydiff*ydiff)>intersect_check_tol) {
			printf("WARNING: bad intersection\n!");
		}
		// "Left" vertices should always come before "right" vertices in the
		// corresponding linked lists
		if (poly1left->next!=poly1right || poly1right->prev!=poly1left)
			printf("WARNING: 'left', 'right' vertices in wrong order\n");
		if (poly2left->next!=poly2right || poly2right->prev!=poly2left)
			printf("WARNING: 'left', 'right' vertices in wrong order\n");
	}
	// Insert an intersection vertex into polygon 1
	GreinerV2f *newv1 = new GreinerV2f();
	newv1->x = x1;
	newv1->y = y1;
	newv1->isIntersection = true;
	poly1left->next = newv1;
	newv1->next = poly1right;
	poly1right->prev = newv1;
	newv1->prev = poly1left;
	
	// Insert an intersection vertex into polygon 2
	GreinerV2f *newv2 = new GreinerV2f();
	newv2->x = x1;
	newv2->y = y1;
	newv2->isIntersection = true;
	poly2left->next = newv2;
	newv2->next = poly2right;
	poly2right->prev = newv2;
	newv2->prev = poly2left;
	
	// Tell the intersection vertices that they're stacked on top of one another
	newv1->neighbour = newv2;
	newv2->neighbour = newv1;
	
	return newv1;
}

// Returns true if p1,p2,p3 form a tripple that is in counter-clockwise
// orientation. In other words, if ((p2-p1)x(p3-p2)).z >0
inline bool points_ccw(GreinerV2f* p1, GreinerV2f* p2, GreinerV2f* p3) {
	float x1=p1->x, x2=p2->x, x3=p3->x;
	float y1=p1->y, y2=p2->y, y3=p3->y;
	float z = x1*(y2-y3) + x2*(y3-y1) + x3*(y1-y2);
	return z>0;
}

bool get_line_seg_intersection(GreinerV2f* poly1left,
							   GreinerV2f* poly1right,
							   float* alpha1,
							   GreinerV2f* poly2left,
							   GreinerV2f* poly2right,
							   float* alpha2
							   ) {
	// poly1left--------poly1right
	//   poly2left------------poly2right
	// Do they intersect? If so, return true and fill alpha{1,2} with the
	// affine interpolation params necessary to find the intersection.
	bool ccw_acd = points_ccw(poly1left, poly2left, poly2right);
	bool ccw_bcd = points_ccw(poly1right, poly2left, poly2right);
	if (ccw_acd==ccw_bcd) return false;
	bool ccw_abc = points_ccw(poly1left, poly1right, poly2left);
	bool ccw_abd = points_ccw(poly1left, poly1right, poly2right);
	if (ccw_abc==ccw_abd) return false;
	double a11 = poly1right->x - poly1left->x;
	double a12 = poly2left->x - poly2right->x;
	double a21 = poly1right->y - poly1left->y;
	double a22 = poly2left->y - poly2right->y;
	double idet = 1/(a11*a22-a12*a21);
	double b1 = poly2left->x-poly1left->x;
	double b2 = poly2left->y-poly1left->y;
	double x1 = (+b1*a22 -b2*a12)*idet;
	double x2 = (-b1*a21 +b2*a11)*idet;
	if (alpha1) *alpha1 = x1;
	if (alpha2) *alpha2 = x2;
	return true;
}

// Yeah, the pairwise intersection test is O(n*m) instead of O(N*ln(N)), but
// does it really matter if m==4 almost all the time? The sweepline algo has
// many edge cases but this one is simpler. We can always move to sweepline
// if this is too slow (maybe if someone tries to cut a speaker with 10,000 holes)?
double min_dist = 1e-5;
int add_verts_at_intersections(GreinerV2f* poly1, GreinerV2f* poly2) {
	int added_pt_count = 0;
	for (GreinerV2f* v1=poly1; v1->next; v1=v1->next) {
		for (GreinerV2f* v2=poly2; v2->next; v2=v2->next) {
			float a1, a2;
			bool does_intersect = get_line_seg_intersection(v1,v1->next,&a1, v2,v2->next,&a2);
			bool misses_v1 = fmin(a1,1-a1)>min_dist;
			bool misses_v2 = fmin(a2,1-a2)>min_dist;
			if (does_intersect && misses_v1 && misses_v2) {
				insert_vert_at_intersect(v1,v1->next,a1, v2,v2->next,a2);
				added_pt_count++;
			}
			if (v2->next==poly2) break;
		}
		if (v1->next==poly1) break;
	}
	return added_pt_count;
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

bool point_in_polygon(float x, float y, GreinerV2f* poly) {
	bool contains_boundary = true;
	float last_x=poly->x, last_y=poly->y;
	int last_quadrant = quadrant(last_x,last_y,x,y);
	if (last_quadrant==99) return contains_boundary;
	int ccw = 0; // Number of counter-clockwise quarter turns around pt
	for (GreinerV2f* v=poly->next; v; v=v->next) {
		float next_x=v->x, next_y=v->y;
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
		if (v==poly) break;
	}
	// Note: return is_even to exclude self-intersecting regions
	return ccw!=0;
}
