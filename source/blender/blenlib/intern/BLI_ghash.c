/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_ghash.c
 *  \ingroup bli
 *
 * A general (pointer -> pointer) chaining hash table
 * for 'Abstract Data Types' (known as an ADT Hash Table).
 *
 * \note edgehash.c is based on this, make sure they stay in sync.
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"  /* for intptr_t support */
#include "BLI_utildefines.h"
#include "BLI_hash_mm2a.h"
#include "BLI_mempool.h"
#include "BLI_ghash.h"
#include "BLI_strict_flags.h"

#define GHASH_USE_MODULO_BUCKETS

/* Also used by smallhash! */
const unsigned int hashsizes[] = {
	5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 
	268435459
};

#ifdef GHASH_USE_MODULO_BUCKETS
#  define GHASH_MAX_SIZE 27
#else
#  define GHASH_BUCKET_BIT_MIN 2
#  define GHASH_BUCKET_BIT_MAX 28  /* About 268M of buckets... */
#endif

/* internal flag to ensure sets values aren't used */
#ifndef NDEBUG
#  define GHASH_FLAG_IS_SET (1 << 8)
#  define IS_GHASH_ASSERT(gh) BLI_assert((gh->flag & GHASH_FLAG_IS_SET) == 0)
// #  define IS_GSET_ASSERT(gs) BLI_assert((gs->flag & GHASH_FLAG_IS_SET) != 0)
#else
#  define IS_GHASH_ASSERT(gh)
// #  define IS_GSET_ASSERT(eh)
#endif

#define GHASH_LIMIT_GROW(_nbkt) ((_nbkt) * 3) / 4
#define GHASH_LIMIT_SHRINK(_nbkt) ((_nbkt) * 3) / 16

/***/

/* WARNING! Keep in sync with ugly _gh_Entry in header!!! */
typedef struct Entry {
	struct Entry *next;

	unsigned int hash;
	void *key;
	void *val;  /* This pointer ***must*** remain the last one, since it is 'virtually removed' for gset. */
} Entry;

struct GHash {
	GHashHashFP hashfp;
	GHashCmpFP cmpfp;

	Entry **buckets;
	struct BLI_mempool *entrypool;
	unsigned int nbuckets;
	unsigned int limit_grow, limit_shrink;
#ifdef GHASH_USE_MODULO_BUCKETS
	unsigned int cursize, size_min;
#else
	unsigned int bucket_mask, bucket_bit, bucket_bit_min;
#endif

	unsigned int nentries;
	unsigned int flag;
};

/* -------------------------------------------------------------------- */
/* GHash API */

/** \name Internal Utility API
 * \{ */

/**
 * Get the full hash for a key.
 */
BLI_INLINE unsigned int ghash_keyhash(GHash *gh, const void *key)
{
	return gh->hashfp(key);
}

/**
 * Get the bucket-hash for an already-computed full hash.
 */
BLI_INLINE unsigned int ghash_bucket_hash(GHash *gh, const unsigned int full_hash)
{
#ifdef GHASH_USE_MODULO_BUCKETS
	return full_hash % gh->nbuckets;
#else
	return full_hash & gh->bucket_mask;
#endif
}

/**
 * Expand buckets to the next size up or down.
 */
BLI_INLINE void ghash_resize_buckets(GHash *gh, const unsigned int nbuckets)
{
	Entry **buckets_old = gh->buckets;
	Entry **buckets_new;
	const unsigned int nbuckets_old = gh->nbuckets;
	unsigned int i;
	Entry *e;

	BLI_assert((gh->nbuckets != nbuckets) || !gh->buckets);
//	printf("%s: %d -> %d\n", __func__, nbuckets_old, nbuckets);

	gh->nbuckets = nbuckets;
#ifdef GHASH_USE_MODULO_BUCKETS
#else
	gh->bucket_mask = nbuckets - 1;
#endif

	buckets_new = (Entry **)MEM_callocN(sizeof(*gh->buckets) * gh->nbuckets, __func__);

	if (buckets_old) {
		if (nbuckets > nbuckets_old) {
			for (i = 0; i < nbuckets_old; i++) {
				Entry *e_next;
				for (e = buckets_old[i]; e; e = e_next) {
					const unsigned bucket_hash = ghash_bucket_hash(gh, e->hash);
					e_next = e->next;
					e->next = buckets_new[bucket_hash];
					buckets_new[bucket_hash] = e;
				}
			}
		}
		else {
			for (i = 0; i < nbuckets_old; i++) {
#ifdef GHASH_USE_MODULO_BUCKETS
				Entry *e_next;
				for (e = buckets_old[i]; e; e = e_next) {
					const unsigned bucket_hash = ghash_bucket_hash(gh, e->hash);
					e_next = e->next;
					e->next = buckets_new[bucket_hash];
					buckets_new[bucket_hash] = e;
				}
#else
				/* No need to recompute hashes in this case, since our mask is just smaller, all items in old bucket i
				 * will go in same new bucket (i & new_mask)! */
				const unsigned bucket_hash = ghash_bucket_hash(gh, i);
				BLI_assert(bucket_hash == ghash_bucket_hash(gh, e->hash));
				for (e = buckets_old[i]; e && e->next; e = e->next);
				if (e) {
					e->next = buckets_new[bucket_hash];
					buckets_new[bucket_hash] = buckets_old[i];
				}
#endif
			}
		}
	}

	gh->buckets = buckets_new;
	if (buckets_old) {
		MEM_freeN(buckets_old);
	}
}

