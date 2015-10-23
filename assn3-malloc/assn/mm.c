/*
 * This implementation replicates the implicit list implementation
 * provided in the textbook
 * "Computer Systems - A Programmer's Perspective"
 * Blocks are never coalesced or reused.
 * Realloc is implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"

#define DEBUG

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "Smash bros",
    /* First member's full name */
    "Yiming Kang",
    /* First member's email address */
    "yiming.kang@mail.utoronto.ca",
    /* Second member's full name (leave blank if none) */
    "Zexuan Wang",
    /* Second member's email address (leave blank if none) */
    "TODO@TODO.TODO" // TODO
};

/*************************************************************************
 * Basic Constants and Macros
 * You are not required to use these macros but may find them helpful.
*************************************************************************/
#define WSIZE       sizeof(void *)            /* word size (bytes) */
#define DSIZE       (2 * WSIZE)            /* doubleword size (bytes) */
#define CHUNKSIZE   (1<<7)      /* initial heap size (bytes) */

#define MAX(x,y) ((x) > (y)?(x) :(y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)          (*(uintptr_t *)(p))
#define PUT(p,val)      (*(uintptr_t *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~(DSIZE - 1))
#define GET_ALLOC(p)    (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)        (void *)((char *)(bp))
#define FTRP(bp)        (void *)((char *)(bp) + GET_SIZE(HDRP(bp)) - WSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   (void *)(*((uint64_t *)((char *)(bp) + WSIZE)))
#define PREV_BLKP(bp)   (void *)(*((uint64_t *)((char *)(bp) + DSIZE)))

/* For coalesce */
#define PHYSICAL_NEXT_W(bp) (void *)((char *)(bp) + GET_SIZE((char *)(bp)))
#define PHYSICAL_PREV_W(bp) (void *)((char *)(bp) - WSIZE)
#define PHYSICAL_PREV_BLK(bp) (void *)((char *)(bp) - GET_SIZE(PHYSICAL_PREV_W(bp)))
#define PHYSICAL_NEXT_BLK(bp) PHYSICAL_NEXT_W(bp)

/* Set next/previous pointer*/
#define SET_NEXT(f, t)  PUT((char *)f + WSIZE, (uintptr_t) t)
#define SET_PREV(f, t)  PUT((char *)f + DSIZE, (uintptr_t) t)

#define IS_END(bp)      GET(bp) == 0 ? 1 : 0

void* heap_listp = NULL;

/* Function prototypes */
void *insert_block(void *);
void blk_unlink(void *);

/**********************************************************
 * mm_init
 * Initialize the heap, including "allocation" of the
 * prologue and epilogue
 **********************************************************/
 int mm_init(void)
 {
     if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1)
         return -1;
     PUT(heap_listp, PACK(2 * DSIZE, 1));   // Header
     PUT(heap_listp + (1 * WSIZE), 0);                    // Next pointer
     PUT(heap_listp + (2 * WSIZE), 0);                    // Prev pointer
     PUT(heap_listp + (3 * WSIZE), PACK(2 * DSIZE, 1));   // Footer
     PUT(heap_listp + (4 * WSIZE), PACK(0, 1));           // End of heap indicator
     PUT(heap_listp + (5 * WSIZE), PACK(0, 1));           // Padding
     printf("mm_init heap is at 0x%x, heap brk is at 0x%x\n", heap_listp, heap_listp + 5 * WSIZE);
     /*
     printf("HEAP IS 0x%x\n", *((uint64_t *)((char *)heap_listp)));
     printf("HEAP +1 IS 0x%x\n", *((uint64_t *)((char *)heap_listp + WSIZE)));
     printf("HEAP +2 IS 0x%x\n", *((uint64_t *)((char *)heap_listp + 2 * WSIZE)));
     printf("HEAP +3 IS 0x%x\n", *((uint64_t *)((char *)heap_listp + 3 * WSIZE)));
     */
     return 0;
 }


/**********************************************************
 * coalesce
 * Covers the 4 cases discussed in the text:
 * - both neighbours are allocated
 * - the next block is available for coalescing
 * - the previous block is available for coalescing
 * - both neighbours are available for coalescing
 **********************************************************/
