/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * My solution: Implicit List implementation, used balanced tages. almost same to the one in text book
 * Splitting policy: splitting at place(), splitt as much as possible(until left size is bigger thatn 16byte) 
 * Placement policy: next_fit, as it showed the best performance
 * Coalescing policy: immediate coalescing while extend_heap and free 
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))



/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */ 
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  

#define MAX(x, y) ((x) > (y)? (x) : (y))  

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) 

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            
#define PUT(p, val)  (*(unsigned int *)(p) = (val))   

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                  
#define GET_ALLOC(p) (GET(p) & 0x1)                   

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) 

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) 
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 

/*Given block ptr bp of free block. compute address of next/prev free block pointer*/
#define NEXT_POINTER(bp) *(char **)(bp)
#define PREV_POINTER(bp) *((char **)(bp) + 1)

/* Global variables */
static char *heap_listp = 0;  // Pointer to first block   

/*root node for explicit free list*/
static char *root=NULL; 

/*number of free blocks in list*/
static int cnt = 0;

/*count the number of malloc & free*/
static int acount = 0;


static char *rover;           /* Next fit rover */





int mm_check(void); //check heap consistency

static void *extend_heap(size_t words); //extend heap
static void place(void *bp, size_t asize); //place allocated block in the freed block found + split if needed
static void *find_fit(size_t asize); //find the block to allocate in the free block list
static void *coalesce(void *bp); //immediate coalesce

//not used, it was for alternate implementation
static void update_root(void*bp); 
static void delete_node(void*bp);
static void check_coalesence(void);


/* 
 * mm_init - initialize the malloc package.
 */


int mm_init(void)
{
    /*Create initial empty heap
    return -1 when ran out of memory while creating empty heap*/
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)  return -1; 

    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2*WSIZE);        

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
	    return -1;

    
    rover = heap_listp;
  

    return 0;             

}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    if (heap_listp == 0){
	mm_init();
    }

    /* Ignore spurious requests */
    if (size == 0)
	return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) asize = 2*DSIZE;                                        
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); 

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  
	    place(bp, asize);                 
	    return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;                                  
    place(bp, asize);         

    acount++;
    if(acount>200){
        check_coalesence();
        acount = 0;
    }

    return bp;
}

/*
 * mm_free - Freeing a block & coalesce
 */
void mm_free(void *ptr)
{
    if(ptr == 0) 
	return;

    size_t size = GET_SIZE(HDRP(ptr));
    if (heap_listp == 0){
	mm_init();
    }

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    /*
    NEXT_POINTER(ptr) = NULL;
    PREV_POINTER(ptr) = NULL;
    cnt++;
    */

    coalesce(ptr);

    /*
    acount++;
    if(acount>200){
        check_coalesence();
        acount = 0;
    }
    */

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
     size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
	mm_free(ptr);
	return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
	return mm_malloc(size);
    }

    size_t csize = GET_SIZE(HDRP(ptr));



    /*realloc trying to use original address as possible as can
   //when realloc size is smaller
    if(size<csize){
        newptr = ptr;        

        if((csize-size)>=(3*DSIZE)){  //split into free/ allocated block
            PUT(HDRP(ptr), PACK(size, 1));
            PUT(FTRP(ptr), PACK(size, 1));
            void* next = NEXT_BLKP(ptr);
	        PUT(HDRP(next), PACK(csize-size, 0));
	        PUT(FTRP(next), PACK(csize-size, 0));
        }
        else{
            PUT(HDRP(ptr), PACK(csize, 1));
	        PUT(FTRP(ptr), PACK(csize, 1));
        }  	    
        return newptr;
    } 

    //when it it availabe to realloc at same address
    size_t asize = size-csize; //memory needed to be allocated more
    


    //newptr = mm_malloc(size);
    
    size_t backsize = GET_SIZE(HDRP(NEXT_BLKP(ptr)));

    if(GET_ALLOC(HDRP(NEXT_BLKP(ptr)))){
        newptr = mm_malloc(size);
    }
    else if(backsize<asize){
        newptr = mm_malloc(size);
    }
    else{
        if((backsize-asize)<2*DSIZE){ //no split
            PUT(HDRP(ptr), PACK(csize+backsize, 1));
	        PUT(FTRP(ptr), PACK(csize+backsize, 1));
        }
        else{ //split
            PUT(HDRP(ptr), PACK(size, 1));
	        PUT(FTRP(ptr), PACK(size, 1));
            void* next = NEXT_BLKP(ptr);
	        PUT(HDRP(next), PACK(backsize-asize, 0));
	        PUT(FTRP(next), PACK(backsize-asize, 0));
        }
        return newptr;
    }
    */

    newptr = mm_malloc(size);
    //If realloc() fails the original block is left untouched  
    if(!newptr) {
	return 0;
    }    
   

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