//#include "PIL_time.h"
//#include "PIL_time_utildefines.h"

/**
 * Check if the number of items in the GHash is large enough to require more buckets,
 * or small enough to require less buckets, and resize \a gh accordingly.
 */
BLI_INLINE void ghash_expand_buckets(GHash *gh, const unsigned int nentries, const bool user_defined)
{
	unsigned int new_nbuckets;

	if (LIKELY(gh->buckets && (nentries < gh->limit_grow) && (nentries > gh->limit_shrink))) {
		return;
	}

	new_nbuckets = gh->nbuckets;

	while ((nentries > gh->limit_grow) &&
#ifdef GHASH_USE_MODULO_BUCKETS
	       (gh->cursize < GHASH_MAX_SIZE - 1))
	{
		new_nbuckets = hashsizes[++gh->cursize];
#else
	       (gh->bucket_bit < GHASH_BUCKET_BIT_MAX))
	{
		new_nbuckets = 1u << ++gh->bucket_bit;
#endif
		gh->limit_grow = GHASH_LIMIT_GROW(new_nbuckets);
	}
	if (gh->flag & GHASH_FLAG_ALLOW_SHRINK) {
		while ((nentries < gh->limit_shrink) &&
#ifdef GHASH_USE_MODULO_BUCKETS
			   (gh->cursize > gh->size_min))
		{
			new_nbuckets = hashsizes[--gh->cursize];
#else
			  (gh->bucket_bit > gh->bucket_bit_min))
		{
			new_nbuckets = 1u << --gh->bucket_bit;
#endif
			gh->limit_shrink = GHASH_LIMIT_SHRINK(new_nbuckets);
		}
	}

	if (user_defined) {
#ifdef GHASH_USE_MODULO_BUCKETS
		gh->size_min = gh->cursize;
#else
		gh->bucket_bit_min = gh->bucket_bit;
#endif
	}

	if ((new_nbuckets == gh->nbuckets) && gh->buckets) {
		return;
	}

	gh->limit_grow = GHASH_LIMIT_GROW(new_nbuckets);
	gh->limit_shrink = GHASH_LIMIT_SHRINK(new_nbuckets);
//	TIMEIT_BENCH(ghash_resize_buckets(gh, new_nbuckets), ghash_resize_buckets);
	ghash_resize_buckets(gh, new_nbuckets);
}

/**
 * Clear and reset \a gh buckets, reserve again buckets for given number of entries.
 */
BLI_INLINE void ghash_buckets_reset(GHash *gh, const unsigned int nentries)
{
	MEM_SAFE_FREE(gh->buckets);

#ifdef GHASH_USE_MODULO_BUCKETS
	gh->cursize = 0;
	gh->size_min = 0;
	gh->nbuckets = hashsizes[gh->cursize];
#else
	gh->bucket_bit = GHASH_BUCKET_BIT_MIN;
	gh->bucket_bit_min = GHASH_BUCKET_BIT_MIN;
	gh->nbuckets = 1u << gh->bucket_bit;
	gh->bucket_mask = gh->nbuckets - 1;
#endif

	gh->limit_grow = GHASH_LIMIT_GROW(gh->nbuckets);
	gh->limit_shrink = GHASH_LIMIT_SHRINK(gh->nbuckets);

	gh->nentries = 0;
	gh->flag = 0;

	ghash_expand_buckets(gh, nentries, (nentries != 0));
}

/**
 * Internal lookup function.
 * Takes hash and bucket_hash arguments to avoid calling #ghash_keyhash and #ghash_bucket_hash multiple times.
 */
BLI_INLINE Entry *ghash_lookup_entry_ex(
        GHash *gh, const void *key, const unsigned int hash, const unsigned int bucket_hash)
{
	Entry *e;

	for (e = gh->buckets[bucket_hash]; e; e = e->next) {
		if (UNLIKELY(e->hash == hash)) {
			if (LIKELY(gh->cmpfp(key, e->key) == false)) {
				return e;
			}
		}
	}
	return NULL;
}

/**
 * Internal lookup function. Only wraps #ghash_lookup_entry_ex
 */
BLI_INLINE Entry *ghash_lookup_entry(GHash *gh, const void *key)
{
	const unsigned int hash = ghash_keyhash(gh, key);
	const unsigned int bucket_hash = ghash_bucket_hash(gh, hash);
	return ghash_lookup_entry_ex(gh, key, hash, bucket_hash);
}

