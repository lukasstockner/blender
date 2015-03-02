/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_rand.h"
}

#define TESTCASE_SIZE 10000

/* Only keeping this in case here, for now. */
#define PRINTF_GHASH_STATS(_gh) \
{ \
	double q, lf, var, pempty, poverloaded; \
	int bigb; \
	q = BLI_ghash_calc_quality((_gh), &lf, &var, &pempty, &poverloaded, &bigb); \
	printf("GHash stats (%d entries):\n\t" \
	       "Quality (the lower the better): %f\n\tVariance (the lower the better): %f\n\tLoad: %f\n\t" \
	       "Empty buckets: %.2f%%\n\tOverloaded buckets: %.2f%% (biggest bucket: %d)\n", \
	       BLI_ghash_size(_gh), q, var, lf, pempty * 100.0, poverloaded * 100.0, bigb); \
} void (0)

/* Note: for pure-ghash testing, nature of the keys and data have absolutely no importance! So here we just use mere
 *       random integers stored in pointers. */

static void init_keys(unsigned int keys[TESTCASE_SIZE], const int seed)
{
	RNG *rng = BLI_rng_new(seed);
	unsigned int *k;
	int i;

	for (i = 0, k = keys; i < TESTCASE_SIZE; ) {
		/* Risks of collision are low, but they do exist.
		 * And we cannot use a GSet, since we test that here! */
		int j, t = BLI_rng_get_uint(rng);
		for (j = i; j--; ) {
			if (keys[j] == t) {
				continue;
			}
		}
		*k = t;
		i++;
		k++;
	}
	BLI_rng_free(rng);
}

/* Here we simply insert and then lookup all keys, ensuring we do get back the expected stored 'data'. */
TEST(ghash, InsertLookup)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 0);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		BLI_ghash_insert(ghash, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}

	EXPECT_EQ(TESTCASE_SIZE, BLI_ghash_size(ghash));

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		void *v = BLI_ghash_lookup(ghash, SET_UINT_IN_POINTER(*k));
		EXPECT_EQ(*k, GET_UINT_FROM_POINTER(v));
	}

	BLI_ghash_free(ghash, NULL, NULL);
}

/* Here we simply insert and then remove all keys, ensuring we do get an empty, unshrinked ghash. */
TEST(ghash, InsertRemove)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	unsigned int keys[TESTCASE_SIZE], *k;
	int i, bkt_size;

	init_keys(keys, 10);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		BLI_ghash_insert(ghash, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}

	EXPECT_EQ(TESTCASE_SIZE, BLI_ghash_size(ghash));
	bkt_size = BLI_ghash_buckets_size(ghash);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		void *v = BLI_ghash_popkey(ghash, SET_UINT_IN_POINTER(*k), NULL);
		EXPECT_EQ(*k, GET_UINT_FROM_POINTER(v));
	}

	EXPECT_EQ(0, BLI_ghash_size(ghash));
	EXPECT_EQ(bkt_size, BLI_ghash_buckets_size(ghash));

	BLI_ghash_free(ghash, NULL, NULL);
}

/* Same as above, but this time we allow ghash to shrink. */
TEST(ghash, InsertRemoveShrink)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	unsigned int keys[TESTCASE_SIZE], *k;
	int i, bkt_size;

	BLI_ghash_flag_set(ghash, GHASH_FLAG_ALLOW_SHRINK);
	init_keys(keys, 20);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		BLI_ghash_insert(ghash, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}

	EXPECT_EQ(TESTCASE_SIZE, BLI_ghash_size(ghash));
	bkt_size = BLI_ghash_buckets_size(ghash);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		void *v = BLI_ghash_popkey(ghash, SET_UINT_IN_POINTER(*k), NULL);
		EXPECT_EQ(*k, GET_UINT_FROM_POINTER(v));
	}

	EXPECT_EQ(0, BLI_ghash_size(ghash));
	EXPECT_LT(BLI_ghash_buckets_size(ghash), bkt_size);

	BLI_ghash_free(ghash, NULL, NULL);
}

