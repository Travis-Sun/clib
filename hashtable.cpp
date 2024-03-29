
/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Hash table
 *
 * The hash function used here is by Bob Jenkins, 1996:
 *    <http://burtleburtle.net/bob/hash/doobs.html>
 *       "By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.
 *       You may use this code any way you wish, private, educational,
 *       or commercial.  It's free."
 *
 * The rest of the file is licensed under the BSD license.  See LICENSE.
 */
#include "hashtable.h"
#include "hash.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* multi-thread support
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>


static pthread_cond_t maintenance_cond = PTHREAD_COND_INITIALIZER;
*/

typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */

/* how many powers of 2's worth of buckets we use */
unsigned int hashpower = HASHPOWER_DEFAULT;

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/* Main hash table. This is where we look except during expansion. */
static item** primary_hashtable = 0;

/*
 * Previous hash table. During expansion, we look here for keys that haven't
 * been moved over to the primary yet.
 */
static item** old_hashtable = 0;

/* Number of items in the hash table. */
static S_UINT hash_items = 0;

/* Flag: Are we in the middle of expanding now? */
static bool expanding = false;
static bool started_expanding = false;

/*
 * During expansion we migrate values with bucket granularity; this is how
 * far we've gotten so far. Ranges from 0 .. hashsize(hashpower - 1) - 1.
 */
static unsigned int expand_bucket = 0;

void hashtable_init(const int ht_init) {
    if (ht_init) {
        hashpower = ht_init;
    }
    primary_hashtable = (item**)calloc(hashsize(hashpower), sizeof(void *));
    if (! primary_hashtable) {
        fprintf(stderr, "Failed to init hashtable.\n");
        exit(EXIT_FAILURE);
    }        
    
    //STATS_LOCK();
    //stats.hash_power_level = hashpower;
    //stats.hash_bytes = hashsize(hashpower) * sizeof(void *);
    //STATS_UNLOCK();    
}

