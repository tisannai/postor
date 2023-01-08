#ifndef POSTOR_H
#define POSTOR_H

/**
 * @file   postor.h
 * @author Tero Isannainen <tero.isannainen@gmail.com>
 * @date   Sun Jan  8 15:32:51 2023
 *
 * @brief  Postor - Growing Pointer Storage.
 *
 */

#ifndef SIXTEN_STD_INCLUDE
#include <stdlib.h>
#include <stdint.h>
#endif


#ifndef POSTOR_NO_ASSERT
#include <assert.h>
/** Default assert. */
#define po_assert assert
#else
/** Disabled assertion. */
#define po_assert( cond ) (void)( ( cond ) || po_void_assert() )
#endif


#ifndef PO_DEFAULT_SIZE
/** Default size for Postor. */
#define PO_DEFAULT_SIZE 16
#endif

/** Minimum size for pointer array. */
#define PO_MIN_SIZE 2

/** Outsize Postor index. */
#define PO_NOT_INDEX -1


/** Size type. */
typedef uint64_t po_size_t;

/** Position type (negative for end references). */
typedef int64_t po_pos_t;

/** Data pointer type. */
typedef void* po_d;


/**
 * Postor struct.
 *
 * NOTE: "size" is strictly forbidden to be used directly, because it
 * includes the "local/non-local" information in LSB. Use po_size()
 * instead.
 */
struct po_struct_s
{
    po_size_t size;      /**< Reservation size for data (N mod 2==0). */
    po_size_t used;      /**< Used count for data. */
    po_d*     data;      /**< Pointer array. */
};
typedef struct po_struct_s po_s; /**< Postor struct. */
typedef po_s*              po_t; /**< Postor. */
typedef po_t*              po_p; /**< Postor reference. */


/** Resize function type. */
typedef int ( *po_resize_fn_p )( po_t po, po_size_t new_size, po_d state );

/** Compare function type. */
typedef int ( *po_compare_fn_p )( const po_d a, const po_d b );


/** Iterate over all items. */
#define po_each( po, iter, cast )                                       \
    for ( po_size_t po_idx = 0;                                         \
          ( po_idx < ( po )->used ) && ( iter = ( cast )( po )->data[ po_idx ] ); \
          po_idx++ )

/** Item at index with casting to target type. */
#define po_item( po, idx, cast ) ( ( cast )( po )->data[ ( idx ) ] )

/** Item at index with casting to target type. */
#define po_assign( po, idx, item ) ( ( po )->data[ ( idx ) ] = ( item ) )

/** Shift item from first index. */
#define po_shift( po ) po_delete_at( po, 0 )

/** Insert item to first index. */
#define po_unshift( po, item ) po_insert_at( po, 0, item )

/** Postor unit size, e.g. use as unit for local allocations. */
#define po_unitsize ( sizeof( po_d ) )

/**
 * Make allocation for local (stack) Postor. NOTE: User must make sure
 * the allocation size is valid.
 */
#define po_local_use( ps, buf, size )                           \
    po_s ps;                                                    \
    /* Legalize the size, i.e. make it even. */                 \
    po_d buf[ ((((size-1)/2)+1)*2)  ];                          \
    po_use( &ps, buf, ((((size-1)/2)+1)*2) )



/* Short names for functions. */

/** @cond postor_none */
#define ponew po_new
#define posiz po_new_sized
#define popag po_new_pages
#define podes po_destroy
#define pores po_resize
#define pouse po_used
#define porss po_size
#define podat po_data
#define pofst po_first
#define polst po_last
#define ponth po_nth
#define poemp po_is_empty
#define pofll po_is_full
#define popsh po_push
#define popop po_pop
#define poadd po_add
#define porem po_remove
#define porst po_reset
#define podup po_duplicate
#define poswp po_swap
#define poins po_insert_at
#define poiif po_insert_if
#define podel po_delete
#define pofnd po_find
#define pofnw po_find_with
#define poalc po_alloc_bytes

#define pofor po_for_each
/** @endcond postor_none */



/* clang-format off */

#ifdef POSTOR_USE_MEM_API

/*
 * POSTOR_USE_MEM_API allows to use custom memory allocation functions,
 * instead of the default: po_malloc, po_free, po_realloc.
 *
 * If POSTOR_USE_MEM_API is used, the user must provide implementation for
 * the above functions and they must be compatible with malloc
 * etc. Also Postor assumes that po_malloc sets all new memory to
 * zero.
 *
 * Additionally user should compile the library by own means.
 */

extern void* po_malloc( size_t size );
extern void  po_free( void* ptr );
extern void* po_realloc( void* ptr, size_t size );

#else /* POSTOR_USE_MEM_API */


#    if SIXTEN_USE_MEM_API == 1