static GHash *ghash_new(GHashHashFP hashfp, GHashCmpFP cmpfp, const char *info,
                        const unsigned int nentries_reserve,
                        const unsigned int entry_size)
{
	GHash *gh = MEM_mallocN(sizeof(*gh), info);

	gh->hashfp = hashfp;
	gh->cmpfp = cmpfp;

	gh->buckets = NULL;
	ghash_buckets_reset(gh, nentries_reserve);
	gh->entrypool = BLI_mempool_create(entry_size, 64, 64, BLI_MEMPOOL_NOP);

	return gh;
}

/**
 * Internal insert function.
 * Takes hash and bucket_hash arguments to avoid calling #ghash_keyhash and #ghash_bucket_hash multiple times.
 */
BLI_INLINE void ghash_insert_ex(
        GHash *gh, void *key, void *val, const unsigned int hash, const unsigned int bucket_hash)
{
	Entry *e = (Entry *)BLI_mempool_alloc(gh->entrypool);
	BLI_assert((gh->flag & GHASH_FLAG_ALLOW_DUPES) || (BLI_ghash_haskey(gh, key) == 0));
	IS_GHASH_ASSERT(gh);

	e->next = gh->buckets[bucket_hash];
	e->hash = hash;
	e->key = key;
	e->val = val;
	gh->buckets[bucket_hash] = e;

	ghash_expand_buckets(gh, ++gh->nentries, false);
}

/**
 * Insert function that doesn't set the value (use for GSet)
 */
BLI_INLINE void ghash_insert_ex_keyonly(
        GHash *gh, void *key, const unsigned int hash, const unsigned int bucket_hash)
{
	Entry *e = (Entry *)BLI_mempool_alloc(gh->entrypool);
	BLI_assert((gh->flag & GHASH_FLAG_ALLOW_DUPES) || (BLI_ghash_haskey(gh, key) == 0));

	e->next = gh->buckets[bucket_hash];
	e->hash = hash;
	e->key = key;
	/* intentionally leave value unset */
	gh->buckets[bucket_hash] = e;

	ghash_expand_buckets(gh, ++gh->nentries, false);
}

BLI_INLINE void ghash_insert(GHash *gh, void *key, void *val)
{
	const unsigned int hash = ghash_keyhash(gh, key);
	const unsigned int bucket_hash = ghash_bucket_hash(gh, hash);
	ghash_insert_ex(gh, key, val, hash, bucket_hash);
}

/**
 * Remove the entry and return it, caller must free from gh->entrypool.
 */
static Entry *ghash_remove_ex(
        GHash *gh, void *key, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp,
        const unsigned int hash, const unsigned int bucket_hash)
{
	Entry *e;
	Entry *e_prev = NULL;

	for (e = gh->buckets[bucket_hash]; e; e = e->next) {
		if (UNLIKELY(e->hash == hash)) {
			if (LIKELY(gh->cmpfp(key, e->key) == false)) {
				Entry *e_next = e->next;

				if (keyfreefp) keyfreefp(e->key);
				if (valfreefp) valfreefp(e->val);

				if (e_prev) e_prev->next = e_next;
				else   gh->buckets[bucket_hash] = e_next;

				ghash_expand_buckets(gh, --gh->nentries, false);
				return e;
			}
		}
		e_prev = e;
	}

	return NULL;
}

/**
 * Run free callbacks for freeing entries.
 */
static void ghash_free_cb(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
	unsigned int i;

	BLI_assert(keyfreefp || valfreefp);

	for (i = 0; i < gh->nbuckets; i++) {
		Entry *e;

		for (e = gh->buckets[i]; e; ) {
			Entry *e_next = e->next;

			if (keyfreefp) keyfreefp(e->key);
			if (valfreefp) valfreefp(e->val);

			e = e_next;
		}
	}
}
/** \} */


/** \name Public API
 * \{ */

/**
 * Creates a new, empty GHash.
 *
 * \param hashfp  Hash callback.
 * \param cmpfp  Comparison callback.
 * \param info  Identifier string for the GHash.
 * \param nentries_reserve  Optionally reserve the number of members that the hash will hold.
 * Use this to avoid resizing buckets if the size is known or can be closely approximated.
 * \return  An empty GHash.
 */
GHash *BLI_ghash_new_ex(GHashHashFP hashfp, GHashCmpFP cmpfp, const char *info,
                        const unsigned int nentries_reserve)
{
	return ghash_new(hashfp, cmpfp, info,
	                 nentries_reserve,
	                 (unsigned int)sizeof(Entry));
}

/**
 * Wraps #BLI_ghash_new_ex with zero entries reserved.
 */
GHash *BLI_ghash_new(GHashHashFP hashfp, GHashCmpFP cmpfp, const char *info)
{
	return BLI_ghash_new_ex(hashfp, cmpfp, info, 0);
}

/**
 * Reverve given ammount of entries (resize \a gh accordingly if needed).
 */
void BLI_ghash_reserve(GHash *gh, const unsigned int nentries_reserve)
{
	ghash_expand_buckets(gh, nentries_reserve, true);
}

/**
 * \return size of the GHash.
 */
