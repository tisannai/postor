/**
 * @file   postor.c
 * @author Tero Isannainen <tero.isannainen@gmail.com>
 * @date   Sun Jan  8 15:32:51 2023
 *
 * @brief  Postor - Growing Pointer Storage.
 *
 */

#define _POSIX_C_SOURCE 200112L

#include <string.h>
#include <unistd.h>

#include "postor.h"


/* clang-format off */

/** @cond postor_none */
#define po_true  1
#define po_false 0

#define po_lbmc            0xFFFFFFFFFFFFFFFEL // Local Bitmask Clear
#define po_lbms            0x0000000000000001L // Local Bitmask Set

#define po_unit_size       ( sizeof( po_d ) )
#define po_byte_size( po ) ( po_unit_size * pm_size( po) )
#define po_used_size( po ) ( po_unit_size * po->used )

#define po_snor(size)      (((size) & 0x1L) ? (size) + 1 : (size))
#define po_local( po )     ( (po)->size & 0x1L )

#define pm_any( po ) (     ( po )->used > 0 )
#define pm_empty( po )     ( ( po )->used == 0 )
#define pm_size( po )      ( ( po )->size & po_lbmc )
#define pm_end( po )       ( po )->data[ ( po )->used ]
#define pm_last( po )      ( po )->data[ ( po )->used - 1 ]
#define pm_first( po )     ( po )->data[ 0 ]
#define pm_nth( po, pos )  ( po )->data[ ( pos ) ]

#define pm_unit2byte(n)    ((n)<<3)
#define pm_byte2unit(n)    ((n)>>3)

/** @endcond postor_none */

/* clang-format on */


static po_t po_allocate_descriptor_if( po_t po );
static void po_set_size( po_t po, po_size_t size );
static void po_set_size_and_local( po_t po, po_size_t size, int local );
static void po_init( po_t po, po_size_t size, po_d data, int local );
static po_size_t po_align_size( po_size_t new_size );
static po_size_t po_incr_size( po_t po );
static po_size_t po_legal_size( po_size_t size );
static po_size_t po_norm_idx( po_t po, po_pos_t idx );
static void po_resize_to( po_t po, po_size_t new_size );
void po_void_assert( void );



/* ------------------------------------------------------------
 * Create and destroy:
 */


po_t po_new( po_t po )
{
    return po_new_sized( po, PO_DEFAULT_SIZE );
}


po_t po_new_sized( po_t po, po_size_t size )
{
    po = po_allocate_descriptor_if( po );
    if ( po == NULL ) {
        return po;
    }

    size = po_legal_size( size );
    po_init( po, size, po_malloc( pm_unit2byte( size ) ), 0 );

    return po;
}


po_t po_new_pages( po_t po, po_size_t count )
{
    po_size_t bytes;

    if ( count == 0 ) {
        count = 1;
    }

    po = po_allocate_descriptor_if( po );
    if ( po == NULL ) {
        return po;
    }

    bytes = po_alloc_pages( count, &po->data );
    po_init( po, pm_byte2unit( bytes ), po->data, 0 );

    return po;
}


po_t po_new_descriptor( po_t po )
{
    po = po_allocate_descriptor_if( po );
    if ( po == NULL ) {
        return po;
    }
    po->size = 0;
    po->used = 0;
    po->data = NULL;

    return po;
}


po_t po_use( po_t po, po_d* mem, po_size_t size )
{
    /* Check that size is multiple of 2*po_d, i.e. even. */
    po_assert( ( size % 2 ) == 0 );
    /* Check size is over minimum. */
    po_assert( size >= PO_MIN_SIZE );

    po_init( po, size, mem, 1 );
    memset( po->data, 0, pm_unit2byte( size ) );

    return po;
}


po_t po_destroy( po_t po )
{
    if ( po ) {
        po_destroy_storage( po );
        po_free( po );
    }
    
    return NULL;
}


void po_destroy_storage( po_t po )
{
    if ( po == NULL ) {
        return;
    }

    if ( po->data && !po_local( po ) ) {
        po_free( po->data );
    }
    
    po->data = NULL;
    po_set_size( po, 0 );
}