/* extend heap with free block and return its block pointer*/
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; 
    if ((long)(bp = mem_sbrk(size)) == -1)  
	return NULL;                                    

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ 

    /*Initialize pointer for free list*//*
    NEXT_POINTER(bp) = NULL;
    PREV_POINTER(bp) = NULL;
    cnt++;
    */

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                         
}
/*update root*/
static void update_root(void* bp){
    if(root!=NULL){
        PREV_POINTER(root) = bp;
        NEXT_POINTER(bp) = root;
        PREV_POINTER(bp) = NULL;
        root = bp;
    }
    else{
        root = bp;
        NEXT_POINTER(bp) = NULL;
        PREV_POINTER(bp) = NULL;
    }
}

/*delete node while coalescing*/
static void delete_node(void *bp){

    if(root!=bp&&PREV_POINTER(bp)==NULL && NEXT_POINTER(bp)==NULL) return;
    else if(root==bp&&PREV_POINTER(bp)==NULL && NEXT_POINTER(bp)==NULL){ //root & only one node in the list
        root = NULL;
    }
    else if(root==bp&&PREV_POINTER(bp)==NULL){ //root
        PREV_POINTER(NEXT_POINTER(bp)) = NULL;
        root = NEXT_POINTER(bp);
    }
    else if(NEXT_POINTER(bp)==NULL){ //last node
        NEXT_POINTER(PREV_POINTER(bp)) = NULL;
    }
     else{ //else
        PREV_POINTER(NEXT_POINTER(bp)) = PREV_POINTER(bp);
        NEXT_POINTER(PREV_POINTER(bp)) = NEXT_POINTER(bp);
    }
}


/* coalesce - Boundary tag coalescing. Return ptr to coalesced block */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    /*
    void* prev = PREV_BLKP(bp);
    void* next = NEXT_BLKP(bp);
    */

    if (prev_alloc && next_alloc) {            /* Case 1 */
	    //update_root(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
	size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

    //pointer update
    //delete_node(next);

    //boundary tag update
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(bp), PACK(size,0));

    //update_root(bp);
    //cnt--;

    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp)));
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	bp = PREV_BLKP(bp);

    /*pointer update*//*
        if(root!=bp){
            delete_node(bp);
            update_root(bp);
        }

        cnt--;
    */
    
    }
    

    else {                                     /* Case 4 */
	size += GET_SIZE(HDRP(PREV_BLKP(bp))) + 
	GET_SIZE(FTRP(NEXT_BLKP(bp)));
	PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
	PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
	
    /*
    if(prev==root){
            delete_node(next);
        }
        else if(next==root){
            delete_node(next);
            update_root(prev);
        }
        else{
            delete_node(next);
            delete_node(prev);
            update_root(prev);
        }
    cnt+=-2
    */

    bp = PREV_BLKP(bp);

    }

#
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp))) 
	rover = bp;

    return bp;
}

/*
    place - Place block of asize bytes at start of free block bp 
    and split if remainder would be at least minimum block size
*/
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   

    if ((csize - asize) >= (2*DSIZE)) { 
	PUT(HDRP(bp), PACK(asize, 1));
	PUT(FTRP(bp), PACK(asize, 1));
	bp = NEXT_BLKP(bp);
	PUT(HDRP(bp), PACK(csize-asize, 0));
	PUT(FTRP(bp), PACK(csize-asize, 0));

    /*adjust pointer
        PREV_POINTER(next) = PREV_POINTER(bp);
        NEXT_POINTER(next) = NEXT_POINTER(bp);
        //update root if needed
        if(root==bp) root = next;
    */

    }
    else { 
	PUT(HDRP(bp), PACK(csize, 1));
	PUT(FTRP(bp), PACK(csize, 1));

    //adjust free list --> delete this node
    //delete_node(bp);
    }
}