int BLI_ghash_size(GHash *gh)
{
	return (int)gh->nentries;
}

/**
 * Insert a key/value pair into the \a gh.
 *
 * \note Duplicates are not checked,
 * the caller is expected to ensure elements are unique unless
 * GHASH_FLAG_ALLOW_DUPES flag is set.
 */
void BLI_ghash_insert(GHash *gh, void *key, void *val)
{
	ghash_insert(gh, key, val);
}

/**
 * Like #BLI_ghash_insert, but does nothing in case \a key is already in the \a gh.
 *
 * Avoids #BLI_ghash_haskey, #BLI_ghash_insert calls (double lookups)
 *
 * \returns true if a new key has been added.
 */
bool BLI_ghash_add(GHash *gh, void *key, void *val)
{
	const unsigned int hash = ghash_keyhash(gh, key);
	const unsigned int bucket_hash = ghash_bucket_hash(gh, hash);
	Entry *e = ghash_lookup_entry_ex(gh, key, hash, bucket_hash);
	if (e) {
		return false;
	}
	else {
		ghash_insert_ex(gh, key, val, hash, bucket_hash);
		return true;
	}
}

/**
 * Inserts a new value to a key that may already be in ghash.
 *
 * Avoids #BLI_ghash_remove, #BLI_ghash_insert calls (double lookups)
 *
 * \returns true if a new key has been added.
 */
bool BLI_ghash_reinsert(GHash *gh, void *key, void *val, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
	const unsigned int hash = ghash_keyhash(gh, key);
	const unsigned int bucket_hash = ghash_bucket_hash(gh, hash);
	Entry *e = ghash_lookup_entry_ex(gh, key, hash, bucket_hash);
	if (e) {
		if (keyfreefp) keyfreefp(e->key);
		if (valfreefp) valfreefp(e->val);
		e->key = key;
		e->val = val;
		return false;
	}
	else {
		ghash_insert_ex(gh, key, val, hash, bucket_hash);
		return true;
	}
}

/**
 * Lookup the value of \a key in \a gh.
 *
 * \param key  The key to lookup.
 * \returns the value for \a key or NULL.
 *
 * \note When NULL is a valid value, use #BLI_ghash_lookup_p to differentiate a missing key
 * from a key with a NULL value. (Avoids calling #BLI_ghash_haskey before #BLI_ghash_lookup)
 */
void *BLI_ghash_lookup(GHash *gh, const void *key)
{
	Entry *e = ghash_lookup_entry(gh, key);
	IS_GHASH_ASSERT(gh);
	return e ? e->val : NULL;
}

/**
 * A version of #BLI_ghash_lookup which accepts a fallback argument.
 */
void *BLI_ghash_lookup_default(GHash *gh, const void *key, void *val_default)
{
	Entry *e = ghash_lookup_entry(gh, key);
	IS_GHASH_ASSERT(gh);
	return e ? e->val : val_default;
}

/**
 * Lookup a pointer to the value of \a key in \a gh.
 *
 * \param key  The key to lookup.
 * \returns the pointer to value for \a key or NULL.
 *
 * \note This has 2 main benefits over #BLI_ghash_lookup.
 * - A NULL return always means that \a key isn't in \a gh.
 * - The value can be modified in-place without further function calls (faster).
 */
void **BLI_ghash_lookup_p(GHash *gh, const void *key)
{
	Entry *e = ghash_lookup_entry(gh, key);
	IS_GHASH_ASSERT(gh);
	return e ? &e->val : NULL;
}

/**
 * Remove \a key from \a gh, or return false if the key wasn't found.
 *
 * \param key  The key to remove.
 * \param keyfreefp  Optional callback to free the key.
 * \param valfreefp  Optional callback to free the value.
 * \return true if \a key was removed from \a gh.
 */
bool BLI_ghash_remove(GHash *gh, void *key, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
	const unsigned int hash = ghash_keyhash(gh, key);
	const unsigned int bucket_hash = ghash_bucket_hash(gh, hash);
	Entry *e = ghash_remove_ex(gh, key, keyfreefp, valfreefp, hash, bucket_hash);
	if (e) {
		BLI_mempool_free(gh->entrypool, e);
		return true;
	}
	else {
		return false;
	}
}

/* same as above but return the value,
 * no free value argument since it will be returned */
/**
 * Remove \a key from \a gh, returning the value or NULL if the key wasn't found.
 *
 * \param key  The key to remove.
 * \param keyfreefp  Optional callback to free the key.
 * \return the value of \a key int \a gh or NULL.
 */
void *BLI_ghash_popkey(GHash *gh, void *key, GHashKeyFreeFP keyfreefp)
{
	const unsigned int hash = ghash_keyhash(gh, key);
	const unsigned int bucket_hash = ghash_bucket_hash(gh, hash);
	Entry *e = ghash_remove_ex(gh, key, keyfreefp, NULL, hash, bucket_hash);
	IS_GHASH_ASSERT(gh);
	if (e) {
		void *val = e->val;
		BLI_mempool_free(gh->entrypool, e);
		return val;
	}
	else {
		return NULL;
	}
}

