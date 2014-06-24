/* Apache License, Version 2.0 */

#include "testing/testing.h"

/* Use to write out OBJ files, handy for checking output */
// #define USE_OBJ_PREVIEW

extern "C" {
#include "BLI_polyfill2d.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "MEM_guardedalloc.h"

#ifdef USE_OBJ_PREVIEW
#  include "BLI_string.h"
#endif
}

/* -------------------------------------------------------------------- */
/* test utility functions */

#define TRI_ERROR_VALUE (unsigned int)-1

static void test_valid_polyfill_prepare(unsigned int tris[][3], unsigned int tris_tot)
{
	unsigned int i;
	for (i = 0; i < tris_tot; i++) {
		unsigned int j;
		for (j = 0; j < 3; j++) {
			tris[i][j] = TRI_ERROR_VALUE;
		}
	}
}

/**
 * Basic check for face index values:
 *
 * - no duplicates.
 * - all tris set.
 * - all verts used at least once.
 */
static void test_polyfill_simple(
        const float poly[][2], const unsigned int poly_tot,
        const unsigned int tris[][3], const unsigned int tris_tot)
{
	unsigned int i;
	int *tot_used = (int *)MEM_callocN(poly_tot * sizeof(int), __func__);
	for (i = 0; i < tris_tot; i++) {
		unsigned int j;
		for (j = 0; j < 3; j++) {
			EXPECT_NE(TRI_ERROR_VALUE, tris[i][j]);
			tot_used[tris[i][j]] += 1;
		}
		EXPECT_NE(tris[i][0], tris[i][1]);
		EXPECT_NE(tris[i][1], tris[i][2]);
		EXPECT_NE(tris[i][2], tris[i][0]);
	}
	for (i = 0; i < poly_tot; i++) {
		EXPECT_NE(0, tot_used[i]);
	}
	MEM_freeN(tot_used);
}

static void  test_polyfill_topology(
        const float poly[][2], const unsigned int poly_tot,
        const unsigned int tris[][3], const unsigned int tris_tot)
{
	EdgeHash *edgehash = BLI_edgehash_new(__func__);
	EdgeHashIterator *ehi;
	unsigned int i;
	for (i = 0; i < tris_tot; i++) {
		unsigned int j;
		for (j = 0; j < 3; j++) {
			const unsigned int v1 = tris[i][j];
			const unsigned int v2 = tris[i][(j + 1) % 3];
			void **p = BLI_edgehash_lookup_p(edgehash, v1, v2);
			if (p) {
				*p = (void *)((intptr_t)*p + (intptr_t)1);
			}
			else {
				BLI_edgehash_insert(edgehash, v1, v2, (void *)(intptr_t)1);
			}
		}
	}
	EXPECT_EQ(poly_tot + (poly_tot - 3), BLI_edgehash_size(edgehash));

	for (i = 0; i < poly_tot; i++) {
		const unsigned int v1 = i;
		const unsigned int v2 = (i + 1) % poly_tot;
		void **p = BLI_edgehash_lookup_p(edgehash, v1, v2);
		EXPECT_EQ(1, (void *)p != NULL);
		EXPECT_EQ(1, (intptr_t)*p);
	}

	for (ehi = BLI_edgehashIterator_new(edgehash), i = 0;
	     BLI_edgehashIterator_isDone(ehi) == false;
	     BLI_edgehashIterator_step(ehi), i++)
	{
		void **p = BLI_edgehashIterator_getValue_p(ehi);
		EXPECT_EQ(true, ELEM((intptr_t)*p, 1, 2));
	}

	BLI_edgehash_free(edgehash, NULL);
}

/**
 * Check all faces are flipped the same way
 */
static void  test_polyfill_winding(
        const float poly[][2], const unsigned int poly_tot,
        const unsigned int tris[][3], const unsigned int tris_tot)
{
	unsigned int i;
	unsigned int count[2] = {0, 0};
	for (i = 0; i < tris_tot; i++) {
		float winding_test = cross_tri_v2(poly[tris[i][0]], poly[tris[i][1]], poly[tris[i][2]]);
		if (fabsf(winding_test) > FLT_EPSILON) {
			count[winding_test < 0.0f] += 1;
		}
	}
	EXPECT_EQ(true, ELEM(0, count[0], count[1]));
}