void *coalesce(void *bp)
{
    printf("coalesce is called with 0x%x\n", bp);
    /* Find physical prev and next block */
    size_t prev_alloc = GET_ALLOC(PHYSICAL_PREV_W(bp));
    size_t next_alloc = GET_ALLOC(PHYSICAL_NEXT_W(bp));
    size_t size = GET_SIZE(HDRP(bp));

    printf("coalesce prev_alloc: %d ... next_alloc: %d\n", prev_alloc, next_alloc);

    if (prev_alloc && next_alloc) {       /* Case 1 */
        puts("coalesce case 1");
        return insert_block(bp);
    }

    else if (prev_alloc && !next_alloc) { /* Case 2 */
        puts("coalesce case 2");
        // Size of next block is stored in the next word
        size += GET_SIZE(PHYSICAL_NEXT_W(bp));
        void *next_blk = PHYSICAL_NEXT_BLK(bp);

        // unlink next block from neighbors
        blk_unlink(next_blk);

        // construct this block (set sizes only, leave pointers alone)
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return insert_block(bp);
    }

    else if (!prev_alloc && next_alloc) { /* Case 3 */
        puts("coalesce case 3");
        // Size of prev. block is sotred in the word
        // immediately before bp's header
        size += GET_SIZE(PHYSICAL_PREV_W(bp));

        // Extract the addr. of previous block
        void *prev_blk = PHYSICAL_PREV_BLK(bp);

        // Unlink prev. block from its neighbors
        blk_unlink(prev_blk);

        // Setup header and footer
        PUT(HDRP(prev_blk), PACK(size, 0));
        PUT(FTRP(prev_blk), PACK(size, 0));
        return insert_block(prev_blk);
    }

    else {            /* Case 4 */
        puts("coalesce case 4");
        size += GET_SIZE(PHYSICAL_PREV_W(bp));
        size += GET_SIZE(PHYSICAL_NEXT_W(bp));

        void *prev_blk = PHYSICAL_PREV_BLK(bp);
        void *next_blk = PHYSICAL_NEXT_BLK(bp);

        blk_unlink(prev_blk);
        blk_unlink(next_blk);

        PUT(HDRP(prev_blk), PACK(size,0));
        PUT(FTRP(prev_blk), PACK(size,0));
        return insert_block(prev_blk);
    }
}

/**********************************************************
 * extend_heap
 * Extend the heap by "words" words, maintaining alignment
 * requirements of course. Free the former epilogue block
 * and reallocate its new header
 **********************************************************/
void *extend_heap(size_t words)
{
    // basically dont have to worry about this function
    char *bp;
    size_t size;
    puts("extend_heap is called");

    /* Allocate an even number of words to maintain alignments */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    printf("extend_heap is going to call mem_sbrk with %d\n", size);
    if ( (bp = mem_sbrk(size)) == (void *)-1 && puts("SBRK ERROR!"))
        return NULL;
    printf("extend_heap old end is: 0x%x\n", bp - WSIZE);

    /* shift this block up by one word */
    bp -= WSIZE;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                // free block header
    PUT(FTRP(bp), PACK(size, 0));                // free block footer
    PUT(FTRP(bp) + WSIZE, PACK(0, 1));           // end of heap indicator

    //set bp to header of block
    return coalesce(bp);
}


/**********************************************************
 * find_fit
 * Traverse the heap searching for a block to fit asize
 * Return NULL if no free blocks can handle that size
 * Assumed that asize is aligned
 **********************************************************/
void * find_fit(size_t asize)
{
    void *bp;
    trace_heap();
    // To find fit, search from heap_listp (which is the 'root')
    // bp = 0 indicates end of heap
    printf("HEAP NEXT IS 0x%x\n", *((uint64_t *)((char *)heap_listp + WSIZE)));
    for (bp = heap_listp; bp && GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        printf("find_fit is checking block at 0x%x\n", bp);
        printf("find_fit value is 0x%x\n", *((uint64_t *)bp));
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            printf("find_fit found a suitable block at 0x%x\n", bp);
            return bp;
        }
    }
    puts("find_fit cannot find a good block, done");
    return NULL;
}

/**********************************************************
 * place
 * Mark the block as allocated
 **********************************************************/
void place(void* bp, size_t asize)
{

  /* Get the current block size */
  size_t bsize = GET_SIZE(HDRP(bp));
  printf("place placing at 0x%x, real_size %d, adjusted_size %d\n", bp, asize, bsize);

  PUT(HDRP(bp), PACK(bsize, 1));
  PUT(FTRP(bp), PACK(bsize, 1));
}

