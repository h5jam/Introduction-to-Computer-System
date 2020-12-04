/*
 * mm-explicit.c - an empty malloc package
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 *
 * @id : 201902768 
 * @name : 한승오
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mm.h"
#include "memlib.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif


/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#endif /* def DRIVER */

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
 
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(p) (((size_t)(p) + (ALIGNMENT-1)) & ~0x7)

/* macro */

#define HDRSIZE 4
#define FTRSIZE 4
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define OVERHEAD 8

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) > (y) ? (x) : (y))

#define PACK(size,alloc) 	((unsigned) ((size) | (alloc)))

#define GET(p) 				(*(unsigned*)(p))
#define PUT(p,val) 			(*(unsigned*)(p) = (unsigned)(val))
#define GET8(p) 			(*(unsigned long*)(p))
#define PUT8(p,val) 		(*(unsigned long*)(p) = (unsigned long)(val))

#define GET_SIZE(p) 		(GET(p) & ~0x7)
#define GET_ALLOC(p) 		(GET(p) & 0x1)

#define HDRP(bp) 			((char*)(bp) - WSIZE)
#define FTRP(bp) 			((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) 		((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) 		((char*)(bp) - GET_SIZE((char*)(bp) - DSIZE))

#define NEXT_FREEP(bp) 		((char*)(bp))
#define PREV_FREEP(bp) 		((char*)(bp) + DSIZE)

#define NEXT_FREE_BLKP(bp) 	((char*)GET8((char*)(bp)))
#define PREV_FREE_BLKP(bp) 	((char*)GET8((char*)(bp)+DSIZE))

/* static function  */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void* bp, size_t asize);

static void removes(void *bp);

/*
 * Initialize: return -1 on error, 0 on success.
 */
static char* h_ptr = 0; //free list
static char* heap_start = 0;
//static unsigned* epilogue = 0;

/* 
 * extend_heap
 */
static void *extend_heap(size_t words){
	char *bp;
	unsigned size;
	//홀 수 사이즈 조정
	size = (words % 2) ? (words+1)*WSIZE : words*WSIZE;
	//bp에 size에 해당하는 공간 할당
	if((long)(bp = mem_sbrk(size)) < 0)
		return NULL;

	//초기셋팅
	PUT(HDRP(bp), PACK(size,0));
	PUT(FTRP(bp), PACK(size,0));
	PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
	//병합 & free block list에 추가될 것임 
	return coalesce(bp);
}


int mm_init(void) {
    //heap 자원 할당
	if((heap_start = mem_sbrk(2*WSIZE*4)) == NULL)
		return -1;
	//초기 블록 설정
	PUT(heap_start, NULL);
	PUT(heap_start + DSIZE, NULL);
	PUT(heap_start + 2*DSIZE, 0);
	PUT(heap_start + 2*DSIZE + HDRSIZE, PACK(OVERHEAD,1));
	PUT(heap_start + 2*DSIZE + HDRSIZE + FTRSIZE, PACK(OVERHEAD, 1));
	PUT(heap_start + 2*DSIZE + HDRSIZE * 2 + FTRSIZE, PACK(0,1));
	
	h_ptr = heap_start; //가용 블록 리스트 포인터에 맨 앞의 NEXT 주소를 저장함, 연결 리스트의 시작
	heap_start += DSIZE*3;
	//header와 footer사이를 extend
	if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
		return -1;

	return 0;
}

/*
 * malloc
 */
void *malloc (size_t size) {
	char *bp;
	unsigned asize;
	unsigned extendsize;
	//exception
	if(size==0)
		return NULL;
	//allocated size
	asize = MAX(ALIGN(size) + DSIZE, 2*DSIZE+2*WSIZE);
	//find -> alloc
	if((bp = find_fit(asize))){
		place(bp, asize);
		return bp;
	}
	//alloc 실패, extend 
	extendsize = MAX(asize, CHUNKSIZE);
	if((bp = extend_heap(extendsize/WSIZE)) == NULL)
		return NULL;
	//다시 alloc
	place(bp,asize);

	return bp;
}

/*
 * coalesce
 */
