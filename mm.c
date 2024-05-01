/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Kim Jaehak",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/**
 *  추가한 부분 WSIZE, DSIZE를 변화시켜도 성능에 변화가 없음.
*/
#define WSIZE 4 // word size 4, 16
#define DSIZE 8 // double word size 8, 32
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc)) // 사이즈와 할당 여부를 합침

#define GET(p) (*(unsigned int*)(p))

#define PUT(p, val) (*(unsigned int*)(p) = (val)) // p에 val을 넣음

#define GET_SIZE(p) (GET(p) & ~0x7) // p의 사이즈를 가져옴

#define GET_ALLOC(p) (GET(p) & 0x1) // p의 할당 여부를 가져옴

#define HDRP(bp) ((char*)(bp) - WSIZE) // bp의 헤더 주소를 계산. bp는 메모리 블록의 첫 주소를 가리킴.

// GET_SIZE(HDRP(bp)): bp의 헤더에 저장된 사이즈를 가져옴
// DSIZE: footer의 크기.
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 푸터는 메모리 플록의 끝 부분에 위치. bp의 푸터 주소를 계산

#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(((char*)(bp) - WSIZE))) // 다음 블록의 주소를 계산

#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE(((char*)(bp) - DSIZE))) // 이전 블록의 주소를 계산

static void* extend_heap(size_t words);
static void* coalesce(void* bp);
static void* find_fit(size_t asize);
static void place(void* bp, size_t asize);

static char* heap_listp; // heap의 시작을 가리킴
static char* next_heap_listp; // next_fit에서 사용하기 위한 포인터

/* 
 * mm_init - initialize the malloc package.
 */
 int mm_init(void){
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void*)-1){
        return -1;
    }
    
    // heap의 초기화
    PUT(heap_listp, 0);// 패딩
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));// 프롤로그 헤더
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));// 프롤로그 푸터
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));// 에필로그 헤더
    heap_listp += (2*WSIZE);// heap_listp 위치를 프롤로그 헤더 뒤로 옮김.
    next_heap_listp = heap_listp;// next_fit에서 사용하기 위해 초기 포인터 위치를 넣어줌.

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    return 0;
}

 void* extend_heap(size_t words){ // heap을 확장시킴
    char* bp; // 새로운 블록의 포인터
    size_t size;
    //Double word 제한 조건을 사용하기 위해 8byte의 메모리만 할당
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;// words가 홀수이면 짝수로 만듦
    if((long)(bp = mem_sbrk(size)) == -1){ // 반환된 값이 -1이면 메모리 할당 실패
        return NULL;
    }
    PUT(HDRP(bp), PACK(size, 0));// size만큼의 가용 블록의 헤더 생성
    PUT(FTRP(bp), PACK(size, 0));// size만큼의 가용 블록의 푸터 생성
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));// 새로운 블록 다음에 있는 블록의 헤더
    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
 void *mm_malloc(size_t size) // 메모리 할당
{
    size_t asize; // 조정된 블록 사이즈
    size_t extendsize; // heap을 확장시킬 사이즈
    char *bp;

    if(size == 0){
        return NULL;
    }
    // 블록 사이즈 조정
    if(size <= DSIZE){
        asize = 2*DSIZE;
    }else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    // 조정한 크기(asize)에 맞는 가용 블록을 찾음
    if((bp = find_fit(asize)) != NULL){
        place(bp, asize);
        next_heap_listp = bp; // next_fit을 위해 next_heap_listp를 bp로 설정
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL){
        return NULL;
    }
    place(bp, asize);
    next_heap_listp = bp; // next_fit을 위해 next_heap_listp를 bp로 설정
    return bp;
}

