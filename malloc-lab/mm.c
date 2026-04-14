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
#define DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

static void *extend_heap(size_t words);
static void *coalesce(void *bp);

static void *find_fit(size_t asize);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);

static void place(void *bp, size_t asize);


/* explicit 을 위한 함수들*/
void insert_free_block(void* bp);
void remove_free_block(void* bp);

#ifdef DEBUG
static void mm_checkheap(int lineno);
static void check_block(void *bp, int lineno);
static void check_free_list(int lineno);
static void check_heap_free_vs_list(int lineno);
#endif


/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "KRAFTON_JUNGLE-12-302-7-4",
    /* First member's full name */
    "Lee HaeGeon",
    /* First member's email address */
    "atgsisu@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic contants and macros */
#define WSIZE               4           /* Word and header/footer size (bytes) */
#define DSIZE               8           /* Double word size (bytes) */
#define CHUNKSIZE           (1 << 12)   /* Extend heap by this amount (bytes) */

#define MAX(x, y)           ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)   ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)              (*(unsigned int *) (p))
#define PUT(p, val)         (*(unsigned int *) (p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)         (GET(p) & (~0x07))
#define GET_ALLOC(p)        (GET(p) & (0x01))

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)            ((char*) (bp) - WSIZE)
#define FTRP(bp)            ((char*) (bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)       ((char*) (bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)       ((char*) (bp) - GET_SIZE((char *)(bp) - DSIZE))


/* ==========================================================
    MACRO for explicit
*/
#define MIN_FREE_BLOCK_SIZE     WSIZE * 2 + sizeof(void*) * 2

#define PRED_FIELD(bp)          ((char*) (bp))
#define SUCC_FIELD(bp)          ((char*) (bp) + sizeof(void*))

#define PRED(bp)                (*(void **)(PRED_FIELD(bp)))
#define SUCC(bp)                (*(void **)(SUCC_FIELD(bp)))

#define SET_PRED(bp, p)         (*(void **)(PRED_FIELD(bp))) = (p)
#define SET_SUCC(bp, p)         (*(void **)(SUCC_FIELD(bp))) = (p)




void * heap_listp = NULL;
/** 
 * next_bp는 mm_init, place, free에서 관리해야 한다.
 * [x] mm_init에서는 extend 후 첫 번째 블록을 가리켜야 한다.
 * [x] allocate 후, next_bp는 항상 현재 할당한 블록의 다음 블록을 가리켜야 한다. -> 이렇게 될 경우 next_bp가 에필로그일 경우도 고려해야 함.
 * [x] free 후, next_bp는 현재 해제한 블록을 가리키거나 이전 블록을 가리킨다(prev block과 coalesce 되었을 경우).
 * [] next_fit으로 찾을 때 현재 블록부터 검색한다. epilogue에 도달했다면 next_bp 전까지 탐색한다. 탐색에 실패했다면 place할 block이 존재하지 않음으로 extend한다.
 */
void * next_bp = NULL;


/**
 * global variable for explicit
 */
void * free_listp = NULL;


/*
 * mm_init - initialize the malloc package.
 * @return 0 (success), -1(fail)
 */