void po_resize( po_t po, po_size_t new_size )
{
    new_size = po_legal_size( new_size );

    if ( new_size >= po->used ) {
        po_resize_to( po, new_size );
    }
}


void po_push( po_t po, po_d item )
{
    po_size_t new_used = po->used + 1;

    if ( new_used > pm_size( po ) ) {
        po_resize_to( po, po_incr_size( po ) );
    }

    pm_nth( po, po->used ) = item;
    po->used = new_used;
}


po_d po_pop( po_t po )
{
    if ( pm_any( po ) ) {
        po_d ret = pm_last( po );
        po->used--;
        if ( pm_empty( po ) ) {
            pm_first( po ) = NULL;
        }
        return ret;
    } else {
        return NULL;
    }
}


po_size_t po_drop( po_t po, po_size_t count )
{
    if ( po->used >= count ) {
        po->used -= count;
    } else {
        count = po->used;
        po_reset( po );
    }

    return count;
}


void po_add( po_t po, po_d item )
{
    if ( po->data == NULL ) {
        po_new( po );
    }
    po_push( po, item );
}


po_d po_remove( po_t po )
{
    po_d ret;

    if ( po->data ) {
        ret = po_pop( po );
        if ( pm_empty( po ) ) {
            po_destroy_storage( po );
        }
        return ret;
    } else {
        return NULL;
    }
}


void po_reset( po_t po )
{
    po->used = 0;
}


void po_clear( po_t po )
{
    po->used = 0;
    memset( po->data, 0, po_byte_size( po ) );
}


po_s po_duplicate( po_t po )
{
    po_s dup;

    po_new_sized( &dup, pm_size( po ) );
    dup.used = po->used;
    memcpy( dup.data, po->data, po_used_size( po ) );

    return dup;
}


po_d po_swap( po_t po, po_pos_t pos, po_d item )
{
    if ( po->data == NULL ) {
        return NULL;
    }

    if ( pm_empty( po ) ) {
        return NULL;
    }

    po_size_t norm;
    po_d      ret;

    norm = po_norm_idx( po, pos );
    ret = pm_nth( po, norm );
    pm_nth( po, norm ) = item;

    return ret;
}


void po_insert_at( po_t po, po_pos_t pos, po_d item )
{
    po_size_t new_used = po->used + 1;

    if ( new_used > pm_size( po ) ) {
        po_resize_to( po, po_incr_size( po ) );
    }
    po_insert_if( po, pos, item );
}


int po_insert_if( po_t po, po_pos_t pos, po_d item )
{
    po_size_t new_used = po->used + 1;

    if ( new_used > pm_size( po ) ) {
        return po_false;
    }

    po_size_t norm;
    if ( pos == (po_pos_t)( po->used ) ) {
        /* Insert at end. */
        norm = pos;
    } else {
        /* Insert somewhere else. */
        norm = po_norm_idx( po, pos );
    }

    if ( norm < po->used ) {
        /* Shift container tail right to make space for the insert. */
        memmove( &( pm_nth( po, norm + 1 ) ),
                 &( pm_nth( po, norm ) ),
                 ( po->used - norm ) * po_unit_size );
    } else if ( norm > po->used ) {
        po_assert( 0 ); // GCOV_EXCL_LINE
    }

    pm_nth( po, norm ) = item;
    po->used = new_used;

    return po_true;
}


po_d po_delete_at( po_t po, po_pos_t pos )
{
    if ( pm_empty( po ) ) {
        return NULL;
    }

    po_d      ret;
    po_size_t new_used = po->used - 1;

    if ( po->used == 1 ) {
        ret = po_first( po );
        po->used = 0;
        pm_first( po ) = NULL;
        return NULL;
    }

    po_size_t norm = po_norm_idx( po, pos );

    ret = pm_nth( po, norm );
    memmove( &( pm_nth( po, norm ) ),
             &( pm_nth( po, norm + 1 ) ),
             ( po->used - ( norm + 1 ) ) * po_unit_size );

    po->used = new_used;

    return ret;
}