/**
 * \return true if the \a key is in \a gh.
 */
bool BLI_ghash_haskey(GHash *gh, const void *key)
{
	return (ghash_lookup_entry(gh, key) != NULL);
}

/**
 * Reset \a gh clearing all entries.
 *
 * \param keyfreefp  Optional callback to free the key.
 * \param valfreefp  Optional callback to free the value.
 * \param nentries_reserve  Optionally reserve the number of members that the hash will hold.
 */
void BLI_ghash_clear_ex(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp,
                        const unsigned int nentries_reserve)
{
	if (keyfreefp || valfreefp)
		ghash_free_cb(gh, keyfreefp, valfreefp);

	ghash_buckets_reset(gh, nentries_reserve);
	BLI_mempool_clear_ex(gh->entrypool, nentries_reserve ? (int)nentries_reserve : -1);
}

/**
 * Wraps #BLI_ghash_clear_ex with zero entries reserved.
 */
void BLI_ghash_clear(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
	BLI_ghash_clear_ex(gh, keyfreefp, valfreefp, 0);
}

/**
 * Frees the GHash and its members.
 *
 * \param gh  The GHash to free.
 * \param keyfreefp  Optional callback to free the key.
 * \param valfreefp  Optional callback to free the value.
 */
void BLI_ghash_free(GHash *gh, GHashKeyFreeFP keyfreefp, GHashValFreeFP valfreefp)
{
	BLI_assert((int)gh->nentries == BLI_mempool_count(gh->entrypool));
	if (keyfreefp || valfreefp)
		ghash_free_cb(gh, keyfreefp, valfreefp);

	MEM_freeN(gh->buckets);
	BLI_mempool_destroy(gh->entrypool);
	MEM_freeN(gh);
}

/**
 * Sets a GHash flag.
 */
void BLI_ghash_flag_set(GHash *gh, unsigned int flag)
{
	gh->flag |= flag;
}

/**
 * Clear a GHash flag.
 */
void BLI_ghash_flag_clear(GHash *gh, unsigned int flag)
{
	gh->flag &= ~flag;
}

/** \} */


/* -------------------------------------------------------------------- */
/* GHash Iterator API */

/** \name Iterator API
 * \{ */

/**
 * Create a new GHashIterator. The hash table must not be mutated
 * while the iterator is in use, and the iterator will step exactly
 * BLI_ghash_size(gh) times before becoming done.
 *
 * \param gh The GHash to iterate over.
 * \return Pointer to a new DynStr.
 */
GHashIterator *BLI_ghashIterator_new(GHash *gh)
{
	GHashIterator *ghi = MEM_mallocN(sizeof(*ghi), "ghash iterator");
	BLI_ghashIterator_init(ghi, gh);
	return ghi;
}

/**
 * Init an already allocated GHashIterator. The hash table must not
 * be mutated while the iterator is in use, and the iterator will
 * step exactly BLI_ghash_size(gh) times before becoming done.
 *
 * \param ghi The GHashIterator to initialize.
 * \param gh The GHash to iterate over.
 */
void BLI_ghashIterator_init(GHashIterator *ghi, GHash *gh)
{
	ghi->gh = gh;
	ghi->curEntry = NULL;
	ghi->curBucket = UINT_MAX;  /* wraps to zero */
	if (gh->nentries) {
		do {
			ghi->curBucket++;
			if (UNLIKELY(ghi->curBucket == ghi->gh->nbuckets))
				break;
			ghi->curEntry = ghi->gh->buckets[ghi->curBucket];
		} while (!ghi->curEntry);
	}
}

/**
 * Steps the iterator to the next index.
 *
 * \param ghi The iterator.
 */
void BLI_ghashIterator_step(GHashIterator *ghi)
{
	if (ghi->curEntry) {
		ghi->curEntry = ghi->curEntry->next;
		while (!ghi->curEntry) {
			ghi->curBucket++;
			if (ghi->curBucket == ghi->gh->nbuckets)
				break;
			ghi->curEntry = ghi->gh->buckets[ghi->curBucket];
		}
	}
}

/**
 * Free a GHashIterator.
 *
 * \param ghi The iterator to free.
 */
void BLI_ghashIterator_free(GHashIterator *ghi)
{
	MEM_freeN(ghi);
}

/* inline functions now */
#if 0
/**
 * Retrieve the key from an iterator.
 *
 * \param ghi The iterator.
 * \return The key at the current index, or NULL if the
 * iterator is done.
 */
void *BLI_ghashIterator_getKey(GHashIterator *ghi)
{
	return ghi->curEntry->key;
}

/**
 * Retrieve the value from an iterator.
 *
 * \param ghi The iterator.
 * \return The value at the current index, or NULL if the
 * iterator is done.
 */
void *BLI_ghashIterator_getValue(GHashIterator *ghi)
{
	return ghi->curEntry->val;
}

/**
 * Retrieve the value from an iterator.
 *
 * \param ghi The iterator.
 * \return The value at the current index, or NULL if the
 * iterator is done.
 */
