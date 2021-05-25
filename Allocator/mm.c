/*
 * Name: Shirley(Shiyang) Li
 * UID:  u1160160
 * mm.c - The memory-efficient malloc package.
 * 
 * In my approach, a block has a header to store the size of the block, as well as 
 * an "allocated" bit, and also uses a footer to store the size of the block. Also,
 * it uses a prolog block and a terminator block. In order to implement a quick and
 * efficient memory allocation, I chose to perform coalescing, use first-fit placement
 * and exlicit free list, and unmap unused pages.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/* always use 16-byte alignment */
#define ALIGNMENT 16

/* the overhead of page is always 32 with prolog block and terminator */
#define PAGE_OVERHEAD 32

/* the allocation granularity is a power of 2 and at least 4096 */
#define ALLOC_GRANULARITY 4096

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

/* rounds up to the nearest multiple of mem_pagesize() */
#define PAGE_ALIGN(size) (((size) + (mem_pagesize()-1)) & ~(mem_pagesize()-1))

// This assumes you have a struct or typedef called "block_header" and "block_footer"
#define OVERHEAD (sizeof(block_header)+sizeof(block_footer))

// Given a payload pointer, get the header or footer pointer
#define HDRP(bp) ((char*)(bp) - sizeof(block_header))
#define FTRP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)) - OVERHEAD)

// Given a payload pointer, get the next or previous payload pointer
#define NEXT_BLKP(bp) ((char*)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char*)(bp) - GET_SIZE((char*)(bp)-OVERHEAD))

// ******These macros assume you are using a size_t for headers and footers ******
// Given a pointer to a header, get or set its value
#define GET(p) (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (val))

// Combine a size and alloc bit
#define PACK(size, alloc) ((size) | (alloc))

// Given a header pionter, get the alloc or size
#define GET_ALLOC(p) (GET(p) & 0x1)
#define GET_SIZE(p)  (GET(p) & ~0xF)

// Header and footer for block
typedef size_t block_header;
typedef size_t block_footer;

/* node for the free list using doubly linked list*/
typedef struct node
{
  struct node* pre;
  struct node* next;
}node;

struct node* list_header;
size_t initial_page;

// ******helper functions******
// Set a block to allocated
static void set_allocated(void* b, size_t size);

// Request more memory by calling mem_map 
static void extend(size_t s);

// Coalesce a free block if applicable
static void* coalesce(void* bp);

// Find the next free block by using first fit method
static void* first_fit(size_t s);

// Add node to the free list
static void add_list_node(void* bp);

// Remove node from the  free list
static void remove_list_node(void* bp);


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  list_header = NULL;//initialize the free list
  return 0;
}


/* 
 * mm_malloc - Allocate a block by using bytes from current_avail,
 *     grabbing a new page if necessary.
 */
void* mm_malloc(size_t size)
{
  size_t new_size = ALIGN(size + OVERHEAD); 
  void* p = first_fit(new_size);//To check our free_list to see if we have a block on the current page to allocate
  
  if(p == NULL)//If do not find a free block, request more memory
  {
    extend(new_size);
    p = list_header;
  }

  set_allocated(p, new_size);//allocate

  return p;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void* ptr)
{
  size_t ptr_size = GET_SIZE(HDRP(ptr));//get the size of a header pointer, ptr
  PUT(HDRP(ptr), PACK(ptr_size, 0));//set the size and alloc bit to the header 
  PUT(FTRP(ptr), PACK(ptr_size, 0));//set the size and alloc bit to the footer

  void* new_ptr = coalesce(ptr);//coalesce the freed block

  //Check if the block that needs to be freed is the whole page by identifying
  //its prolog block and its terminator block
  if(((GET_SIZE(new_ptr - 3 * 8)) == OVERHEAD) && ((GET_SIZE(FTRP(new_ptr) + 8) == 0))) 
  {
    remove_list_node(new_ptr);
    size_t page_size = GET_SIZE(HDRP(new_ptr)) + PAGE_OVERHEAD;
    mem_unmap(new_ptr-PAGE_OVERHEAD, page_size);//mem_unmap for efficiency
  }
}

// ******Recommended helper functions******

/* These functios will provide a high-level recommended structure to your 
program.
 * Fill them in as needed, and create additional helper functions 
   depending on your design.
 */

