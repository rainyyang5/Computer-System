/*
 * mm.c
 *
 * Mengyu YANG
 * mengyuy@andrew.cmu.edu
 *
 * This allocator is implemented by segregated list. Free blocks are divided
 * into 18 free lists according to its size. The header of each free
 * list is stored in the head of the heap.
 * The format of heap is: alignment padding, prologue block header, free
 *  lists header, prologue block footer, regular block and epilogue block header.
 * For free block, the format is header, succ, pred, padding and footer.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "contracts.h"

#include "mm.h"
#include "memlib.h"


// Create aliases for driver tests
// DO NOT CHANGE THE FOLLOWING!
#ifdef DRIVER
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif

/*
 *  Logging Functions
 *  -----------------
 *  - dbg_printf acts like printf, but will not be run in a release build.
 *  - checkheap acts like mm_checkheap, but prints the line it failed on and
 *    exits if it fails.
 */

#ifndef NDEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#define checkheap(verbose) do {if (mm_checkheap(verbose)) {  \
                             printf("Checkheap failed on line %d\n", __LINE__);\
                             exit(-1);  \
                        }}while(0)
#else
#define dbg_printf(...)
#define checkheap(...)
#endif

/* Basic constants */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 9)

/* Free list size */
#define SIZE1 (1<<4)
#define SIZE2 (1<<5)
#define SIZE3 (1<<6)
#define SIZE4 (1<<7)
#define SIZE5 (1<<8)
#define SIZE6 (1<<9)
#define SIZE7 (1<<10)  	// 1 KB 
#define SIZE8 (1<<11)
#define SIZE9 (1<<12)
#define SIZE10 (1<<13)
#define SIZE11 (1<<14)
#define SIZE12 (1<<15)
#define SIZE13 (1<<16)
#define SIZE14 (1<<17)
#define SIZE15 (1<<18)
#define SIZE16 (1<<19)
#define SIZE17 (1<<20) 	// 1 MB 
#define LISTNUM 18

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8



/* Global vars */
static char *heap_listp = 0;

/* Inline functions */
static inline size_t pack(size_t size, size_t prevAllocBit, size_t alloc){
    return (size|prevAllocBit|alloc);
}

static inline unsigned int get(void *p){
    return *(unsigned int *)(p);
}

static inline void put(void *p, size_t val){
    *(unsigned int *)(p) = val;
}

static inline size_t getSize(void *p){
    return (get(p) & ~0x7);
}

static inline size_t getPrevAlloc(void *p){
    return (get(p) & 0x2);
}

static inline size_t getAlloc(void *p){
    return (get(p) & 0x1);
}

static inline char* getHeader(void *bp){
    return ((char *)(bp) - WSIZE);
}

static inline char* getFooter(void *bp){
    return ((char *)(bp)+getSize(getHeader(bp)) - DSIZE);
}

static inline char* nextBlock(void *bp){
    return ((char *)(bp) + getSize(((char *)(bp) - WSIZE)));
}

static inline char* prevBlock(void *bp){
    return ((char *)(bp) - getSize(((char *)(bp) - DSIZE)));
}

static inline void* succ(void *bp){
    return bp;
}

static inline void* pred(void *bp){
    return (char *)bp + WSIZE;
}

static inline void setPrevAlloc(void *p){
    put(p, (get(p) | 0x2));
}

static inline void setPrevFree(void *p){
    put(p, (get(p) & (~0x2)));
}

static inline size_t align (void *p){
    return (((size_t)(p) + (ALIGNMENT-1)) & ~0x7);
}


/*
 * Return whether the pointer is in the heap.
 * Used to check heap
 */
