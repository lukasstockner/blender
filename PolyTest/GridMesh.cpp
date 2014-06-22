//
//  GridMesh.cpp
//  PolyTest
//
//  Created by Jonathan deWerd on 6/20/14.
//  Copyright (c) 2014 a.b.c. All rights reserved.
//

#include "GridMesh.h"

GreinerV2f *GreinerV2f::firstVert() {
	if (!prev || isBackbone) return this;
	GreinerV2f *v = this;
	while (v->prev) {
		v = v->prev;
		if (v->isBackbone) return v;
	}
	return v;
}

GreinerV2f *GreinerV2f::lastVert() {
	if (!next) return this;
	if (next->isBackbone) return this;
	GreinerV2f *v = this;
	while (v->next) {
		v = v->next;
		if (v->isBackbone) return v;
	}
	return v;
}

GreinerV2f *GreinerV2f::nextPolygon() {
	return firstVert()->nextPoly;
}

GreinerV2f *GreinerV2f::vertAt(float x, float y) {
	for(GreinerV2f *v = firstVert(); v; v=v->next) {
		if (fabs(x-v->x)+fabs(y-v->y)<tolerance) return v;
	}
	return nullptr;
}

bool GreinerV2f::isCyclic() {
	if (!prev || !next) return false;
	return bool(firstVert()->prev);
}

void GreinerV2f::setCyclic(bool cyc) {
	if (cyc==isCyclic()) return;
	GreinerV2f *first = firstVert();
	GreinerV2f *last = lastVert();
	if (cyc) {
		first->prev = last;
		last->next = first;
	} else {
		first->prev = nullptr;
		last->next = nullptr;
	}
}