/* Set a block to allocated
 *  Update block headers/footers as needed
 *  Update free list if applicable
 *  Split block if applicable
 */
static void set_allocated(void* b, size_t size) 
{
  size_t pre_size = GET_SIZE(HDRP(b));//get the size of the free block
  size_t left_size = pre_size - size;//the size left after allocating
  remove_list_node(b);//update free list

  // If applicable split the block 
  if(left_size > PAGE_OVERHEAD) 
  {
    // Update block header and footer for allocated part
    PUT(HDRP(b), PACK(size, 1));
    PUT(FTRP(b), PACK(size, 1));
    // Update the new block's header and footer after splitting
    PUT(HDRP(NEXT_BLKP(b)), PACK(left_size, 0));
    PUT(FTRP(NEXT_BLKP(b)), PACK(left_size, 0));
    add_list_node(NEXT_BLKP(b));//update free list
  }
  else 
  {//update block header and footer with pre_size
    PUT(HDRP(b), PACK(pre_size, 1));   
    PUT(FTRP(b), PACK(pre_size, 1));
  }
}

/*
 * Request more memory by calling mem_map
 *  Initialize the new chunk of memory as applicable
 *  Update free list if applicable
 */
static void extend(size_t s) 
{
  size_t init_size = PAGE_ALIGN((initial_page * 2) + PAGE_OVERHEAD);
  size_t size;

  if(init_size >= ALLOC_GRANULARITY * 100)
  {
    size = PAGE_ALIGN(initial_page + PAGE_OVERHEAD);
  }
  else
  {
    size = PAGE_ALIGN(init_size + PAGE_OVERHEAD);
    initial_page = size; 
  }
  
  void* bp = mem_map(size);

  // Prolog
  PUT(bp, 0);                     
  bp +=8;
  PUT(bp, PACK(16, 1));
  bp +=8;
  PUT(bp, PACK(16, 1));
  bp +=8;
  // Header
  PUT(bp, PACK(size-PAGE_OVERHEAD, 0));           
  bp+=8;
  // Footer
  PUT(FTRP(bp), PACK(size-PAGE_OVERHEAD, 0));     
  // Terminator
  PUT((FTRP(bp)+8), PACK(0,1));
  
  add_list_node(bp);//update free list
}

/* Coalesce a free block if applicable
 *  Returns pointer to new coalesced block
 */
static void* coalesce(void* bp) 
{
  size_t pre_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if ((pre_alloc == NULL) && (next_alloc != NULL))//pre is empty 
  {
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
  else if ((pre_alloc != NULL) && (next_alloc == NULL))//next is empty 
  {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    remove_list_node(NEXT_BLKP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    add_list_node(bp);
  }
  else if ((pre_alloc != NULL) && (next_alloc != NULL))
  {
    add_list_node(bp);
  }
  else//both empty 
  {
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
    remove_list_node(NEXT_BLKP(bp));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  return bp;
}

/*
 * Find the next available block by using first fit method
 */
static void *first_fit(size_t s) 
{
  node* current_node = list_header;

  while (current_node != NULL) 
  {
    if (GET_SIZE(HDRP(current_node)) >= s)
    {
      return (void*)current_node;
    }
    current_node = (*current_node).next;
  }
  return NULL;
}

/*
 * Add node to the beggining of the free list
 */
static void add_list_node(void* bp) 
{
  node* add_node = (node*)bp;

  // Make next of add node as head and previous as NULL
  (*add_node).next = list_header;
  (*add_node).pre = NULL;

  // Change pre of head node to add node
  if(list_header != NULL)
  {
    (*list_header).pre = add_node;
  }

  // Move the head to point to the add node 
  list_header = add_node;
}

/*
 * Remove node from the free list
 */
static void remove_list_node(void* bp) 
{
  node* remove_node = (node*)bp;
  
  // If node to be removed is head node
  if (list_header == remove_node)
  { 
    list_header = (*remove_node).next; 
  }
  // Change next only if node to be removed is not the last node
  if ((*remove_node).next != NULL) 
  {
    (*(*remove_node).next).pre = (*remove_node).pre; 
  }
  // Change prev only if node to be removed is not the first node
  if ((*remove_node).pre != NULL) 
  {
    (*(*remove_node).pre).next = (*remove_node).next; 
  }
}