void **BLI_ghashIterator_getValue_p(GHashIterator *ghi)
{
	return &ghi->curEntry->val;
}

/**
 * Determine if an iterator is done (has reached the end of
 * the hash table).
 *
 * \param ghi The iterator.
 * \return True if done, False otherwise.
 */
bool BLI_ghashIterator_done(GHashIterator *ghi)
{
	return ghi->curEntry == NULL;
}
#endif

/** \} */


/** \name Generic Key Hash & Comparison Functions
 * \{ */

/***/

#if 0
/* works but slower */
unsigned int BLI_ghashutil_ptrhash(const void *key)
{
	return (unsigned int)(intptr_t)key;
}
#else
/* based python3.3's pointer hashing function */
unsigned int BLI_ghashutil_ptrhash(const void *key)
{
	size_t y = (size_t)key;
	/* bottom 3 or 4 bits are likely to be 0; rotate y by 4 to avoid
	 * excessive hash collisions for dicts and sets */
	y = (y >> 4) | (y << (8 * sizeof(void *) - 4));
	return (unsigned int)y;
}
#endif
bool BLI_ghashutil_ptrcmp(const void *a, const void *b)
{
	return (a != b);
}

unsigned int BLI_ghashutil_uinthash_v4(const unsigned int key[4])
{
	unsigned int hash;
	hash  = key[0];
	hash *= 37;
	hash += key[1];
	hash *= 37;
	hash += key[2];
	hash *= 37;
	hash += key[3];
	return hash;
}
unsigned int BLI_ghashutil_uinthash_v4_murmur(const unsigned int key[4])
{
	return BLI_hash_mm2((const unsigned char *)key, sizeof(key), 0);
}

bool BLI_ghashutil_uinthash_v4_cmp(const void *a, const void *b)
{
	return (memcmp(a, b, sizeof(unsigned int[4])) != 0);
}

unsigned int BLI_ghashutil_uinthash(unsigned int key)
{
	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

	return key;
}

unsigned int BLI_ghashutil_inthash_p(const void *ptr)
{
	uintptr_t key = (uintptr_t)ptr;

	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

	return (unsigned int)(key & 0xffffffff);
}

unsigned int BLI_ghashutil_inthash_p_murmur(const void *ptr)
{
	uintptr_t key = (uintptr_t)ptr;

	return BLI_hash_mm2((const unsigned char *)&key, sizeof(key), 0);
}

bool BLI_ghashutil_intcmp(const void *a, const void *b)
{
	return (a != b);
}

/**
 * This function implements the widely used "djb" hash apparently posted
 * by Daniel Bernstein to comp.lang.c some time ago.  The 32 bit
 * unsigned hash value starts at 5381 and for each byte 'c' in the
 * string, is updated: ``hash = hash * 33 + c``.  This
 * function uses the signed value of each byte.
 *
 * note: this is the same hash method that glib 2.34.0 uses.
 */
unsigned int BLI_ghashutil_strhash_n(const char *key, size_t n)
{
	const signed char *p;
	unsigned int h = 5381;

	for (p = (const signed char *)key; n-- && *p != '\0'; p++) {
		h = (h << 5) + h + (unsigned int)*p;
	}

	return h;
}
unsigned int BLI_ghashutil_strhash_p(const void *ptr)
{
	const signed char *p;
	unsigned int h = 5381;

	for (p = ptr; *p != '\0'; p++) {
		h = (h << 5) + h + (unsigned int)*p;
	}

	return h;
}
unsigned int BLI_ghashutil_strhash_p_murmur(const void *ptr)
{
	const unsigned char *key = ptr;

	return BLI_hash_mm2(key, strlen((const char *)key) + 1, 0);
}
bool BLI_ghashutil_strcmp(const void *a, const void *b)
{
	return (!STREQ(a, b));
}

GHashPair *BLI_ghashutil_pairalloc(const void *first, const void *second)
{
	GHashPair *pair = MEM_mallocN(sizeof(GHashPair), "GHashPair");
	pair->first = first;
	pair->second = second;
	return pair;
}

unsigned int BLI_ghashutil_pairhash(const void *ptr)
{
	const GHashPair *pair = ptr;
	unsigned int hash = BLI_ghashutil_ptrhash(pair->first);
	return hash ^ BLI_ghashutil_ptrhash(pair->second);
}

bool BLI_ghashutil_paircmp(const void *a, const void *b)
{
	const GHashPair *A = a;
	const GHashPair *B = b;

	return (BLI_ghashutil_ptrcmp(A->first, B->first) ||
	        BLI_ghashutil_ptrcmp(A->second, B->second));
}

void BLI_ghashutil_pairfree(void *ptr)
{
	MEM_freeN(ptr);
}

/** \} */


/** \name Convenience GHash Creation Functions
 * \{ */

GHash *BLI_ghash_ptr_new_ex(const char *info,
                            const unsigned int nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, info,
	                        nentries_reserve);
}
GHash *BLI_ghash_ptr_new(const char *info)
{
	return BLI_ghash_ptr_new_ex(info, 0);
}