/* Check copy. */
TEST(ghash, Copy)
{
	GHash *ghash = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_copy;
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 30);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		BLI_ghash_insert(ghash, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}

	EXPECT_EQ(TESTCASE_SIZE, BLI_ghash_size(ghash));

	ghash_copy = BLI_ghash_copy(ghash, NULL, NULL);

	EXPECT_EQ(TESTCASE_SIZE, BLI_ghash_size(ghash_copy));
	EXPECT_EQ(BLI_ghash_buckets_size(ghash), BLI_ghash_buckets_size(ghash_copy));

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		void *v = BLI_ghash_lookup(ghash_copy, SET_UINT_IN_POINTER(*k));
		EXPECT_EQ(*k, GET_UINT_FROM_POINTER(v));
	}

	BLI_ghash_free(ghash, NULL, NULL);
	BLI_ghash_free(ghash_copy, NULL, NULL);
}

/* Check disjoint. */
TEST(ghash, Disjoint)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 40);

	for (i = (TESTCASE_SIZE - 2) / 2, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = (TESTCASE_SIZE - 2) / 2; i--; k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(keys[0]), SET_UINT_IN_POINTER(keys[0]));

	EXPECT_FALSE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	BLI_ghash_remove(ghash_2, SET_UINT_IN_POINTER(keys[0]), NULL, NULL);

	EXPECT_TRUE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
}

/* Check equality. */
TEST(ghash, Equal)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 50);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = 0, k = keys; i < TESTCASE_SIZE; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isequal(ghash_1, ghash_2));

	BLI_ghash_remove(ghash_2, SET_UINT_IN_POINTER(keys[TESTCASE_SIZE / 2]), NULL, NULL);

	EXPECT_FALSE(BLI_ghash_isequal(ghash_1, ghash_2));

	BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(keys[TESTCASE_SIZE / 2]), SET_UINT_IN_POINTER(keys[TESTCASE_SIZE / 2]));

	EXPECT_TRUE(BLI_ghash_isequal(ghash_1, ghash_2));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
}

/* Check subset. */
TEST(ghash, Subset)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 60);

	for (i = TESTCASE_SIZE, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = 0, k = keys; i < TESTCASE_SIZE / 2; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_issubset(ghash_1, ghash_2));

	BLI_ghash_remove(ghash_1, SET_UINT_IN_POINTER(keys[0]), NULL, NULL);

	EXPECT_FALSE(BLI_ghash_issubset(ghash_1, ghash_2));

	BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(keys[0]), SET_UINT_IN_POINTER(keys[0]));

	EXPECT_TRUE(BLI_ghash_issubset(ghash_1, ghash_2));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
}

/* Check Union (straight and reversed). */
TEST(ghash, Union)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_U, *ghash_U_rev;
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 70);

	for (i = TESTCASE_SIZE / 2, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = 0, k = keys; i < TESTCASE_SIZE / 2; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isequal(ghash_1, ghash_2));

	ghash_U = BLI_ghash_union(NULL, NULL, NULL, ghash_1, ghash_2, NULL);
	ghash_U_rev = BLI_ghash_union_reversed(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_TRUE(BLI_ghash_isequal(ghash_U, ghash_1));
	EXPECT_TRUE(BLI_ghash_isequal(ghash_U_rev, ghash_1));

	for (i = 0, k = keys; i < TESTCASE_SIZE / 2; i++, k++) {
		void *v = BLI_ghash_lookup(ghash_U, SET_UINT_IN_POINTER(*k));
		EXPECT_EQ(*k, GET_UINT_FROM_POINTER(v));

		v = BLI_ghash_lookup(ghash_U_rev, SET_UINT_IN_POINTER(*k));
		EXPECT_EQ(i, GET_INT_FROM_POINTER(v));
	}

	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_U, NULL, NULL);
	ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	for (i = TESTCASE_SIZE / 2, k = &keys[i]; i < TESTCASE_SIZE; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	ghash_U = BLI_ghash_union(NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_TRUE(BLI_ghash_issubset(ghash_U, ghash_1));
	EXPECT_TRUE(BLI_ghash_issubset(ghash_U, ghash_2));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_U, NULL, NULL);
	BLI_ghash_free(ghash_U_rev, NULL, NULL);
}

