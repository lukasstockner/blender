//
//  GridMesh.h
//  PolyTest
//
//  Created by Jonathan deWerd on 6/20/14.
//  Copyright (c) 2014 a.b.c. All rights reserved.
//

#ifndef __PolyTest__GridMesh__
#define __PolyTest__GridMesh__

#include <iostream>

struct GreinerV2f {
	static tolerance = 1e-5;
	float x,y;
	GreinerV2f *prev, *next; // Prev,next verts in the *same* polygon
	GreinerV2f *nextPoly;   // First vertex of the *next* polygon
	float alpha; // If this vertex came from an affine comb, this is the mixing factor
	bool isIntersection; // True if this vertex was added at an intersection
	bool isInterior;
	bool isBackbone; // True if nextPoly!=nullptr || exists prevPoly s.t. prevPoly->nextPoly == this
	GreinerV2f *entryNeighbor; // Corresp. vertex at same {x,y} in different polygon
	GreinerV2f *exitNeighbor;  // Exit = ->next->next->next along this polygon *exits* other polygon
	GreinerV2f() :	next(nullptr), prev(nullptr),
					nextPoly(nullptr), entryNeighbor(nullptr), exitNeighbor(nullptr),
					isIntersection(false), isBackbone(false) {};
	GreinerV2f *firstVert(); // First vert of this polygon
	GreinerV2f *lastVert(); // Last vert of this polygon
	GreinerV2f *nextPolygon(); // equiv to firstVert()->nextPoly
	GreinerV2f *vertAt(float x, float y); // finds the vert in this poly near x,y
	bool isCyclic();
	void setCyclic(bool cyc);
};

struct GridMesh {
	std::vector<GreinerV2f> v; // Vertex storage. "int up" refers to v[up].
	
	
};

#endif