GHash *BLI_ghash_str_new_ex(const char *info,
                            const unsigned int nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, info,
	                        nentries_reserve);
}
GHash *BLI_ghash_str_new(const char *info)
{
	return BLI_ghash_str_new_ex(info, 0);
}

GHash *BLI_ghash_int_new_ex(const char *info,
                            const unsigned int nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_inthash_p, BLI_ghashutil_intcmp, info,
	                        nentries_reserve);
}
GHash *BLI_ghash_int_new(const char *info)
{
	return BLI_ghash_int_new_ex(info, 0);
}

GHash *BLI_ghash_pair_new_ex(const char *info,
                             const unsigned int nentries_reserve)
{
	return BLI_ghash_new_ex(BLI_ghashutil_pairhash, BLI_ghashutil_paircmp, info,
	                        nentries_reserve);
}
GHash *BLI_ghash_pair_new(const char *info)
{
	return BLI_ghash_pair_new_ex(info, 0);
}

/** \} */


/* -------------------------------------------------------------------- */
/* GSet API */

/* Use ghash API to give 'set' functionality */

/* TODO: typical set functions
 * isdisjoint/issubset/issuperset/union/intersection/difference etc */

/** \name GSet Functions
 * \{ */
GSet *BLI_gset_new_ex(GSetHashFP hashfp, GSetCmpFP cmpfp, const char *info,
                      const unsigned int nentries_reserve)
{
	GSet *gs = (GSet *)ghash_new(hashfp, cmpfp, info,
	                             nentries_reserve,
	                             sizeof(Entry) - sizeof(void *));
#ifndef NDEBUG
	((GHash *)gs)->flag |= GHASH_FLAG_IS_SET;
#endif
	return gs;
}

GSet *BLI_gset_new(GSetHashFP hashfp, GSetCmpFP cmpfp, const char *info)
{
	return BLI_gset_new_ex(hashfp, cmpfp, info, 0);
}

int BLI_gset_size(GSet *gs)
{
	return (int)((GHash *)gs)->nentries;
}

/**
 * Adds the key to the set (no checks for unique keys!).
 * Matching #BLI_ghash_insert
 */
void BLI_gset_insert(GSet *gs, void *key)
{
	const unsigned int hash = ghash_keyhash((GHash *)gs, key);
	const unsigned int bucket_hash = ghash_bucket_hash((GHash *)gs, hash);
	ghash_insert_ex_keyonly((GHash *)gs, key, hash, bucket_hash);
}

/**
 * A version of BLI_gset_insert which checks first if the key is in the set.
 * \returns true if a new key has been added.
 *
 * \note GHash has no equivalent to this because typically the value would be different.
 */
bool BLI_gset_add(GSet *gs, void *key)
{
	const unsigned int hash = ghash_keyhash((GHash *)gs, key);
	const unsigned int bucket_hash = ghash_bucket_hash((GHash *)gs, hash);
	Entry *e = ghash_lookup_entry_ex((GHash *)gs, key, hash, bucket_hash);
	if (e) {
		return false;
	}
	else {
		ghash_insert_ex_keyonly((GHash *)gs, key, hash, bucket_hash);
		return true;
	}
}

/**
 * Adds the key to the set (duplicates are managed).
 * Matching #BLI_ghash_reinsert
 *
 * \returns true if a new key has been added.
 */
bool BLI_gset_reinsert(GSet *gs, void *key, GSetKeyFreeFP keyfreefp)
{
	const unsigned int hash = ghash_keyhash((GHash *)gs, key);
	const unsigned int bucket_hash = ghash_bucket_hash((GHash *)gs, hash);
	Entry *e = ghash_lookup_entry_ex((GHash *)gs, key, hash, bucket_hash);
	if (e) {
		if (keyfreefp) keyfreefp(e->key);
		e->key = key;
		return false;
	}
	else {
		ghash_insert_ex_keyonly((GHash *)gs, key, hash, bucket_hash);
		return true;
	}
}

bool BLI_gset_remove(GSet *gs, void *key, GSetKeyFreeFP keyfreefp)
{
	return BLI_ghash_remove((GHash *)gs, key, keyfreefp, NULL);
}


bool BLI_gset_haskey(GSet *gs, const void *key)
{
	return (ghash_lookup_entry((GHash *)gs, key) != NULL);
}

void BLI_gset_clear_ex(GSet *gs, GSetKeyFreeFP keyfreefp,
                       const unsigned int nentries_reserve)
{
	BLI_ghash_clear_ex((GHash *)gs, keyfreefp, NULL,
	                   nentries_reserve);
}

void BLI_gset_clear(GSet *gs, GSetKeyFreeFP keyfreefp)
{
	BLI_ghash_clear((GHash *)gs, keyfreefp, NULL);
}

void BLI_gset_free(GSet *gs, GSetKeyFreeFP keyfreefp)
{
	BLI_ghash_free((GHash *)gs, keyfreefp, NULL);
}

