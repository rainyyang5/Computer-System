#ifndef __CACHE_H__
#define __CACHE_H__

#include "csapp.h"

/* a cache line */
typedef struct cache_block {
    struct cache_block *next;   //point to the next node in linked list
    size_t object_size;         //size of the object in the block
    char *uri;
    char *object;
}cache_block;

/* the cache structure */
typedef struct cache {
    cache_block *start;     //point to the dummy head before the first block
    cache_block *end;       //point to the dummy end after the last block
    int size;               //cache size used to check if cache is full
    sem_t mutex;
}cache;

/* cache initiation */
cache *cache_init();

/*
 * look for the cache block with given uri in the cache pointed by cache_hdr
 * return the pointer to the block if cache hit
 * return NULL otherwise
 */
cache_block *cache_match(cache *cache_hdr, char *uri);

/*
 * insert an object to cache
 * eviction policy when cache is full: LRU (least recently used)
 */
void cache_insert(cache *cache_hdr, char *uri, char *object, size_t size);

#endif