void po_sort( po_t po, po_compare_fn_p compare )
{
    qsort( po->data, po->used, po_unit_size, (int ( * )( const void*, const void* ))compare );
}


po_d po_alloc_bytes( po_t po, po_size_t bytes )
{
    po_d      ret;
    po_size_t units;

    ret = NULL;
    units = ( bytes >> 3 ) + ( ( bytes & 0x07ULL ) != 0 );

    if ( po->size >= ( po->used + units ) ) {
        ret = &po->data[ po->used ];
        po->used += units;
    }

    return ret;
}


/* ------------------------------------------------------------
 * Queries:
 */

po_size_t po_used( po_t po )
{
    return po->used;
}


po_size_t po_size( po_t po )
{
    return pm_size( po );
}


po_size_t po_bytesize( po_t po )
{
    return po_unit_size * pm_size( po );
}


po_d* po_data( po_t po )
{
    return po->data;
}

po_d po_first( po_t po )
{
    return pm_first( po );
}

po_d po_last( po_t po )
{
    if ( pm_any( po ) ) {
        return pm_last( po );
    } else {
        return NULL;
    }
}

po_d po_nth( po_t po, po_pos_t pos )
{
    if ( pm_any( po ) ) {
        po_size_t idx;
        idx = po_norm_idx( po, pos );
        return po->data[ idx ];
    } else {
        return NULL;
    }
}

po_d* po_nth_ref( po_t po, po_pos_t pos )
{
    if ( pm_any( po ) ) {
        po_size_t idx;
        idx = po_norm_idx( po, pos );
        return &po->data[ idx ];
    } else {
        return NULL;
    }
}


int po_is_empty( po_t po )
{
    if ( po->data == NULL ) {
        return po_false;
    }

    if ( po->used <= 0 ) {
        return po_true;
    } else {
        return po_false;
    }
}


int po_is_full( po_t po )
{
    if ( po->data == NULL ) {
        return po_false;
    }

    if ( po->used >= pm_size( po ) ) {
        return po_true;
    } else {
        return po_false;
    }
}


po_pos_t po_find( po_t po, po_d item )
{
    for ( po_size_t i = 0; i < po->used; i++ ) {
        if ( pm_nth( po, i ) == item ) {
            return i;
        }
    }

    return PO_NOT_INDEX;
}


po_pos_t po_find_with( po_t po, po_compare_fn_p compare, po_d ref )
{
    for ( po_size_t i = 0; i < po->used; i++ ) {
        if ( compare( pm_nth( po, i ), ref ) ) {
            return i;
        }
    }

    return PO_NOT_INDEX;
}


void po_set_local( po_t po, int val )
{
    if ( val != 0 ) {
        po->size = po->size | po_lbms;
    } else {
        po->size = po->size & po_lbmc;
    }
}


int po_get_local( po_t po )
{
    return po_local( po );
}



/* ------------------------------------------------------------
 * Utilities:
 */


po_size_t po_alloc_pages( po_size_t count, po_d** mem )
{
    if ( count == 0 ) {
        return sysconf( _SC_PAGESIZE );
    }

    po_size_t page_size;
    page_size = sysconf( _SC_PAGESIZE );

    if ( !posix_memalign( (void**)mem, page_size, count * page_size ) ) {
        memset( *mem, 0, count * page_size );
        return count * page_size;
    } else {
        po_assert( 0 ); // GCOV_EXCL_LINE
    }

    return 0;
}



/* ------------------------------------------------------------
 * Internal support:
 */


/** 
 * Allocate Postor descriptor if po is non-NULL.
 * 
 * Return NULL on allocation failure.
 *
 * @param po Postor or NULL.
 * 
 * @return Postor or NULL.
 */
static po_t po_allocate_descriptor_if( po_t po )
{
    if ( po == NULL ) {
        po = po_malloc( sizeof( po_s ) );
    }
    return po;
}


/** 
 * Set size for Postor, without touching the "local" info.
 * 
 * @param po    Postor.
 * @param size  Size.
 */
