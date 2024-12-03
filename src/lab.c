#include "lab.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

//Converts bytes to its power of k (substitute for log base 2)
size_t btok(size_t bytes)
{
    //total size = size of the block + size of the header
    size_t total_size = bytes + sizeof(struct avail);
    size_t K = SMALLEST_K;

    //keep shifting bits till it find it
    //UINT64_C(1) << K so that it is consistent across platforms
    while ((UINT64_C(1) << K) < total_size)
    {
        K++;
        if (K > MAX_K)
            break;
    }
    return K;
}

// Find the buddy of a given pointer and kval relative to the base address
struct avail *buddy_calc(struct buddy_pool *pool, struct avail *buddy)
{
    /*
    From the example in Knuth’s book we can calculate the buddy of a block of size 16. 
    With the table above we know that a block of size 16 is K=4 
    so we will have 4 zeros at the right. 
    Using the XOR operator we can calculate the buddy as follows:
    */
    
    size_t offset = (uint8_t *)buddy - (uint8_t *)pool->base; // current buddy location
    size_t block_size = UINT64_C(1) << buddy->kval;// get the value for the XOR operation as shown by shane
    size_t buddy_offset = offset ^ block_size;//XOR operator if its forward or behind
    struct avail *buddy_ptr = (struct avail *)((uint8_t *)pool->base + buddy_offset); // use offset to get the buddy
    return buddy_ptr;
}

// Allocates a block of size bytes of memory
void *buddy_malloc(struct buddy_pool *pool, size_t size)
{

    // start off with basic checks
    if (size == 0 || pool == NULL)
    {
        return NULL;
    }

    //now get minimum size of K in order to store the block
    size_t K = btok(size);

    // conditinal check to see if the file is larger than the max size
    if (K > pool->kval_m)
    {
        /*
        If the memory cannot be allocated, then your buddy_calloc or buddy_malloc functions 
        should set errno to ENOMEM (which is defined in errno.h header file) and return NULL.
        */
        errno = ENOMEM;
        return NULL;
    }

    size_t i = K;
    //look through all the blocks to find the first available block//possible optimizatoin is find best fit
    while (i <= pool->kval_m && pool->avail[i].next == &pool->avail[i])
    {
        i++;
    }
    
    //exiti condition, even though the memory can support it, its either too fragmented or is filled with other blocks
    if (i > pool->kval_m)
    {
        errno = ENOMEM;
        return NULL;
    }

    //Lets just go ahead and use that block that works
    struct avail *block = pool->avail[i].next;
    block->prev->next = block->next;
    block->next->prev = block->prev;

    while (i > K)
    {
        // Split block
        i--;
        size_t size_i = UINT64_C(1) << i;
        struct avail *buddy = (struct avail *)((uint8_t *)block + size_i);

        //create the buddy block
        buddy->tag = BLOCK_AVAIL;
        buddy->kval = i;
        buddy->next = &pool->avail[i];
        buddy->prev = &pool->avail[i];

        //don't initialize the block yet, becuase we might need to split it again

        // Insert buddy into free list
        buddy->next = pool->avail[i].next;
        buddy->prev = &pool->avail[i];
        pool->avail[i].next->prev = buddy;
        pool->avail[i].next = buddy;

        // Update block
        block->kval = i;
    }

    // Mark block as reserved
    block->tag = BLOCK_RESERVED;
    block->next = NULL;
    block->prev = NULL;

    // Return pointer to memory after the header
    return (void *)(block + 1);
}

// Frees a previously allocated block of memory
void buddy_free(struct buddy_pool *pool, void *ptr)
{
    if (ptr == NULL || pool == NULL)
    {
        return;
    }

    // Get the block header
    struct avail *block = (struct avail *)ptr - 1;
    size_t K = block->kval;
    struct avail *buddy;

    block->tag = BLOCK_AVAIL;

    while (K <= pool->kval_m)
    {
        // Find buddy
        buddy = buddy_calc(pool, block);

        if (buddy->tag != BLOCK_AVAIL || buddy->kval != K)
        {
            // Buddy is not available or not the same size
            break;
        }

        // Remove buddy from free list
        buddy->prev->next = buddy->next;
        buddy->next->prev = buddy->prev;

        // Determine new block address
        if (block > buddy)
        {
            // Ensure block has the lower address
            struct avail *temp = block;
            block = buddy;
            buddy = temp;
        }

        // Update block's kval
        block->kval = K + 1;

        K = block->kval;
    }

    // Insert block into free list
    block->next = pool->avail[K].next;
    block->prev = &pool->avail[K];
    pool->avail[K].next->prev = block;
    pool->avail[K].next = block;
}

// Changes the size of the memory block pointed to by ptr
void *buddy_realloc(struct buddy_pool *pool, void *ptr, size_t size)
{   
    //edge cases
    if (pool == NULL)
        return NULL;

    if (ptr == NULL)
        return buddy_malloc(pool, size);

    
    // Get the block header to find out if we need to reallocate
    struct avail *block = (struct avail *)ptr - 1;
    size_t old_size = UINT64_C(1) << block->kval;
    size_t new_size = size + sizeof(struct avail);


    // if its not bigger then return the same memory block
    if (new_size <= old_size)
    {
        // No need to allocate new block
        return ptr;
    }
    else
    {
        //delete old block
        //allocate new block
        void *new_ptr = buddy_malloc(pool, size);
        if (new_ptr == NULL)
            return NULL;

        //just use mem copy to get old data to new block
        memcpy(new_ptr, ptr, old_size - sizeof(struct avail));
        buddy_free(pool, ptr);
        return new_ptr;
    }
}