/**
 * Check the accumulated triangle area is close to the original area.
 */
static void test_polyfill_area(
        const float poly[][2], const unsigned int poly_tot,
        const unsigned int tris[][3], const unsigned int tris_tot)
{
	unsigned int i;
	const float area_tot = area_poly_v2(poly, poly_tot);
	float       area_tot_tris = 0.0f;
	const float eps_abs = 0.00001f;
	const float eps = area_tot > 1.0f ? (area_tot * eps_abs) : eps_abs;
	for (i = 0; i < tris_tot; i++) {
		area_tot_tris += area_tri_v2(poly[tris[i][0]], poly[tris[i][1]], poly[tris[i][2]]);
	}
	EXPECT_NEAR(area_tot, area_tot_tris, eps);
}

/**
 * Main template for polyfill testing.
 */
#define TEST_POLYFILL_TEMPLATE_STATIC(poly, is_degenerate) \
{ \
	unsigned int tris[POLY_TRI_COUNT(ARRAY_SIZE(poly))][3]; \
	const unsigned int poly_tot = ARRAY_SIZE(poly); \
	const unsigned int tris_tot = ARRAY_SIZE(tris); \
	test_valid_polyfill_prepare(tris, tris_tot); \
	\
	BLI_polyfill_calc(poly, poly_tot, 0, tris); \
	\
	test_polyfill_simple(poly, poly_tot, (const unsigned int (*)[3])tris, tris_tot); \
	test_polyfill_topology(poly, poly_tot, (const unsigned int (*)[3])tris, tris_tot); \
	if (!is_degenerate) { \
		test_polyfill_winding(poly, poly_tot, (const unsigned int (*)[3])tris, tris_tot); \
		\
		test_polyfill_area(poly, poly_tot, (const unsigned int (*)[3])tris, tris_tot); \
	} \
	polyfill_to_obj(typeid(*this).name(), poly, poly_tot, (const unsigned int (*)[3])tris, tris_tot); \
} (void)0

/* -------------------------------------------------------------------- */
/* visualisation functions (not needed for testing) */

#ifdef USE_OBJ_PREVIEW
static void polyfill_to_obj(
        const char *id,
        const float poly[][2], const unsigned int poly_tot,
        const unsigned int tris[][3], const unsigned int tris_tot)
{
	char path[1024];
	FILE *f;
	unsigned int i;

	BLI_snprintf(path, sizeof(path), "%s.obj", id);

	f = fopen(path, "w");
	if (!f) {
		return;
	}

	for (i = 0; i < poly_tot; i++) {
		fprintf(f, "v %f %f 0.0\n", UNPACK2(poly[i]));
	}

	for (i = 0; i < tris_tot; i++) {
		fprintf(f, "f %u %u %u\n", UNPACK3OP(1 +, tris[i]));
	}

	fclose(f);
}
#else
static void polyfill_to_obj(
        const char *id,
        const float poly[][2], const unsigned int poly_tot,
        const unsigned int tris[][3], const unsigned int tris_tot)
{
	(void)id;
	(void)poly, (void)poly_tot;
	(void)tris, (void)tris_tot;
}
#endif  /* USE_OBJ_PREVIEW */


/* -------------------------------------------------------------------- */
/* tests */

#define POLY_TRI_COUNT(len) ((len) - 2)

#if 0
/* BLI_cleanup_path */
TEST(polyfill2d, Empty)
{
	BLI_polyfill_calc(NULL, 0, 0, NULL);
}
#endif

// @Override
// public void create () {
// // An empty "polygon"
// testCases.add(new TestCase(new float[] {}, true));
//
// // A point
// testCases.add(new TestCase(new float[] {0, 0}, true));
//
// // A line segment
// testCases.add(new TestCase(new float[] {0, 0, 1, 1}, true));