/**********************************************************
 * mm_free
 * Free the block and coalesce with neighbouring blocks
 **********************************************************/
void mm_free(void *bp)
{
    puts("mm_free is called");
    if(bp == NULL){
      return;
    }

    // Header info is in the word before bp
    bp -= WSIZE;
    size_t size = GET_SIZE(HDRP(bp));
    printf("mm_free freeing block starting at 0x%x, size %d\n", bp, size);

    // Restore allocation bit to 0
    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size,0));
    coalesce(bp);
}


/**********************************************************
 * mm_malloc
 * Allocate a block of size bytes.
 * The type of search is determined by find_fit
 * The decision of splitting the block, or not is determined
 *   in place(..)
 * If no block satisfies the request, the heap is extended
 **********************************************************/
void *mm_malloc(size_t size)
{
    size_t asize; /* adjusted block size */
    size_t extendsize; /* amount to extend heap if no fit */
    char * bp;

    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1))/ DSIZE);

    //printf("mm_malloc is called with size %d, adjusted to %d\n", size, asize);
    //printf("mm_malloc current heap pointer is at 0x%x\n", heap_listp);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {
        // first unlink bp from its neighbors 
        blk_unlink(bp);

        // initialize bp
        place(bp, asize);

        /* MUST NOT include the header */
        return bp + WSIZE;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    bp = extend_heap(extendsize/WSIZE);
    //printf("mm_malloc extend_heap returned 0x%x\n", bp);
    if (bp == NULL)
        return NULL;
    //printf("mm_malloc placing bp\n");
    place(bp, asize);

    /* MUST NOT include the header */
    return bp + WSIZE;
}

/**********************************************************
 * mm_realloc
 * Implemented simply in terms of mm_malloc and mm_free
 *********************************************************/
void *mm_realloc(void *ptr, size_t size)
{
    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0){
      mm_free(ptr);
      return NULL;
    }
    /* If oldptr is NULL, then this is just malloc. */
    if (ptr == NULL)
      return (mm_malloc(size));

    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    /* Copy the old data. */

    // Get size from the word before oldptr
    copySize = GET_SIZE(HDRP(oldptr - WSIZE));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

/**********************************************************
 * mm_check
 * Check the consistency of the memory heap
 * Return nonzero if the heap is consistant.
 *********************************************************/
int mm_check(void){
  return 1;
}

void *insert_block(void *bp){
    printf("insert_block: block to be inserted 0x%x\n", bp);
    printf("insert_block: head of heap 0x%x\n", heap_listp);
    printf("insert_block: heap->next = 0x%x\n", *((uint64_t *)((char *)heap_listp + WSIZE)));
    // 1- Get the next element from head
    void *next_ptr = NEXT_BLKP(heap_listp);

    // 2- prev of bp is heap_listp, next of heap_listp is bp
    printf("insert_block setting bp->prev = 0x%x\n", heap_listp);
    SET_PREV(bp, heap_listp);

    printf("insert_block setting heap->next = 0x%x\n", bp);
    SET_NEXT(heap_listp, bp);
    printf("HEAP NEXT IS 0x%x\n", *((uint64_t *)((char *)heap_listp + WSIZE)));

    if (next_ptr){
        puts("insert_block heap->next is not NULL");
        SET_PREV(next_ptr, bp);
    }

    // 4- bp.next = tmp
    printf("insert_block setting bp->next = 0x%x\n", next_ptr);
    SET_NEXT(bp, next_ptr);
    puts("insert_block done");
    return bp;
}

void blk_unlink(void *bp){
    void *prev = PREV_BLKP(bp);
    void *next = PREV_BLKP(bp);

    // If prev is not the begining of heap
    if (!IS_END(prev)){
        SET_NEXT(prev, next);
    }

    // if next is not the end of heap
    if (!IS_END(next)){
        SET_PREV(next, prev);
    }
}

void trace_heap(){
    uint64_t *h = heap_listp;
    puts("Checking heap....");
    while (h){
        puts("!!!HEAP!!!");
        printf("ADDR = 0x%x\n", h);
        h = NEXT_BLKP(h);
    }
    puts("Done checking heap....");
}