int mm_init(void)
{
    /* 힙의 크기를 4 * WSIZE 만큼 늘린다. */
    heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void*) - 1) {
        return -1;
    }

    PUT(heap_listp, 0);                                         /* Aligment Padding */
    PUT(((char*) heap_listp) + (1 * WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(((char*) heap_listp) + (2 * WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(((char*) heap_listp) + (3 * WSIZE), PACK(0, 1));        /* Epilogue header */

    /* head_listp 포인터는 Prologue header와 footer 사이에 위치시키기 */
    heap_listp += (2 * WSIZE);
    
    int extend_size = MAX(CHUNKSIZE, MIN_FREE_BLOCK_SIZE);
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */ 
    if ((next_bp = extend_heap(extend_size/WSIZE)) == NULL) {
        return -1;
    }
    printf("sizeof bp pointer %d\n", sizeof(next_bp));
    #ifdef DEBUG
    check_block(next_bp, __LINE__);
    #endif
    return 0;
}

/*
 * mm_malloc - 
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t expendsize;
    char* bp;

    /* 이상한 요청 쳐내기 */
    if (size <= 0) {
        return  NULL;
    }
    /* 헤더, 푸터를 포함한 필요한 사이즈 asize 계산 */
    if (size <= DSIZE) {
        asize = DSIZE * 2;
    } else {
        asize = DSIZE * ((size 
                            + DSIZE         /* Header, Footer 공간 확보 */ 
                            + (DSIZE - 1)   /* DSIZE로 align 해주는 tactic */
                            ) / DSIZE);     
    }

    /* asize 크기 블록을 할당할 수 있는지 검색 */
    bp = find_fit(asize);
    if (bp != NULL) {
        place(bp, asize);
        return bp;
    }
    /* 없다면 큰 블록을 새롭게 할당받기 */
    expendsize = MAX(asize, CHUNKSIZE);
    expendsize = MAX(expendsize, MIN_FREE_BLOCK_SIZE);
    bp = extend_heap(expendsize / WSIZE);
    if (bp == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - 
 */
void mm_free(void *ptr)
{
    unsigned int size;
    size = GET_SIZE(HDRP(ptr));
    // mark its header and footer free
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    // call coalase
    next_bp = coalesce(ptr);
    // coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


static void *extend_heap(size_t words)
{
    char* bp;
    size_t size;
    
    /* size를 8의 배수(Double word)로 맞추도록 계산(words 반올림) */
    size = (words % 2 == 0) ? words * WSIZE : (words + 1) * WSIZE;

    /* mem_sbrk로 heap size를 늘린다. */
    bp = mem_sbrk(size);
    if (bp == (void *) - 1) {
        return NULL;
    }

    /* 기존의 Epilogue header를 새로운 block의 header로 만든다. */
    PUT(HDRP(bp), PACK(size, 0));

    /* 새로운 block의 footer를 설정한다. */
    PUT(FTRP(bp), PACK(size, 0));

    /* 새로운 Epilogue header를 설정한다. */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void *coalesce(void *bp)
{
    size_t is_prev_allocated = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t is_next_allocated = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    /* Case 1: prev allocated, next allocated */
    if (is_prev_allocated && is_next_allocated) {
        return bp;
    }
    
    /* Case 2: prev allocated, next free */
    else if (is_prev_allocated && !is_next_allocated) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return bp;
    }

    /* Case 3: prev free, next allocated */
    else if (!is_prev_allocated && is_next_allocated) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        return PREV_BLKP(bp);
    }

    /* Case 4: prev free, next free */
    else // (!is_prev_allocated && !is_next_allocated) {
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        return PREV_BLKP(bp);
    }

}


static void *find_fit(size_t asize)
{
    // return first_fit(asize);
    return next_fit(asize);
}


static void *first_fit(size_t asize)
{
    /* first fit을 구현했다. */
    /* heap_listp 를 사용한다. */
    char* bp = heap_listp;
    /* 거기서부터 해당하는 자리가 있는지 순차적으로 검색한다.*/
    bp = NEXT_BLKP(bp);
    while (1) {
        size_t size = GET_SIZE(HDRP(bp));
        if (size == 0) {
            break;
        }
        if (! GET_ALLOC(HDRP(bp)) && asize <= size) {
            return bp;
        }

        bp = NEXT_BLKP(bp);
    }
    /* 없다면 NULL을 반환한다. */
    return NULL;
}


static void *next_fit(size_t asize)
{
    char* cur_bp = next_bp;
    size_t size;
    // next_bp에서 epilogue까지 allocatable block을 찾는다.
    while ((size=GET_SIZE(HDRP(cur_bp))) != 0) {
        if (GET_ALLOC(HDRP(cur_bp)) == 0 && size >= asize) {
            return (void *) cur_bp;
        }
        cur_bp = NEXT_BLKP(cur_bp);
    }
    // list_headp에서 next_bp 전까지 alloactable block을 찾는다.
    cur_bp = heap_listp;
    while (cur_bp != next_bp) {
        size = GET_SIZE(HDRP(cur_bp));
        if (GET_ALLOC(HDRP(cur_bp)) == 0 && size >= asize) {
            return (void *) cur_bp;
        }
        cur_bp = NEXT_BLKP(cur_bp);
    }
    
    return NULL;    
}


static void place(void *bp, size_t asize)
{
    size_t original_size = GET_SIZE(HDRP(bp));
    
    if (original_size - asize < DSIZE * 2) {
        PUT(HDRP(bp), PACK(original_size, 1));
        PUT(FTRP(bp), PACK(original_size, 1));
    } else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(original_size - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(original_size - asize, 0));
    }
    next_bp = NEXT_BLKP(bp);
}


void insert_free_block(void* bp)
{
    SET_PRED(bp, NULL);
    SET_SUCC(bp, free_listp);

    if (free_listp != NULL) {
        SET_PRED(free_listp, bp);
    }
    
    free_listp = bp;
}


void remove_free_block(void* bp)
{
    
    /* edge case는 다음과 같음. 삭제하려는 block이
        1. header 인 경우
        2. trailer 인 경우
        3. 둘 다인 경우
    */

    // 3. 둘 다
    if (PRED(bp) == NULL && SUCC(bp) == NULL) {
        free_listp = NULL;
    }

    // 1. header
    else if (PRED(bp) == NULL) {
        free_listp = SUCC(bp);
        SET_PRED(free_listp, NULL);

    }
    // 2. trailer
    else if (SUCC(bp) == NULL) {
        SUCC(PRED(bp)) = NULL;
    }

    // 일반적인 case
    else {
        SUCC(PRED(bp)) = SUCC(bp);
        PRED(SUCC(bp)) = PRED(bp);
    }

    SET_PRED(bp, NULL);
    SET_SUCC(bp, NULL);
}



#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>

static void fail_check(int lineno, const char *msg, void *bp) {
    fprintf(stderr, "[CHECK FAIL] line=%d bp=%p: %s\n", lineno, bp, msg);
    exit(1);
}

static void check_block(void *bp, int lineno) {
    /* 
       - alignment
       - free block일 때 header/footer 일치
       - minimum block size
    */

    /* alignment check */
    if( ((long) bp & 0x7) != 0) {
        fail_check(lineno, "alignment가 일치하지 않습니다.", bp);
    }
    
    /* free block일 때 header/footer 일치 */
    if (GET_ALLOC(HDRP(bp)) == 0) {
        size_t header = GET(HDRP(bp));
        size_t footer = GET(FTRP(bp));

        if (header != footer) {
            fail_check(lineno, "header, footer가 일치하지 않습니다.", bp);
        }

        if (GET_SIZE(HDRP(bp)) < MIN_FREE_BLOCK_SIZE) {
           fail_check(lineno, "block 이 explicit memory allocation을 사용하기에는 너무 작습니다.", bp);
        }
    }

    
}

static void check_free_list(int lineno) {
    /* 
       - free list 순회 가능?
       - 같은 노드 2번 방문 안 함?
       - pred/succ local consistency
    */

    // heap 전체를 순회하면서 free block 개수 카운트
    int free_block_count = 0;
    int seen = 0;

    void * cur = heap_listp;
    void * prev = NULL;
    while (GET_SIZE(HDRP(cur)) != 0) {
        if (GET_ALLOC(HDRP(cur)) == 0) {
            free_block_count ++;
        }
        
        cur = NEXT_BLKP(cur);
    }

    cur = free_listp;
    /* while  안에 여러 if 문들을 둔다는 아이디어는 내가 처음 떠올리지는 못햇다. gpt의 구현을 힐끗 보고 혼자 쳐보기는 했다....*/
    while (cur != NULL) {
        if (seen >= free_block_count) {
            fail_check(lineno, "free list 순회가 비정상입니다. (cycle / duplicate / stale)", cur);
        }
        
        if (GET_ALLOC(HDRP(cur)) == 1) {
            fail_check(lineno, "free list에 allocated block이 존재합니다.", cur);
        }

        if (GET(HDRP(cur)) != GET(FTRP(cur))) {
            fail_check(lineno, "free block의 header, footer가 일치하지 않습니다.", cur);
        }
        // 같은 노드를 두 번 방문하는지 확인한다. -> 방문한 block의 주소를 적어놓고, 해당 주소가 중복되어서 나오는지 확인한다....?????? 어떻게 C로 구현하지?/? -> 방문 개수가 같으면 없다고 가정하자.

        // 모든 노드에 대해서 pred/succ local consistency를 확인한다.
        if (PRED(cur) != prev) {
            fail_check(lineno, "cur의 predecessor가 잘못 연결되어 있습니다.", cur);
        }
        if (SUCC(cur) != NULL && PRED(SUCC(cur)) != cur) {
            fail_check(lineno, "cur의 successor가 잘못 연결되어 있습니다.", cur);
        }

        prev = cur;
        cur = SUCC(cur);
        seen ++;
    }

    if (seen != free_block_count) {
        fail_check(lineno, "free list 순회가 비정상입니다. (cycle / duplicate / stale)", cur);
    }
    
}

static int is_in_free_list(void* target) {
    void* cur = free_listp;
    while (cur != NULL) {
        if (cur == target) {
            return 1;
        }
        cur = SUCC(cur);
    }

    return 0;
}

static int is_valid_free_block(void * target) {
    void * cur = heap_listp;

    while (GET_SIZE(HDRP(cur)) != 0) {
        if (cur == target) {
            return (GET_ALLOC(HDRP(cur)) == 0);
        }

        cur = NEXT_BLKP(cur);
    }

    return 0;
}


static void check_heap_free_vs_list(int lineno) {
    /* TODO:
       - heap의 free block들과 free list 원소 대응
       - 누락 / 중복 / stale block 검사
       - immediate coalescing이면 인접 free block 금지
    */
   void * cur = heap_listp;
   while (GET_SIZE(HDRP(cur)) != 0) {
        if (GET_ALLOC(HDRP(cur)) == 0) {
            if (!is_in_free_list(cur)) {
                fail_check(lineno, "힙에 존재하는 free block이 list에 존재하지 않습니다.", cur);
            }

            // coalesce 되어서 사라진 block이 free list에 존재하지 않았는지 확인(next 블록이 free 면 fail)
            if (GET_SIZE(HDRP(cur)) != 0 && GET_ALLOC(HDRP(NEXT_BLKP(cur))) == 0) {
                fail_check(lineno, "힙에 연속된 free block이 존재합니다.", cur);
            }
        }
        cur = NEXT_BLKP(cur);
   }

   cur = free_listp;
   
    // free list에 있는 block이 heap에 정상적으로 존재하는지 확인
    while (cur != NULL) {
        if (!is_valid_free_block(cur)) {
            fail_check(lineno, "free list에 비정상 free block이 존재합니다.", cur);
        }
        
        cur = SUCC(cur);
    }
}

static void mm_checkheap(int lineno) {
    void *bp;

    /* heap 전체 순회 */
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        check_block(bp, lineno);
    }

    check_free_list(lineno);
    check_heap_free_vs_list(lineno);
}
#endif