item *hashtable_find(const S_CHAR *key, const S_UINT nkey, const S_UINT32 hv) {
    item *it;
    unsigned int oldbucket;

    if (expanding &&
        (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
    {
        it = old_hashtable[oldbucket];
    } else {
        it = primary_hashtable[hv & hashmask(hashpower)];
    }

    item *ret = NULL;
    int depth = 0;
    while (it) {
        if ((nkey == it->nkey) && (memcmp(key, it->key, nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
        ++depth;
    }
    //MEMCACHED_ASSOC_FIND(key, nkey, depth);
    return ret;
}

/* returns the address of the item pointer before the key.  if *item == 0,
   the item wasn't found */

static item** _hashitem_before (const char *key, const size_t nkey, const uint32_t hv) {
    item **pos;
    unsigned int oldbucket;

    if (expanding &&
        (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
    {
        pos = &old_hashtable[oldbucket];
    } else {
        pos = &primary_hashtable[hv & hashmask(hashpower)];
    }

    while (*pos && ((nkey != (*pos)->nkey) || memcmp(key, (*pos)->key, nkey))) {
        pos = &(*pos)->h_next;
    }
    return pos;
}

/* grows the hashtable to the next power of 2. */
static void hashtable_expand(void) {
    old_hashtable = primary_hashtable;

    primary_hashtable = (item**)calloc(hashsize(hashpower + 1), sizeof(void *));
    if (primary_hashtable) {        
        hashpower++;
        expanding = true;
        expand_bucket = 0;
        
        //STATS_LOCK();
        /* stats.hash_power_level = hashpower; */
        /* stats.hash_bytes += hashsize(hashpower) * sizeof(void *); */
        /* stats.hash_is_expanding = 1; */
        //STATS_UNLOCK();
        
    } else {
        primary_hashtable = old_hashtable;
        /* Bad news, but we can keep running. */
    }
}

/*
  another thread to extend the hashtable
 */
static void hashtable_start_expand(void) {
    if (started_expanding)
        return;
    started_expanding = true;
    //pthread_cond_signal(&maintenance_cond);
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int hashtable_insert(item *it, const S_UINT32 hv) {
    unsigned int oldbucket;

//    assert(assoc_find(ITEM_key(it), it->nkey) == 0);  /* shouldn't have duplicately named things defined */

    if (expanding &&
        (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
    {
        it->h_next = old_hashtable[oldbucket];
        old_hashtable[oldbucket] = it;
    } else {
        it->h_next = primary_hashtable[hv & hashmask(hashpower)];
        primary_hashtable[hv & hashmask(hashpower)] = it;
    }

    hash_items++;
    if (! expanding && hash_items > (hashsize(hashpower) * 3) / 2) {
        hashtable_expand();
        //hashtable_start_expand();
    }

    //MEMCACHED_ASSOC_INSERT(ITEM_key(it), it->nkey, hash_items);
    return 1;
}

void hashtable_delete(const S_CHAR *key, const S_UINT nkey, const S_UINT32 hv) {
    item **before = _hashitem_before(key, nkey, hv);

    if (*before) {
        item *nxt;
        hash_items--;
        /* The DTrace probe cannot be triggered as the last instruction
         * due to possible tail-optimization by the compiler
         */
        //MEMCACHED_ASSOC_DELETE(key, nkey, hash_items);
        nxt = (*before)->h_next;
        (*before)->h_next = 0;   /* probably pointless, but whatever. */
        *before = nxt;
        return;
    }
    /* Note:  we never actually get here.  the callers don't delete things
       they can't find. */
    assert(*before != 0);
}


static volatile int do_run_maintenance_thread = 1;

#define DEFAULT_HASH_BULK_MOVE 1
int hash_bulk_move = DEFAULT_HASH_BULK_MOVE;

static void hashtable_expand_move() {
    int ii;
    for (ii = 0; ii < hash_bulk_move && expanding; ++ii) {
        item *it, *next;
        int bucket;

        for (it = old_hashtable[expand_bucket]; NULL != it; it = next) {
            next = it->h_next;

            bucket = hash(it->key, it->nkey, 0) & hashmask(hashpower);
            it->h_next = primary_hashtable[bucket];
            primary_hashtable[bucket] = it;
        }
        
        old_hashtable[expand_bucket] = NULL;

        expand_bucket++;
        if (expand_bucket == hashsize(hashpower - 1)) {
            expanding = false;
            free(old_hashtable);            
            fprintf(stderr, "Hash table expansion done\n");
        }
    }    
}
    


static void *hashtable_maintenance_thread(void *arg) {

    while (do_run_maintenance_thread) {
        int ii = 0;

        /* Lock the cache, and bulk move multiple buckets to the new
         * hash table. */
        //item_lock_global();
        //mutex_lock(&cache_lock);

        for (ii = 0; ii < hash_bulk_move && expanding; ++ii) {
            item *it, *next;
            int bucket;

            for (it = old_hashtable[expand_bucket]; NULL != it; it = next) {
                next = it->h_next;

                bucket = hash(it->key, it->nkey, 0) & hashmask(hashpower);
                it->h_next = primary_hashtable[bucket];
                primary_hashtable[bucket] = it;
            }

            old_hashtable[expand_bucket] = NULL;

            expand_bucket++;
            if (expand_bucket == hashsize(hashpower - 1)) {
                expanding = false;
                free(old_hashtable);
                /*
                STATS_LOCK();
                stats.hash_bytes -= hashsize(hashpower - 1) * sizeof(void *);
                stats.hash_is_expanding = 0;
                STATS_UNLOCK();
                if (settings.verbose > 1)
                    fprintf(stderr, "Hash table expansion done\n");
                */
            }
        }

        //mutex_unlock(&cache_lock);
        //item_unlock_global();

        // TODO        
        if (!expanding) {
            /* finished expanding. tell all threads to use fine-grained locks */
            //switch_item_lock_type(ITEM_LOCK_GRANULAR);
            //slabs_rebalancer_resume();
            /* We are done expanding.. just wait for next invocation */
            //mutex_lock(&cache_lock);
            //started_expanding = false;
            //pthread_cond_wait(&maintenance_cond, &cache_lock);
            /* Before doing anything, tell threads to use a global lock */
            /* mutex_unlock(&cache_lock); */
            /* slabs_rebalancer_pause(); */
            /* switch_item_lock_type(ITEM_LOCK_GLOBAL); */
            /* mutex_lock(&cache_lock); */
            /* hashtable_expand(); */
            /* mutex_unlock(&cache_lock); */
        }
    }
    return NULL;
}

//static pthread_t maintenance_tid;

int start_hashtable_maintenance_thread() {
    /* int ret; */
    /* char *env = getenv("MEMCACHED_HASH_BULK_MOVE"); */
    /* if (env != NULL) { */
    /*     hash_bulk_move = atoi(env); */
    /*     if (hash_bulk_move == 0) { */
    /*         hash_bulk_move = DEFAULT_HASH_BULK_MOVE; */
    /*     } */
    /* } */
    /* if ((ret = pthread_create(&maintenance_tid, NULL, */
    /*                           assoc_maintenance_thread, NULL)) != 0) { */
    /*     fprintf(stderr, "Can't create thread: %s\n", strerror(ret)); */
    /*     return -1; */
    /* } */
    return 0;
}

void stop_hashtable_maintenance_thread() {
    /* mutex_lock(&cache_lock); */
    /* do_run_maintenance_thread = 0; */
    /* pthread_cond_signal(&maintenance_cond); */
    /* mutex_unlock(&cache_lock); */

    /* Wait for the maintenance thread to stop */
    //pthread_join(maintenance_tid, NULL);
}