/* Check Intersection. */
TEST(ghash, Intersection)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_I;
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 80);

	for (i = TESTCASE_SIZE / 2, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = 0, k = keys; i < TESTCASE_SIZE / 2; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isequal(ghash_1, ghash_2));

	ghash_I = BLI_ghash_intersection(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_TRUE(BLI_ghash_isequal(ghash_I, ghash_1));

	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_I, NULL, NULL);
	ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	for (i = TESTCASE_SIZE / 2, k = &keys[i]; i < TESTCASE_SIZE; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	ghash_I = BLI_ghash_intersection(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_EQ(0, BLI_ghash_size(ghash_I));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_I, NULL, NULL);
}

/* Check Difference. */
TEST(ghash, Difference)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_D;
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 90);

	for (i = TESTCASE_SIZE / 2, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = 0, k = keys; i < TESTCASE_SIZE / 2; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isequal(ghash_1, ghash_2));

	ghash_D = BLI_ghash_difference(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_EQ(0, BLI_ghash_size(ghash_D));

	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_D, NULL, NULL);
	ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	for (i = TESTCASE_SIZE / 2, k = &keys[i]; i < TESTCASE_SIZE; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	ghash_D = BLI_ghash_difference(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_TRUE(BLI_ghash_isequal(ghash_D, ghash_1));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_D, NULL, NULL);
}

/* Check Symmetric Difference. */
TEST(ghash, SymmDiff)
{
	GHash *ghash_1 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	GHash *ghash_SD;
	unsigned int keys[TESTCASE_SIZE], *k;
	int i;

	init_keys(keys, 100);

	for (i = TESTCASE_SIZE / 2, k = keys; i--; k++) {
		BLI_ghash_insert(ghash_1, SET_UINT_IN_POINTER(*k), SET_UINT_IN_POINTER(*k));
	}
	for (i = 0, k = keys; i < TESTCASE_SIZE / 2; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isequal(ghash_1, ghash_2));

	ghash_SD = BLI_ghash_symmetric_difference(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_EQ(0, BLI_ghash_size(ghash_SD));

	ghash_SD = BLI_ghash_symmetric_difference(NULL, NULL, NULL, NULL, ghash_SD, ghash_1, NULL);

	EXPECT_TRUE(BLI_ghash_isequal(ghash_SD, ghash_1));

	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_SD, NULL, NULL);
	ghash_2 = BLI_ghash_new(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, __func__);
	for (i = TESTCASE_SIZE / 2, k = &keys[i]; i < TESTCASE_SIZE; i++, k++) {
		/* Because values should have no effect at all here. */
		BLI_ghash_insert(ghash_2, SET_UINT_IN_POINTER(*k), SET_INT_IN_POINTER(i));
	}

	EXPECT_TRUE(BLI_ghash_isdisjoint(ghash_1, ghash_2));

	ghash_SD = BLI_ghash_symmetric_difference(NULL, NULL, NULL, NULL, NULL, ghash_1, ghash_2, NULL);

	EXPECT_EQ(TESTCASE_SIZE, BLI_ghash_size(ghash_SD));

	ghash_SD = BLI_ghash_symmetric_difference(NULL, NULL, NULL, NULL, ghash_SD, ghash_1, ghash_2, NULL);

	EXPECT_EQ(0, BLI_ghash_size(ghash_SD));

	BLI_ghash_free(ghash_1, NULL, NULL);
	BLI_ghash_free(ghash_2, NULL, NULL);
	BLI_ghash_free(ghash_SD, NULL, NULL);
}
