#include "csapp.h"
#include "cache.h"

#define MAX_CACHE_SIZE (1 << 20)
#define MAX_OBJECT_SIZE 102400


/* cache initiation */
cache *cache_init() {
    cache *temp = malloc(sizeof(cache));
    cache_block *init_block = malloc(sizeof(cache_block));
    temp->start = init_block;
    temp->end = init_block;
    temp->size = 0;
    sem_init(&(temp->mutex), 0, 1);
    return temp;
}

/* add the cache block to the end of the linked list */
void cache_most_recent(cache *cache_hdr, cache_block *block) {
    cache_hdr->size += block->object_size;

    /* put the block info into the old end block */
    cache_hdr->end->uri = block->uri;
    cache_hdr->end->object = block->object;
    cache_hdr->end->object_size = block->object_size;

    /* use the old block as the end block */
    cache_hdr->end->next = block;
    cache_hdr->end = block;
    return;
}

/* evict the least recently used block if the cache is full */
void cache_evict(cache *cache_hdr) {
    cache_block *start = cache_hdr->start;
    cache_hdr->size -= start->object_size;

    cache_hdr->start = cache_hdr->start->next;
    free(start);

    return;
}

/* delete a cache block from the cache */
void cache_delete(cache *cache_hdr, cache_block *block) {
    cache_hdr->size -= block->object_size;

    if (cache_hdr->start->next == cache_hdr->end) {
        /* if the cache contains only one block */
        cache_hdr->end = cache_hdr->start;
    }
    else {
        /* the node is in the middle */
        block->uri = block->next->uri;
        block->object_size = block->next->object_size;
        block->object = block->next->object;
        free(block->next);
        block->next = block->next->next;
    }
    return;
}

/*
 * look for the cache block with given uri in the cache pointed by cache_hdr
 * return the pointer to the block if cache hit
 * return NULL otherwise
 */
cache_block *cache_match(cache *cache_hdr, char *uri) {
    /* using mutex and semaphors to prevent race conditions */
    P(&(cache_hdr->mutex));

    cache_block *ptr;

    for (ptr = cache_hdr->start; ptr != cache_hdr->end; ptr = ptr->next) {
        /* go through each node in the linked list */
        if (!strcmp(uri, ptr->uri)) {
            /* object matches - save the node info into a temp block */
            cache_block *temp = malloc(sizeof(cache_block));
            temp->uri = ptr->uri;
            temp->object_size = ptr->object_size;
            temp->object = ptr->object;
            temp->next = ptr->next;

            /* update the matched block as most recently used */
            cache_delete(cache_hdr, ptr);
            cache_most_recent(cache_hdr, temp);
            V(&(cache_hdr->mutex));
            return temp;
        }     
    }

    V(&(cache_hdr->mutex));

    /* not found and return NULL */
    return NULL;
}

/* insert an object to cache */
void cache_insert(cache *cache_hdr, char *uri, char *object, size_t size) {
    P(&(cache_hdr->mutex));

    /* do not insert objects exceed the max size */
    if (size > MAX_OBJECT_SIZE) {
        return;
    }

    /* if the cache is full, evict blocks until the object can be fitted in */
    while (size + cache_hdr->size > MAX_CACHE_SIZE) {
        cache_evict(cache_hdr);
    }

    /* copy object */
    char *obj_copy = malloc(size);
    memcpy(obj_copy, object, size);

    /* copy uri */
    char *uri_copy = malloc(strlen(uri));
    memcpy(uri_copy, uri, strlen(uri));

    /* create a new block and add into the cache */
    cache_block *temp = malloc(sizeof(cache_block));
    temp->uri = uri_copy;
    temp->object_size = size;
    temp->object = obj_copy;
    /* update the new block as most recently used */
    cache_most_recent(cache_hdr, temp);

    V(&(cache_hdr->mutex));
    return;
}