// Initialize a new memory pool using the buddy algorithm
void buddy_init(struct buddy_pool *pool, size_t size)
{
    if (pool == NULL)
        return;

    size_t K;
    if (size == 0)
        K = DEFAULT_K;
    else
    {
        K = SMALLEST_K;
        while ((UINT64_C(1) << K) < size)
        {
            K++;
            if (K > MAX_K)
                break;
        }
    }

    if (K < MIN_K)
    {
        K = MIN_K;
    }
    pool->kval_m = K;
    pool->numbytes = UINT64_C(1) << K;

    // Allocate memory using mmap
    //https://stackoverflow.com/questions/34042915/what-is-the-purpose-of-map-anonymous-flag-in-mmap-system-call
    //https://man7.org/linux/man-pages/man2/mmap.2.html
    pool->base = mmap(NULL, pool->numbytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    if (pool->base == MAP_FAILED)
    {
        // mmap failed
        pool->base = NULL;
        return;
    }

    // Initialize avail array
    for (size_t i = 0; i <= pool->kval_m; i++)
    {
        pool->avail[i].tag = BLOCK_UNUSED;
        pool->avail[i].kval = i;
        pool->avail[i].next = &pool->avail[i];
        pool->avail[i].prev = &pool->avail[i];
    }

    // Initialize the single large block
    struct avail *initial_block = (struct avail *)pool->base;
    initial_block->tag = BLOCK_AVAIL;
    initial_block->kval = pool->kval_m;

    // Insert the initial block into the avail list
    initial_block->next = &pool->avail[pool->kval_m];
    initial_block->prev = &pool->avail[pool->kval_m];
    pool->avail[pool->kval_m].next = initial_block;
    pool->avail[pool->kval_m].prev = initial_block;
}

// Inverse of buddy_init
void buddy_destroy(struct buddy_pool *pool)
{
    if (pool == NULL || pool->base == NULL)
    {
        return;
    }

    // Unmap the memory
    munmap(pool->base, pool->numbytes);

    // Reset the pool structure
    pool->base = NULL;
    pool->numbytes = 0;
    pool->kval_m = 0;
}



int myMain(int argc, char **argv)
{
    struct buddy_pool pool;
    size_t bytes = UINT64_C(1) << MIN_K;
    buddy_init(&pool, bytes);

    size_t ask = bytes - sizeof(struct avail);
    void *mem = buddy_malloc(&pool, 1);
    print_buddy_system(&pool);

    buddy_free(&pool, mem);
    buddy_destroy(&pool);
}


static int is_block_free(struct buddy_pool *pool, void *addr, size_t kval)
{
    struct avail *current = pool->avail[kval].next;
    while (current != &pool->avail[kval])
    {
        if ((void *)current == addr)
        {return 1;}
        current = current->next;
    }
    return 0; 
}

// Recursive function to print the state of a block and its sub-blocks
static long unsigned int print_buddy_block(struct buddy_pool *pool, void *addr, size_t kval, int indent_level)
{
    if (is_block_free(pool, addr, kval))
    {   
        for (int i = 0; i < indent_level; i++)
            printf("  ");
        ///printf("%p: ", addr);
        printf("%lu byte bock AVAIL\n", (UINT64_C(1) << kval));
        return 0;
    }
    else if (kval == SMALLEST_K)
    {
        // Block is allocated and cannot be split further
        //printf("%lu byte block RESERVED\n", (UINT64_C(1) << kval));
        return UINT64_C(1) << kval;
    }
    else
    {
        size_t half_size = UINT64_C(1) << (kval - 1);

        void *left_child = addr;
        void *right_child = (void *)((uint8_t *)addr + half_size);

        // Recursively print the left and right sub-blocks
        long unsigned int a = print_buddy_block(pool, left_child, kval - 1, indent_level + 1);
        long unsigned int b = print_buddy_block(pool, right_child, kval - 1, indent_level + 1);

        if (a != 0 && a == b)
        {
            return a + b;
        }

        if (a != 0)
        {
            for (int i = 0; i < indent_level + 1; i++)
            printf("  ");
            printf("%lu byte block RESERVED\n", a);
        }
        if (b != 0)
        {
            for (int i = 0; i < indent_level + 1; i++)
            printf("  ");
            printf("%lu byte block RESERVED\n", b);
        }
        return 0;



    }
}

// Function to print the entire buddy memory system
void print_buddy_system(struct buddy_pool *pool)
{
    if (pool == NULL || pool->base == NULL)
    {
        printf("Pool is not initialized.\n");
        return;
    }
    printf("Buddy memory system:\n");
    print_buddy_block(pool, pool->base, pool->kval_m, 0);
}