/* find_fit - Find a fit for a block with asize byte */
static void *find_fit(size_t asize)
{

    /* Next fit search */
    char *oldrover = rover;

    /* Search from the rover to the end of list */
    for ( ; GET_SIZE(HDRP(rover)) > 0; rover = NEXT_BLKP(rover))
	    if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	        return rover;

    /* search from start of list to old rover */
    for (rover = heap_listp; rover < oldrover; rover = NEXT_BLKP(rover))
	    if (!GET_ALLOC(HDRP(rover)) && (asize <= GET_SIZE(HDRP(rover))))
	        return rover;

    return NULL;  /* no fit found */

    /* First fit search */
    /* void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
	        return bp;
	    }
    }
    return NULL; /* No fit */

    /*First fit search for explicit free list 
    void *bp=root;

    int i=cnt;

    while(bp!=NULL&&i>0){
        
	    if (!GET_ALLOC(HDRP(bp)) && asize <= GET_SIZE(HDRP(bp))) {
	        return bp;
	    }
        if(NEXT_POINTER(bp)==NULL) break;
        if(NEXT_POINTER(bp)<(char*)mem_heap_lo() || NEXT_POINTER(bp)>(char*)(mem_heap_lo()+mem_heapsize())) break;
        printf("%p\n", NEXT_POINTER(bp));
        if(bp==NEXT_POINTER(bp)) break;
        bp = NEXT_POINTER(bp);
        i--;       
    }

    return NULL; */

    /*best-fit search*/
    /*
    void *bp;
    void *result = NULL;
    size_t subtract;
    size_t min = CHUNKSIZE;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
	    if (!GET_ALLOC(HDRP(bp)) && (asize ==GET_SIZE(HDRP(bp)))) {
	        return bp;
	    }
        else if(!GET_ALLOC(HDRP(bp))){
            subtract = GET_SIZE(HDRP(bp)) - asize;
            if(subtract>0&&subtract<min){
                min = subtract;
                result = bp;
            }
        }
    }
    return result;
    */

}

//for deferred coalescing
static void check_coalesence(void){
    void *bp = 0;
    int isprev_free = 0;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if(!GET_ALLOC(HDRP(bp))){
            if(isprev_free){
                coalesce(bp);
            }
            isprev_free=1;
        }
        else isprev_free = 0;
    }
}

//heap consistency checker
//return nonzero value if and only if heap is consistent
int mm_check(void){

    void* bp;

    int isprev_free = 0;
    // for every block, check if they are doubleword aligned 
    //check if their header and footer are same
    //check if any contiguous free blocks escape coalescing
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if ((size_t)bp % 8){
            printf("Error: %p is not 8byte aligned\n", bp);
            return 0;
        }
        if (GET(HDRP(bp)) != GET(FTRP(bp))){ printf("Error: header does not match footer\n"); return 0; }
        
        
        if(!GET_ALLOC(HDRP(bp))){
            if(isprev_free){
                printf("Error: %p, %p are not coalesced even though they are neighboring freeblock\n", PREV_BLKP(bp), bp);
                return 0;
            }
            isprev_free=1;
        }
        else isprev_free=0;
        
    }

    //checker for explicit free list
    //check if a heap block point to valid heap address
    //check if every block in free list marked free
    //check if the pointers in the free list point to valid free block
    /*
    bp = root;
    int i = cnt;
    while(bp!=NULL&&i>0){
        if(NEXT_POINTER(bp)!=NULL && PREV_POINTER(bp)!=NULL){
            if(NEXT_POINTER(bp)<(char*)mem_heap_lo() || NEXT_POINTER(bp)>(char*)(mem_heap_lo()+mem_heapsize())){
                printf("Error:pointers not pointing valid heap address\n");
                return 0;
            }
            if(PREV_POINTER(bp)<(char*)mem_heap_lo() || PREV_POINTER(bp)>(char*)(mem_heap_lo()+mem_heapsize())){
             printf("Error:pointers not pointing valid heap address\n");
                return 0;
            }
        }
	    if(GET_ALLOC(HDRP(bp))){
            printf("Error: block not marked free in the free list\n");
            return 0;
        } 

        if(NEXT_POINTER(bp)!=NULL&&GET_ALLOC(HDRP(NEXT_POINTER(bp)))){
            printf("Error: pointers in the free list not pointing to valid free block\n");
            return 0;
        } 
        bp = NEXT_POINTER(bp);
        i--;
    }
    */

    return 1;
}