#        define po_malloc  st_alloc
#        define po_free    st_del
#        define po_realloc st_realloc


#    else /* SIXTEN_USE_MEM_API == 1 */

/* Default to common memory management functions. */

/** Reserve memory. */
#        define po_malloc( size ) calloc( size, 1 )

/** Release memory. */
#        define po_free free

/** Re-reserve memory. */
#        define po_realloc realloc

#    endif /* SIXTEN_USE_MEM_API == 1 */

#endif /* POSTOR_USE_MEM_API */

/* clang-format on */


/* ------------------------------------------------------------
 * Create and destroy:
 */


/**
 * Create Postor container with default size (PO_DEFAULT_SIZE).
 * 
 * If po is NULL, descriptor is allocated from heap. This
 * type of descriptor must be freed by the user after use.
 *
 * NOTE: po->data is NULL, if allocation error occurred.
 *
 * @param po  Postor or NULL.
 *
 * @return Postor.
 */
po_t po_new( po_t po );


/**
 * Create Postor with size.
 * 
 * If po is NULL, descriptor is allocated from heap. This
 * type of descriptor must be freed by the user after use.
 *
 * Minimum size is PO_MINSIZE. Size is forced if given size is too
 * small.
 *
 * NOTE: po->data is NULL, if allocation error occurred.
 *
 * @param po   Postor.
 * @param size Initial size.
 *
 * @return Postor.
 */
po_t po_new_sized( po_t po, po_size_t size );


/**
 * Create Postor with page (4k) aligned size.
 * 
 * If po is NULL, descriptor is allocated from heap. This
 * type of descriptor must be freed by the user after use.
 *
 * Paged Postor is typically used for temporary scratch pad memory
 * area.
 *
 * @param count Page count.
 *
 * @return Postor.
 */
po_t po_new_pages( po_t po, po_size_t count );


/** 
 * Initialize the Postor as empty.
 * 
 * If po is NULL, descriptor is allocated from heap. This
 * type of descriptor must be freed by the user after use.
 * 
 * @param po Postor descriptor or NULL.
 * 
 * @return Initialized Postor.
 */
po_t po_new_descriptor( po_t po );


/**
 * Use existing memory allocation for Postor.
 *
 * @param mem  Allocation for Postor.
 * @param size Allocation size (in po_d units).
 * 
 * @return Initialized Postor.
 */
po_t po_use( po_t po, po_d* mem, po_size_t size );


/**
 * Destroy Postor.
 *
 * @param po Postor.
 *
 * @return NULL.
 */
po_t po_destroy( po_t po );


/**
 * Destroy Postor storage (data).
 *
 * @param po Postor.
 */
void po_destroy_storage( po_t po );


/**
 * Resize Postor to new_size.
 *
 * If new_size is smaller than usage, no action is performed.
 *
 * @param po       Postor.
 * @param new_size Requested size.
 */
void po_resize( po_t po, po_size_t new_size );


/**
 * Push item to end of container.
 *
 * po_push() can be used for efficient stack operations. Postor is
 * resized if these is no space available.
 *
 * @param po   Postor.
 * @param item Item to push.
 */
void po_push( po_t po, po_d item );


/**
 * Pop item from end of container.
 *
 * po_pop() can be used for efficient stack operations. No operation
 * is Postor is empty or Postor is NULL.
 *
 * @param po Postor.
 *
 * @return Popped item (or NULL).
 */
po_d po_pop( po_t po );


/**
 * Drop number of items from end.
 *
 * If count is bigger than the number of items, then maximum number of
 * items are removed.
 *
 * @param po    Postor.
 * @param count Item count.
 *
 * @return Number of items removed.
 */
po_size_t po_drop( po_t po, po_size_t count );


/**
 * Add item to end of container.
 *
 * If "po->data" is NULL, Postor container is created and item is
 * added. Postor is resized if these is no space available.
 *
 * @param po   Postor.
 * @param item Item to add.
 */
void po_add( po_t po, po_d item );


/**
 * Remove item from end of container.
 *
 * If container becomes empty, it will be destroyed.
 *
 * @param po Postor.
 *
 * @return Popped item (or NULL).
 */
po_d po_remove( po_t po );


/**
 * Reset Postor to empty.
 *
 * @param po Postor.
 */
void po_reset( po_t po );


/**
 * Clear Postor, i.e. reset and clear data.
 *
 * @param po Postor.
 */
void po_clear( po_t po );


/**
 * Duplicate Postor.
 *
 * @param po Postor to duplicate.
 *
 * @return Duplicated postor.
 */
po_s po_duplicate( po_t po );


/**
 * Swap item in Postor with given "item".
 *
 * @param po   Postor.
 * @param pos  Addressed item position.
 * @param item Item to swap in.
 *
 * @return Item swapped out.
 */