static inline int inHeap(void *p) {
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * Used to check heap
 */
static inline int aligned(void *p) {
    return (size_t)align(p) == (size_t)p;
}


static inline size_t getIndex(size_t size){
    if (size <= SIZE1) {
	return 0;
    } else if (size <= SIZE2) {
	return 1;
    } else if (size <= SIZE3) {
	return 2;
    } else if (size <= SIZE4) {
	return 3;
    } else if (size <= SIZE5) {
	return 4;
    } else if (size <= SIZE6) {
	return 5;
    } else if (size <= SIZE7) {
	return 6;
    } else if (size <= SIZE8) {
	return 7;
    } else if (size <= SIZE9) {
	return 8;
    } else if (size <= SIZE10) {
	return 9;
    } else if (size <= SIZE11) {
	return 10;
    } else if (size <= SIZE12) {
	return 11;
    } else if (size <= SIZE13) {
	return 12;
    } else if (size <= SIZE14) {
	return 13;
    } else if (size <= SIZE15) {
	return 14;
    } else if (size <= SIZE16) {
	return 15;
    } else if (size <= SIZE17) {
	return 16;
    } else 
	return 17;
}

static inline void* nextFree(void *bp){
    return heap_listp + get(succ(bp));
}

static inline void* prevFree(void *bp){
    return heap_listp + get(pred(bp));
}

static inline void insertListNode(void *bp, size_t index){

    char *firstList = heap_listp + DSIZE;
    put(succ(bp), get(succ(firstList + index * DSIZE)));
    put(pred(bp), get(pred(nextFree(bp))));
    put(succ(firstList + index * DSIZE), (long)bp - (long)heap_listp);
    put(pred(nextFree(bp)), (long)bp - (long)heap_listp);
}

static inline void deleteListNode(void *bp){
    put(pred(nextFree(bp)), get(pred(bp)));
    put(succ(prevFree(bp)), get(succ(bp)));
}


/* helper funtions prototype*/
static void *extendHeap(size_t words);
static void *coalesce(void *bp);
static void *findFit(size_t asize);
static void place(void *bp, size_t asize);


/*  helper functions  */

/* Extend the heap by the size words. */
static void *extendHeap(size_t words){
    char *bp;
    size_t size;
    size_t prevAllocBit;

    size = (words % 2) ? (words + 1) * WSIZE : (words * WSIZE);
   
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    prevAllocBit = getPrevAlloc(getHeader(bp));
    put(getHeader(bp), pack(size, prevAllocBit, 0));
    put(getFooter(bp), pack(size, 0, 0));
    put(getHeader(nextBlock(bp)), pack(0, prevAllocBit, 1));
   
    return coalesce(bp); // coalesce if the previous blk is free
}

/* coalesce free blocks */
static void *coalesce(void *bp){
   
    size_t prevAllocBit = getPrevAlloc(getHeader(bp));
    size_t nextAllocBit = getAlloc(getHeader(nextBlock(bp)));
    size_t size = getSize(getHeader(bp));
    size_t index;
    size_t prevPAlloc;

    /* both the previous and next block allocated */
    if (prevAllocBit && nextAllocBit) {
        setPrevFree(getHeader(nextBlock(bp)));
    }

    /* Only previous block is free */
    else if (!prevAllocBit && nextAllocBit) {
        size += getSize(getHeader(prevBlock(bp)));
        prevPAlloc = getPrevAlloc(getHeader(prevBlock(bp)));
        deleteListNode(prevBlock(bp));
        put(getFooter(bp), size);
        bp = prevBlock(bp);
        put(getHeader(bp), pack(size, prevPAlloc, 0));
        setPrevFree(getHeader(nextBlock(bp)));
    }

    /* Only next block is free */
    else if (prevAllocBit && !nextAllocBit) {
        size += getSize(getHeader(nextBlock(bp)));
        deleteListNode(nextBlock(bp));
        put(getHeader(bp), pack(size, prevAllocBit, 0));
        put(getFooter(bp), size);
    }
    
    /* Both previous and next block are free */
    else {
        size += getSize(getHeader(prevBlock(bp))) + 
            getSize(getFooter(nextBlock(bp)));
        prevPAlloc = getPrevAlloc(getHeader(prevBlock(bp)));
        deleteListNode(nextBlock(bp));
        deleteListNode(prevBlock(bp));
        put(getHeader(prevBlock(bp)), pack(size, prevPAlloc, 0));
        put(getFooter(nextBlock(bp)), size);
        bp = prevBlock(bp);
    }

    index = getIndex(size);
    insertListNode(bp, index);
    return bp;
}

/* Find a segregate fit. If there is no a fit in small free list, search
   a larger free list
*/
static void *findFit(size_t asize){

    size_t index = getIndex(asize);
    char *firstList = heap_listp + DSIZE;
    char *lastList = firstList + (LISTNUM-1) * DSIZE;
    void *list, *ptr;


    for (list = firstList + index * DSIZE; list != (char *)lastList + DSIZE;
        list = (char *)list + DSIZE){
        for (ptr = nextFree(list); ptr != list;
            ptr = nextFree(ptr)){
            if (!getAlloc(getHeader(ptr)) && (getSize(getHeader(ptr)) >= asize)){
                   return ptr;
            }
        }
    }
   
    return NULL;
}

/* Place a block to bp. If bp is larger than asize by more than the minimum
   block size, then split the block into two part, otherwize just set 
   the whole block allocated
*/
static void place(void *bp, size_t asize){
    size_t csize = getSize(getHeader(bp));
    size_t prevAllocBit = getPrevAlloc(getHeader(bp));
    size_t index;

    deleteListNode(bp);

    // Split the block
    if ((csize - asize) >= (2 * DSIZE)) {
        // Allocate the first part
        put(getHeader(bp), pack(asize, prevAllocBit, 1));

        // Set the remaining part free
        bp = nextBlock(bp);
        put(getHeader(bp), pack((csize - asize), prevAllocBit, 0));
        put(getFooter(bp), pack((csize - asize), 0, 0));
        // Insert the free part into free list
        index = getIndex(csize - asize);
        insertListNode(bp, index);
    }
    // No split
    else{
        put(getHeader(bp), pack(csize, prevAllocBit, 1));
        setPrevAlloc(getHeader(nextBlock(bp)));
    }
}


/*
 *  Malloc Implementation
 *  ---------------------
 *  The following functions deal with the user-facing malloc implementation.
 */


/*
 * Initialize: return -1 on error, 0 on success.
 * The heap format is padding alignment, prologue header, free 
 * list header, prologue footer, regular blks and epilogue.
 */
int mm_init(void) {

    size_t initSize, prologueSize;
    size_t listAddr;

    initSize = (LISTNUM + 2) * DSIZE;
    if ((heap_listp = mem_sbrk(initSize)) == (void *)-1) {
        return -1;
    }

    // padding
    put(heap_listp, pack(0, 0, 0)); 

    // prologue
    prologueSize = (LISTNUM + 1) * DSIZE;
    put(heap_listp+WSIZE, pack(prologueSize, 2, 1)); 
    put(heap_listp+DSIZE, pack(prologueSize, 2, 1));

    // free list header
    for (size_t i = 0; i < LISTNUM; i++) {
        listAddr = (i + 1) * DSIZE;
        put(heap_listp + listAddr, listAddr); // pointer is 8 bytes
        put(heap_listp + (listAddr + WSIZE), listAddr);
    }
     
    // epilogue 
    put(getFooter(heap_listp+DSIZE) + WSIZE, pack(0, 2, 1));

    if (extendHeap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * malloc: adjust the size to the minimum block size or a
 * multiple of alignment. Search the free list for a fit,
 * if not, extend the heap
 */
void *malloc (size_t size) {
    //checkheap(1);  // Let's make sure the heap is ok!
    size_t asize; // adjust size
    size_t extendsize;
    char *bp;

    if (size <= 0){
        return NULL;
    }

    // Adjust the size to at least the minimum blk size and 
    // a multiple of alignment 8
    if (size <= (DSIZE + WSIZE)){
        asize = 2 * DSIZE;
    }
    else{
        asize = DSIZE*((size + DSIZE + (DSIZE - 1)) / DSIZE);
    }

    // search the freelist for a fit 
    if ((bp = findFit(asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    // If no fit, extend the heap
    if(asize>CHUNKSIZE)
        extendsize = asize;
    else
        extendsize = CHUNKSIZE;

    if ((bp = extendHeap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);

    return bp;
}

/*
 * free: free a blk started by *ptr, set its header and ftr
 */
void free (void *ptr) {
    if (ptr == NULL) {
        return;
    }

    size_t size = getSize(getHeader(ptr));
    size_t prevAllocBit = getPrevAlloc(getHeader(ptr));

    put(getHeader(ptr), pack(size, prevAllocBit, 0));
    put(getFooter(ptr), pack(size, 0, 0));
    coalesce(ptr);
}

/*
 * realloc - If size is zero, realloc is equivalent to free.
 * If oldptr is NULL, realloc is equivalent to malloc.
 * Otherwise, realloc taks an existing block of memory, 
 * pointed by the oldptr then allocates a region of memory 
 * large enough to hold size bytes and returns the address 
 * of this new block
 */
void *realloc(void *oldptr, size_t size) {
    size_t oldsize;
    void *newptr;

    if (size == 0){
        free(oldptr);
        return 0;
    }

    if (oldptr == NULL){
        return malloc(size);
    }

    newptr = malloc(size);

    // If realloc() fails the original block is left untouched  
    if(!newptr) {
        return 0;
    }

    oldsize = getSize(getHeader(oldptr));
    if (size < oldsize){
        oldsize=size;
    }
    memcpy(newptr, oldptr, oldsize);

    
    free(oldptr);
    return newptr;
}

/*
 * calloc - simple implementation from mm.naive
 */
void *calloc (size_t nmemb, size_t size) {
    size_t bytes = nmemb * size;
    void *newptr;

    newptr = malloc(bytes);
    memset(newptr, 0, bytes);

    return newptr;
}

/* checkheap: Returns 0 if no errors were found, 
 * otherwise returns the error
 */
int mm_checkheap(int verbose) {

    char *firstList = heap_listp + DSIZE;
    char *lastList = firstList + (LISTNUM-1) * DSIZE;
    char *curBlock = heap_listp + DSIZE;    
    size_t size = getSize(getHeader(curBlock)); 
    int freeCountHeap = 0;
    int freeCountList = 0;
    char *nextFreePtr;
    char *list;
    char *ptr;
    int  s,a;    
    size_t prevAlloc;

    verbose = verbose;

    /* check alignment padding*/
    if (get(mem_heap_lo()) != 0) {
        printf("Alignment padding error!\n");
    }

    /* check prologue */
    if (!getAlloc(getHeader(heap_listp+DSIZE))) {
        printf("Prologue error!\n");
    }

    
    /*     Check the heap    */
    while (size != 0){

        // Check whether the block is in heap
        if (!inHeap(curBlock)){
            dbg_printf("Block %p is not in the heap\n", curBlock);
        }

        // Check each blk's address alignment
        if (!aligned(curBlock)) {
            dbg_printf("Block %p is not aligned.\n", curBlock);
            exit(0);
        }
       
       
        if (!getAlloc(getHeader(curBlock))){
            freeCountHeap ++;
            // Check each blk's header and footer
            s = (getSize(getHeader(curBlock)) == getSize(getFooter(curBlock)));
            a = (getAlloc(getHeader(curBlock)) == getAlloc(getFooter(curBlock)));
            if (!s || !a) {
                dbg_printf("Header&Footer does not match in %p\n", curBlock);
            }

            // Check coalescing
            prevAlloc = getPrevAlloc(getHeader(curBlock));
	    if (!prevAlloc){
                dbg_printf("Coalescing error! \n");
            }

        }
        curBlock = nextBlock(curBlock);
        size = getSize(getHeader(curBlock));
    }



    /*     Check the free list     */
    for (list = firstList; list != (char *)lastList + DSIZE; 
        list = (char *)list + DSIZE){              
        for (ptr = nextFree(list); ptr != list; ptr = nextFree(ptr)){

            // Check the free list node is in heap
	    if (!inHeap(ptr)){
                dbg_printf("list node %p is not in the heap\n", ptr);
            }

            freeCountList ++;
            nextFreePtr = nextFree(ptr);

            // Check all next/prev pointers are consistent             
            if (prevFree(nextFreePtr) != (ptr)){
		dbg_printf("next/prev pointers is not consistent!\n");
	    }
                             	
        }
    }

    // Check free blocks by iterating thru every blk and traversing
    // free list by pointers and see if they match.
    if (freeCountHeap != freeCountList){
        dbg_printf("Free block count does not match!\n");
    }

    return 0;
}