void BLI_gset_flag_set(GSet *gs, unsigned int flag)
{
	((GHash *)gs)->flag |= flag;
}

void BLI_gset_flag_clear(GSet *gs, unsigned int flag)
{
	((GHash *)gs)->flag &= ~flag;
}

/** \} */


/** \name Convenience GSet Creation Functions
 * \{ */

GSet *BLI_gset_ptr_new_ex(const char *info,
                          const unsigned int nentries_reserve)
{
	return BLI_gset_new_ex(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, info,
	                       nentries_reserve);
}
GSet *BLI_gset_ptr_new(const char *info)
{
	return BLI_gset_ptr_new_ex(info, 0);
}

GSet *BLI_gset_pair_new_ex(const char *info,
                             const unsigned int nentries_reserve)
{
	return BLI_gset_new_ex(BLI_ghashutil_pairhash, BLI_ghashutil_paircmp, info,
	                        nentries_reserve);
}
GSet *BLI_gset_pair_new(const char *info)
{
	return BLI_gset_pair_new_ex(info, 0);
}

/** \} */


/** \name Debugging & Introspection
 * \{ */

#include "BLI_math.h"

/**
 * \return number of buckets in the GHash.
 */
int BLI_ghash_buckets_size(GHash *gh)
{
#ifdef GHASH_USE_MODULO_BUCKETS
	return (int)hashsizes[gh->cursize];
#else
	return 1 << gh->bucket_bit;
#endif
}
int BLI_gset_buckets_size(GSet *gs)
{
	return BLI_ghash_buckets_size((GHash *)gs);
}

/**
 * Measure how well the hash function performs (1.0 is approx as good as random distribution),
 * and return a few other stats like load, variance of the distribution of the entries in the buckets, etc.
 *
 * Smaller is better!
 */
double BLI_ghash_calc_quality(
        GHash *gh, double *r_load, double *r_variance,
        double *r_prop_empty_buckets, double *r_prop_overloaded_buckets, int *r_biggest_bucket)
{
	double mean;
	unsigned int i;

	if (gh->nentries == 0) {
		if (r_load) {
			*r_load = 0.0;
		}
		if (r_variance) {
			*r_variance = 0.0;
		}
		if (r_prop_empty_buckets) {
			*r_prop_empty_buckets = 1.0;
		}
		if (r_prop_overloaded_buckets) {
			*r_prop_overloaded_buckets = 0.0;
		}
		if (r_biggest_bucket) {
			*r_biggest_bucket = 0;
		}

		return 0.0;
	}

	mean = (double)gh->nentries / (double)gh->nbuckets;
	if (r_load) {
		*r_load = mean;
	}
	if (r_biggest_bucket) {
		*r_biggest_bucket = 0;
	}

	if (r_variance) {
		/* We already know our mean (i.e. load factor), easy to compute variance.
		 * See http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Two-pass_algorithm
		 */
		double sum = 0.0;
		for (i = 0; i < gh->nbuckets; i++) {
			int count = 0;
			Entry *e;
			for (e = gh->buckets[i]; e; e = e->next) {
				count++;
			}
			sum += ((double)count - mean) * ((double)count - mean);
		}
		*r_variance = sum / (double)(gh->nbuckets - 1);
	}

	{
	   uint64_t sum = 0;
	   uint64_t overloaded_buckets_threshold = (uint64_t)max_ii(GHASH_LIMIT_GROW(1), 1);
	   uint64_t sum_overloaded = 0;
	   uint64_t sum_empty = 0;

	   for (i = 0; i < gh->nbuckets; i++) {
		   uint64_t count = 0;
		   Entry *e;
		   for (e = gh->buckets[i]; e; e = e->next) {
			   count++;
		   }
		   if (r_biggest_bucket) {
			   *r_biggest_bucket = max_ii(*r_biggest_bucket, (int)count);
		   }
		   if (r_prop_overloaded_buckets && (count > overloaded_buckets_threshold)) {
			   sum_overloaded++;
		   }
		   if (r_prop_empty_buckets && !count) {
			   sum_empty++;
		   }
		   sum += count * (count + 1);
	   }
	   if (r_prop_overloaded_buckets) {
		   *r_prop_overloaded_buckets = (double)sum_overloaded / (double)gh->nbuckets;
	   }
	   if (r_prop_empty_buckets) {
		   *r_prop_empty_buckets = (double)sum_empty / (double)gh->nbuckets;
	   }
	   return ((double)sum * (double)gh->nbuckets /
			   ((double)gh->nentries * (gh->nentries + 2 * gh->nbuckets - 1)));
   }
}
double BLI_gset_calc_quality(GSet *gs, double *r_load, double *r_variance, double *r_prop_empty_buckets, double *r_prop_overloaded_buckets, int *r_biggest_bucket)
{
	return BLI_ghash_calc_quality((GHash *)gs, r_load, r_variance, r_prop_empty_buckets, r_prop_overloaded_buckets, r_biggest_bucket);
}

/** \} */