/*
 * mm_free - Freeing a block does nothing. 고칠 것 없음.
 */
 void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

 void* coalesce(void* bp){ // free된 블록들을 합침
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp)); 
    if(prev_alloc && next_alloc){ // 이전 블록과 다음 블록이 모두 할당되어 있을 때
        return bp;
    }
    else if(prev_alloc && !next_alloc){ // 이전 블록은 할당되어 있고 다음 블록은 할당되어 있지 않을 때
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if(!prev_alloc && next_alloc){ // 이전 블록은 할당되어 있지 않고 다음 블록은 할당되어 있을 때
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else{ // 이전 블록과 다음 블록이 모두 할당되어 있지 않을 때
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
//  void *mm_realloc(void *ptr, size_t size)
// {
//     void *newptr;
//     size_t copySize;
//     newptr = mm_malloc(size);
//     if(newptr == NULL){
//         return NULL;
//     }
//     copySize = GET_SIZE(HDRP(ptr));
//     if(size < copySize){
//         copySize = size;
//     }
//     memcpy(newptr, ptr, copySize);
//     mm_free(ptr);
//     return newptr;
// }
/**
 * 새로 크기를 조정하려는 블록이 가용 블록일 경우 새로 메모리 할당을 안해도 됨
 * 헤더, 푸터의 사이즈 정보만 갱신
*/
void *mm_realloc(void *ptr, size_t size){
    void *oldptr = ptr;//이전 포인터
    void *newptr;//새로운 포인터

    size_t originsize = GET_SIZE(HDRP(oldptr));//이전 포인터의 사이즈
    size_t newsize = size + DSIZE;//새로운 사이즈

    if(newsize <= originsize){
        return oldptr;
    }else{
        size_t addSize = originsize + GET_SIZE(HDRP(NEXT_BLKP(oldptr)));//다음 블록의 사이즈를 더함
        if(!GET_ALLOC(HDRP(NEXT_BLKP(oldptr))) && (addSize >= newsize)){//다음 블록이 가용 블록이고, 합쳐서 충분한 사이즈라면
            PUT(HDRP(oldptr), PACK(addSize, 1));//합친 블록의 헤더를 변경
            PUT(FTRP(oldptr), PACK(addSize, 1));//합친 블록의 푸터를 변경
            return oldptr;
    }else{
        newptr = mm_malloc(newsize);//새로운 사이즈만큼 메모리 할당
        if(newptr == NULL){
            return NULL;
        }
        memmove(newptr, oldptr, originsize);//memmove를 할 경우 메모리가 겹치더라도 문제가 없음
        mm_free(oldptr);//이전 포인터의 메모리 해제
        return newptr;
    }
    
    }
}

//  void* find_fit(size_t asize){ // first_fit
//     void* bp;
//     for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
//         if(!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))){
//             return bp;
//         }
//     }
//     return NULL;
// }

void* find_fit(size_t aszie){ //best_fit. heap 전체를 순회하면서 들어갈 수 있는 공간 중 가장 작은 공간을 할당
    void* bp;
    void* best_bp = NULL;
    size_t min_size = 0;
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        if(!GET_ALLOC(HDRP(bp)) && (aszie <= GET_SIZE(HDRP(bp)))){ // 할당되지 않은 블록 && asize보다 크거나 같을 때
            if(min_size == 0 || GET_SIZE(HDRP(bp)) < min_size){
                min_size = GET_SIZE(HDRP(bp));
                best_bp = bp;
            }
        }
    }
    return best_bp;
}
//next_fit. 이전에 할당한 블록의 다음 블록부터 탐색
// void* find_fit(size_t size){

//     char *bp;
//     // next_heap_listp의 다음 블록부터 탐색
//     for (bp = NEXT_BLKP(next_heap_listp); GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
//         if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
//             return bp;
//         }
//     }
//     // 처음부터 next_heap_listp까지 탐색
//     for (bp = heap_listp; bp <= next_heap_listp; bp = NEXT_BLKP(bp)) {
//         if (!GET_ALLOC(HDRP(bp)) && (size <= GET_SIZE(HDRP(bp)))) {
//             return bp;
//         }
//     }

//     return NULL;
// }




 void place(void* bp, size_t asize){ // 블록을 할당
    size_t csize = GET_SIZE(HDRP(bp));
    if((csize - asize) >= (2*DSIZE)){
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        next_heap_listp = bp;// next_fit을 위해 next_heap_listp를 bp로 설정
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        next_heap_listp = NEXT_BLKP(bp);// next_fit을 위해 next_heap_listp를 bp로 설정
    }
}