static void *coalesce(void *bp){
	size_t next_alloc = GET_ALLOC((void*)(FTRP(bp))+WSIZE);
	size_t prev_alloc = GET_ALLOC((void*)(bp) - 2*WSIZE);
	size_t size = GET_SIZE(HDRP(bp));
	
	//병합 불가
	if(prev_alloc & next_alloc){}
	//next 병합 가능
	else if(prev_alloc && !next_alloc){
		size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
		removes(NEXT_BLKP(bp));
		PUT(HDRP(bp), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
	}
	//prev 병합 가능
	else if(!prev_alloc && next_alloc){
		size += GET_SIZE(HDRP(PREV_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(bp), PACK(size, 0));
		bp = PREV_BLKP(bp);
		removes(bp);
	}
	// next & prev 병합 가능
	else{
		size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
		PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
		PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
		removes(NEXT_BLKP(bp));
		removes(PREV_BLKP(bp));
		bp = PREV_BLKP(bp);
	}
	
	//free block list 맨 앞에 추가
	if(h_ptr){
		PUT8(NEXT_FREEP(bp),h_ptr);
		PUT8(PREV_FREEP(bp), NULL);
		PUT8(PREV_FREEP(h_ptr),bp);
		h_ptr = bp;
	}
	//free block list가 비어있음.
	else{
		h_ptr = bp;
		PUT8(NEXT_FREEP(h_ptr), NULL);
		PUT8(PREV_FREEP(h_ptr), NULL);
	}

	return bp;
}

/*
 * find fit
 */
static void *find_fit(size_t asize){
	void *bp;
	//first fit
	for(bp = h_ptr; bp != NULL; bp = NEXT_FREE_BLKP(bp))
		if(asize <= (size_t)GET_SIZE(HDRP(bp)))
			return bp;

	return NULL;
}

/*
 * place
 */
static void place(void *bp, size_t asize){
	size_t csize = GET_SIZE(HDRP(bp));
	//블록 분할이 가능한 경우
	if((csize-asize) >= (3*DSIZE)){
		PUT(HDRP(bp), PACK(asize, 1));
		PUT(FTRP(bp), PACK(asize, 1));
		removes(bp);
		bp = NEXT_BLKP(bp);
		PUT(HDRP(bp), PACK(csize-asize, 0));
		PUT(FTRP(bp), PACK(csize-asize, 0));
		coalesce(bp);
	}
	//한 번에 배치
	else{
		PUT(HDRP(bp), PACK(csize, 1));
		PUT(FTRP(bp), PACK(csize, 1));
		removes(bp);
	}
	
}

/*
 * removes
 */
static void removes(void *bp){
	//앞 뒤 모두 존재
	if(PREV_FREE_BLKP(bp) && NEXT_FREE_BLKP(bp)){
		PUT8(NEXT_FREEP(PREV_FREE_BLKP(bp)), NEXT_FREE_BLKP(bp));
		PUT8(PREV_FREEP(NEXT_FREE_BLKP(bp)), PREV_FREE_BLKP(bp));
	}
	//뒤만 존재
	else if(!PREV_FREE_BLKP(bp) && NEXT_FREE_BLKP(bp)){
		h_ptr = NEXT_FREE_BLKP(bp);
		PUT8(PREV_FREEP(NEXT_FREE_BLKP(bp)),NULL);
	}
	//앞만 존재
	else if(PREV_FREE_BLKP(bp) && !NEXT_FREE_BLKP(bp)){
		PUT8(NEXT_FREEP(PREV_FREE_BLKP(bp)),NULL);
	}
	//앞, 뒤 모두 존재하지 않음
	else if(!PREV_FREE_BLKP(bp) && !NEXT_FREE_BLKP(bp)) {
		h_ptr = NULL;
	}
}

/*
 * free
 */
void free (void *ptr) {
	//exception
	if(!ptr)
		return;
	//alloc bit 초기화
	size_t size = GET_SIZE(HDRP(ptr));
	PUT(HDRP(ptr), PACK(size,0));
	PUT(FTRP(ptr), PACK(size, 0));
	//free block이 생성됨에 따라 coalesce 호출
	coalesce(ptr);
}

/*
 * realloc - you may want to look at mm-naive.c
 */
void *realloc(void *oldptr, size_t size) {
   
	size_t oldsize;
	void *newptr;

	if(size == 0){
		free(oldptr);
		return 0;
	}

	if(oldptr == NULL)
		return malloc(size);

	newptr = malloc(size);

	if(!newptr)
		return 0;

	oldsize = GET_SIZE(HDRP(oldptr));
	if(size < oldsize)
		oldsize = size;
	memcpy(newptr, oldptr, oldsize);

	free(oldptr);

	return newptr;
}

/*
 * calloc - you may want to look at mm-naive.c
 * This function is not tested by mdriver, but it is
 * needed to run the traces.
 */
void *calloc (size_t nmemb, size_t size) {
    return NULL;
}


/*
 * Return whether the pointer is in the heap.
 * May be useful for debugging.
 */
static int in_heap(const void *p) {
    return p < mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Return whether the pointer is aligned.
 * May be useful for debugging.
 */
static int aligned(const void *p) {
    return (size_t)ALIGN(p) == (size_t)p;
}

/*
 * mm_checkheap
 */
void mm_checkheap(int verbose) {
}