/* A counterclockwise triangle */
TEST(polyfill2d, TriangleCCW)
{
	float poly[][2] = {{0, 0}, {0, 1}, {1, 0},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* A counterclockwise square */
TEST(polyfill2d, SquareCCW)
{
	float poly[][2] = {{0, 0}, {0, 1}, {1, 1}, {1, 0},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* A clockwise square */
TEST(polyfill2d, SquareCW)
{
	float poly[][2] = {{0, 0}, {1, 0}, {1, 1}, {0, 1},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Starfleet insigna */
TEST(polyfill2d, Starfleet)
{
	float poly[][2] = {{0, 0}, {0.6f, 0.4f}, {1, 0}, {0.5f, 1},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Starfleet insigna with repeated point */
TEST(polyfill2d, StarfleetDegenerate)
{
	float poly[][2] = {{0, 0}, {0.6f, 0.4f}, {0.6f, 0.4f}, {1, 0}, {0.5f, 1},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Three collinear points */
TEST(polyfill2d, 3Colinear)
{
	float poly[][2] = {{0, 0}, {1, 0}, {2, 0},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Four collinear points */
TEST(polyfill2d, 4Colinear)
{
	float poly[][2] = {{0, 0}, {1, 0}, {2, 0}, {3, 0},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Non-consecutive collinear points */
TEST(polyfill2d, UnorderedColinear)
{
	float poly[][2] = {{0, 0}, {1, 1}, {2, 0}, {3, 1}, {4, 0},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Plus shape */
TEST(polyfill2d, PlusShape)
{
	float poly[][2] = {
	    {1, 0}, {2, 0}, {2, 1}, {3, 1}, {3, 2}, {2, 2}, {2, 3}, {1, 3}, {1, 2}, {0, 2}, {0, 1}, {1, 1},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Star shape */
TEST(polyfill2d, StarShape)
{
	float poly[][2] = {
	    {4, 0}, {5, 3}, {8, 4}, {5, 5}, {4, 8}, {3, 5}, {0, 4}, {3, 3},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* U shape */
TEST(polyfill2d, UShape)
{
	float poly[][2] = {
	    {1, 0}, {2, 0}, {3, 1}, {3, 3}, {2, 3}, {2, 1}, {1, 1}, {1, 3}, {0, 3}, {0, 1},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Spiral */
TEST(polyfill2d, Spiral)
{
	float poly[][2] = {
	    {1, 0}, {4, 0}, {5, 1}, {5, 4}, {4, 5}, {1, 5}, {0, 4}, {0, 3},
	    {1, 2}, {2, 2}, {3, 3}, {1, 3}, {1, 4}, {4, 4}, {4, 1}, {0, 1},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Test case from http:# www.flipcode.com/archives/Efficient_Polygon_Triangulation.shtml */
TEST(polyfill2d, TestFlipCode)
{
	float poly[][2] = {
	    {0, 6}, {0, 0}, {3, 0}, {4, 1}, {6, 1}, {8, 0}, {12, 0}, {13, 2},
	    {8, 2}, {8, 4}, {11, 4}, {11, 6}, {6, 6}, {4, 3}, {2, 6},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Self-intersection */
TEST(polyfill2d, SelfIntersect)
{
	float poly[][2] = {{0, 0}, {1, 1}, {2, -1}, {3, 1}, {4, 0},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, true);
}

/* Self-touching */
TEST(polyfill2d, SelfTouch)
{
	float poly[][2] = {
	    {0, 0}, {4, 0}, {4, 4}, {2, 4}, {2, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {2, 3}, {2, 4}, {0, 4},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Self-overlapping */
TEST(polyfill2d, SelfOverlap)
{
	float poly[][2] = {
	    {0, 0}, {4, 0}, {4, 4}, {1, 4}, {1, 3}, {3, 3}, {3, 1}, {1, 1}, {1, 3}, {3, 3}, {3, 4}, {0, 4},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, true);
}

/* Test case from http:# www.davdata.nl/math/polygons.html */
TEST(polyfill2d, TestDavData)
{
	float poly[][2] = {
	    {190, 480}, {140, 180}, {310, 100}, {330, 390}, {290, 390}, {280, 260}, {220, 260}, {220, 430}, {370, 430},
	    {350, 30}, {50, 30}, {160, 560}, {730, 510}, {710, 20}, {410, 30}, {470, 440}, {640, 410}, {630, 140},
	    {590, 140}, {580, 360}, {510, 370}, {510, 60}, {650, 70}, {660, 450}, {190, 480},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Issue 815, http:# code.google.com/p/libgdx/issues/detail?id=815 */
TEST(polyfill2d, TestIssue815)
{
	float poly[][2] = {
	    {-2.0f, 0.0f}, {-2.0f, 0.5f}, {0.0f, 1.0f}, {0.5f, 2.875f},
	    {1.0f, 0.5f}, {1.5f, 1.0f}, {2.0f, 1.0f}, {2.0f, 0.0f},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Issue 207, comment #1, http:# code.google.com/p/libgdx/issues/detail?id=207#c1 */
TEST(polyfill2d, TestIssue207_1)
{
	float poly[][2] = {
	    {72.42465f, 197.07095f}, {78.485535f, 189.92776f}, {86.12059f, 180.92929f}, {99.68253f, 164.94557f},
	    {105.24325f, 165.79604f}, {107.21862f, 166.09814f}, {112.41958f, 162.78253f}, {113.73238f, 161.94562f},
	    {123.29477f, 167.93805f}, {126.70667f, 170.07617f}, {73.22717f, 199.51062f},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, true);
}

/* Issue 207, comment #11, http:# code.google.com/p/libgdx/issues/detail?id=207#c11 */
/* Also on issue 1081, http:# code.google.com/p/libgdx/issues/detail?id=1081 */
TEST(polyfill2d, TestIssue207_11)
{
	float poly[][2] = {
	    {2400.0f, 480.0f}, {2400.0f, 176.0f}, {1920.0f, 480.0f}, {1920.0459f, 484.22314f},
	    {1920.1797f, 487.91016f}, {1920.3955f, 491.0874f}, {1920.6875f, 493.78125f}, {1921.0498f, 496.01807f},
	    {1921.4766f, 497.82422f}, {1921.9619f, 499.22607f}, {1922.5f, 500.25f}, {1923.085f, 500.92236f},
	    {1923.7109f, 501.26953f}, {1924.3721f, 501.31787f}, {1925.0625f, 501.09375f}, {1925.7764f, 500.62354f},
	    {1926.5078f, 499.9336f}, {1927.251f, 499.0503f}, {1928.0f, 498.0f}, {1928.749f, 496.80908f},
	    {1929.4922f, 495.5039f}, {1930.2236f, 494.11084f}, {1930.9375f, 492.65625f}, {1931.6279f, 491.1665f},
	    {1932.2891f, 489.66797f}, {1932.915f, 488.187f}, {1933.5f, 486.75f}, {1934.0381f, 485.3833f},
	    {1934.5234f, 484.11328f}, {1934.9502f, 482.9663f}, {1935.3125f, 481.96875f}, {1935.6045f, 481.14697f},
	    {1935.8203f, 480.52734f}, {1935.9541f, 480.13623f}, {1936.0f, 480.0f},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Issue 1407, http:# code.google.com/p/libgdx/issues/detail?id=1407 */
TEST(polyfill2d, TestIssue1407)
{
	float poly[][2] = {
	    {3.914329f, 1.9008259f}, {4.414321f, 1.903619f}, {4.8973203f, 1.9063174f}, {5.4979978f, 1.9096732f},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}

/* Issue 1407, http:# code.google.com/p/libgdx/issues/detail?id=1407, */
/* with an additional point to show what is happening. */
TEST(polyfill2d, TestIssue1407_pt)
{
	float poly[][2] = {
	    {3.914329f, 1.9008259f}, {4.414321f, 1.903619f}, {4.8973203f, 1.9063174f}, {5.4979978f, 1.9096732f}, {4, 4},};
	TEST_POLYFILL_TEMPLATE_STATIC(poly, false);
}