po_d po_swap( po_t po, po_pos_t pos, po_d item );


/**
 * Insert item to given position.
 *
 * Postor is resized if these is no space available.
 *
 * @param po   Postor.
 * @param pos  Position.
 * @param item Item to insert.
 */
void po_insert_at( po_t po, po_pos_t pos, po_d item );


/**
 * Insert item to given position.
 *
 * Postor is not resized. Item is not inserted if it does not fit.
 *
 * @param po   Postor.
 * @param pos  Position.
 * @param item Item to insert.
 *
 * @return 1 if item was inserted.
 */
int po_insert_if( po_t po, po_pos_t pos, po_d item );


/**
 * Delete item from position.
 *
 * Postor is not destroyed if last item is deleted.
 *
 * @param po  Postor.
 * @param pos Position.
 *
 * @return Item from delete position.
 */
po_d po_delete_at( po_t po, po_pos_t pos );


/**
 * Sort Postor items.
 *
 * @param po      Postor.
 * @param compare Compare function.
 */
void po_sort( po_t po, po_compare_fn_p compare );


/**
 * Allocate consecutive bytes from Postor.
 *
 * Allocation is converted to next Postor unit size, i.e. the minimum
 * allocation is one Postor unit. If Postor is full, NULL is returned
 * and Postor is not resized.
 *
 * @param po    Postor.
 * @param bytes Number of bytes to allocate.
 *
 * @return Pointer (or NULL).
 */
po_d po_alloc_bytes( po_t po, po_size_t bytes );



/* ------------------------------------------------------------
 * Queries:
 */


/**
 * Return count of container usage.
 *
 * @param po Postor.
 *
 * @return Usage count.
 */
po_size_t po_used( po_t po );


/**
 * Return Postor reservation size.
 *
 * @param po Postor.
 *
 * @return Reservation size.
 */
po_size_t po_size( po_t po );


/**
 * Return Postor reservation size in bytes.
 *
 * @param po Postor.
 *
 * @return Reservation size in bytes.
 */
po_size_t po_bytesize( po_t po );


/**
 * Return container data array.
 *
 * @param po Postor.
 *
 * @return Data array.
 */
po_d* po_data( po_t po );


/**
 * Return first item.
 *
 * @param po Postor.
 *
 * @return First item.
 */
po_d po_first( po_t po );


/**
 * Return last item.
 *
 * @param po Postor.
 *
 * @return Last item.
 */
po_d po_last( po_t po );


/**
 * Return nth item.
 *
 * @param po  Postor.
 * @param pos Item position.
 *
 * @return Indexed item.
 */
po_d po_nth( po_t po, po_pos_t pos );


/**
 * Return reference to nth item.
 *
 * @param po  Postor.
 * @param pos Item position.
 *
 * @return Referenct to indexed item.
 */
po_d* po_nth_ref( po_t po, po_pos_t pos );


/**
 * Return empty status.
 *
 * NOTE: Non-existing container does not mean empty.
 *
 * @param po Postor.
 *
 * @return 1 if empty, 0 if not.
 */
int po_is_empty( po_t po );


/**
 * Return full status.
 *
 * NOTE: Non-existing container does not mean full.
 *
 * @param po Postor.
 *
 * @return 1 if full, 0 if not.
 */
int po_is_full( po_t po );


/**
 * Find item from Postor.
 *
 * Direct address comparison (see: po_find_with).
 *
 * @param po   Postor.
 * @param item Item to find.
 *
 * @return Item index (or PO_NOT_INDEX).
 */
po_pos_t po_find( po_t po, po_d item );


/**
 * Find item from Postor using compare function.
 *
 * @param po      Postor.
 * @param compare Compare function.
 * @param ref     Item to find.
 *
 * @return Item index (or PO_NOT_INDEX).
 */
po_pos_t po_find_with( po_t po, po_compare_fn_p compare, po_d ref );


/**
 * Set Postor as local.
 *
 * @param po  Postor.
 * @param val Local (or not).
 */
void po_set_local( po_t po, int val );


/**
 * Return Postor local mode.
 *
 * @param po Postor.
 *
 * @return 1 if local (else 0).
 */
int po_get_local( po_t po );



/* ------------------------------------------------------------
 * Utilities:
 */


/**
 * Allocate number of pages of memory.
 *
 * Returned memory is cleared. If count is 0, return size of memory
 * page (of the system).
 *
 * @param count[in] Page count.
 * @param mem[out]  Reference to memory.
 *
 * @return Byte count for allocation (or page size when count=0).
 */
po_size_t po_alloc_pages( po_size_t count, po_d** mem );


void po_void_assert( void );


#endif