static void po_set_size( po_t po, po_size_t size )
{
    int local;

    local = po_get_local( po );
    po->size = size;
    po_set_local( po, local );
}


/** 
 * Set size and local for Postor.
 * 
 * @param po     Postor.
 * @param size   Size.
 * @param local  Local info.
 */
static void po_set_size_and_local( po_t po, po_size_t size, int local )
{
    po->size = size;
    po_set_local( po, local );
}


/**
 * Initialize Postor struct to given size and local mode.
 *
 * @param po    Postor.
 * @param size  Size (must be already legal).
 * @param local Local mode.
 */
static void po_init( po_t po, po_size_t size, po_d data, int local )
{
    po_set_size_and_local( po, size, local );
    po->used = 0;
    po->data = data;
}


/**
 * Align reservation size for 4k and bigger.
 *
 * Small reservations are not effected.
 *
 * @param new_size    Size to align, if needed.
 *
 * @return            Valid size.
 */
static po_size_t po_align_size( po_size_t new_size )
{
    if ( new_size >= 4096 ) {
        if ( new_size == 4096 ) {
            new_size = 4096;
        } else {
            new_size = ( ( ( new_size >> 12 ) + 1 ) << 12 );
        }
    }

    return new_size;
}


/**
 * Calculate incremented memory reservation size.
 *
 * Reservation size is doubled from the existing value.
 *
 * @param po Postor.
 *
 * @return New size.
 */
static po_size_t po_incr_size( po_t po )
{
    return po_align_size( pm_size( po ) * 2 );
}


/**
 * Convert size to a legal Postor size.
 *
 * @param size Requested size.
 *
 * @return Legal size.
 */
static po_size_t po_legal_size( po_size_t size )
{
    /* Make size even. */
    size = po_snor( size );

    /* Make size big enough. */
    if ( size < PO_MIN_SIZE ) {
        size = PO_MIN_SIZE;
    }

    /* Return proper aligment if size if bigger than page size. */
    return po_align_size( size );
}


/**
 * Normalize (possibly negative) Postor index. Positive index is
 * saturated to Postor length, and negative index is normalized.
 *
 * -1 means last char in Postor, -2 second to last, etc. Index after
 * last char can only be expressed by positive indeces. E.g. for
 * Postor with length of 4 the indeces are:
 *
 * Chars:     a  b  c  d  \0
 * Positive:  0  1  2  3  4
 * Negative: -4 -3 -2 -1
 *
 * @param po  Postor.
 * @param idx Index to Postor.
 *
 * @return Unsigned (positive) index to Postor.
 */
static po_size_t po_norm_idx( po_t po, po_pos_t idx )
{
    po_size_t pidx;

    if ( idx < 0 ) {
        pidx = po->used + idx;
        idx  = po->used + idx;
    } else {
        pidx = idx;
    }

    if ( pidx >= po->used ) {
        po_assert( 0 ); // GCOV_EXCL_LINE
        return po->used - 1; // GCOV_EXCL_LINE
    } else if ( idx < 0 ) {
        po_assert( 0 ); // GCOV_EXCL_LINE
        return 0;
    } else {
        return pidx;
    }
}


/**
 * Resize Postor to requested size.
 *
 * @param po       Postor reference.
 * @param new_size Requested size (legalized).
 */
static void po_resize_to( po_t po, po_size_t new_size )
{
    if ( po_local( po ) ) {

        po->data = po_malloc( pm_unit2byte( new_size ) );

    } else {

        po_size_t old_size = pm_size( po );
        po->data = po_realloc( po->data, pm_unit2byte( new_size ) );

        if ( new_size > old_size ) {
            /* Clear newly allocated memory. */
            memset( &( po->data[ old_size ] ),
                    0,
                    ( new_size - old_size ) * sizeof( po_d ) );
        }
    }

    po_set_size_and_local( po, new_size, 0 );
}


/**
 * Disabled (void) assertion.
 */
void po_void_assert( void ) // GCOV_EXCL_LINE
{
}
