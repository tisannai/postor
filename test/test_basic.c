#include "unity.h"
#include "postor.h"
#include <string.h>
#include <unistd.h>

void gdb_breakpoint( void ) {}

void test_basics( void )
{
    po_s  ps;
    po_t  po;
    po_s  dup;
    char* text = "text";
    po_d  ret;

    po = po_new( &ps );
    TEST_ASSERT_EQUAL( PO_DEFAULT_SIZE, po_size( po ) );
    TEST_ASSERT_EQUAL( 1, po_is_empty( po ) );
    TEST_ASSERT_EQUAL( 0, po_is_full( po ) );

    po_destroy_storage( po );
    TEST_ASSERT_EQUAL( NULL, po->data );
    TEST_ASSERT_EQUAL( 0, po->size );

    po_new_sized( &ps, 12 );
    TEST_ASSERT_EQUAL( 12, po_size( po ) );
    TEST_ASSERT_EQUAL( 0, po_used( po ) );

    po_push( po, text );
    TEST_ASSERT_EQUAL( 12, po_size( po ) );
    TEST_ASSERT_EQUAL( 1, po_used( po ) );

    ret = po_pop( po );
    TEST_ASSERT_EQUAL( text, ret );
    TEST_ASSERT_EQUAL( 0, po_used( po ) );

    /* Push 13 items, so that size is doubled, and check that. */
    for ( int i = 0; i < 13; i++ ) {
        po_push( po, text );
    }

    TEST_ASSERT_EQUAL( 24, po_size( po ) );
    TEST_ASSERT_EQUAL( 13, po_used( po ) );
    /* Push 11 more, and check that size is same as used. */
    for ( int i = 0; i < 11; i++ ) {
        po_push( po, text );
    }
    TEST_ASSERT( po_size( po ) == po_used( po ) );
    TEST_ASSERT_EQUAL( 1, po_is_full( po ) );
    TEST_ASSERT_EQUAL( 0, po_is_empty( po ) );

    po_reset( po );
    TEST_ASSERT_EQUAL( 0, po_used( po ) );
    po_resize( po, PO_DEFAULT_SIZE );
    TEST_ASSERT_EQUAL( PO_DEFAULT_SIZE, po_size( po ) );

    po_add( po, text );
    TEST_ASSERT_EQUAL( PO_DEFAULT_SIZE, po_size( po ) );
    TEST_ASSERT_EQUAL( 1, po_used( po ) );
    TEST_ASSERT_EQUAL( text, po_last( po ) );
    TEST_ASSERT_EQUAL( text, po_nth( po, 0 ) );

    ret = po_remove( po );
    TEST_ASSERT_EQUAL( text, ret );
    TEST_ASSERT_EQUAL( NULL, po->data );
    TEST_ASSERT_EQUAL( 0, po->size );

    po_add( po, text );
    po_add( po, NULL );
    po_add( po, text );
    dup = po_duplicate( po );
    for ( po_size_t i = 0; i < po_used( po ); i++ ) {
        TEST_ASSERT( po_nth( &dup, i ) == po_nth( po, i ) );
    }

    po_destroy_storage( po );
    po_destroy_storage( &dup );
    po_destroy_storage( po );
}


int po_compare_fn( const po_d a, const po_d b )
{
    if ( a == b )
        return 1;
    else
        return 0;
}


void test_random_access( void )
{
    po_s     ps;
    po_t     po;
    char*    text = "text";
    po_d     tmp;
    po_pos_t pos;

    po = po_new( &ps );
    po_insert_at( po, 0, text );
    po_insert_at( po, 0, NULL );
    TEST_ASSERT_EQUAL( NULL, po_data( po )[ 0 ] );
    TEST_ASSERT_EQUAL( text, po_last( po ) );

    tmp = po_swap( po, 0, text );
    po_swap( po, 1, tmp );
    TEST_ASSERT_EQUAL( text, po_first( po ) );
    TEST_ASSERT_EQUAL( NULL, po_last( po ) );

    pos = po_find( po, NULL );
    TEST_ASSERT_EQUAL( 1, pos );

    pos = po_find( po, text );
    TEST_ASSERT_EQUAL( 0, pos );

    pos = po_find_with( po, po_compare_fn, NULL );
    TEST_ASSERT_EQUAL( 1, pos );

    pos = po_find_with( po, po_compare_fn, text );
    TEST_ASSERT_EQUAL( 0, pos );


    for ( int i = 0; i < PO_DEFAULT_SIZE; i++ ) {
        po_insert_at( po, -1, text );
    }

    TEST_ASSERT_EQUAL( 2 * PO_DEFAULT_SIZE, po_size( po ) );

    for ( int i = 0; i < PO_DEFAULT_SIZE; i++ ) {
        po_insert_if( po, po_used( po ) - 1, text );
    }

    TEST_ASSERT_EQUAL( 2 * PO_DEFAULT_SIZE, po_size( po ) );

    for ( int i = 0; i < PO_DEFAULT_SIZE / 2; i++ ) {
        tmp = po_delete_at( po, 0 );
        TEST_ASSERT_EQUAL( text, tmp );
    }

    for ( int i = 0; i < PO_DEFAULT_SIZE / 2; i++ ) {
        po_delete_at( po, po_used( po ) - 1 );
    }

    TEST_ASSERT_EQUAL( text, po_first( po ) );
    TEST_ASSERT_EQUAL( text, po_last( po ) );

    TEST_ASSERT_EQUAL_STRING( text, po_item( po, 1, char* ) );

    char*     item;
    po_size_t idx = 0;
    po_each( po, item, char* )
    {
        TEST_ASSERT_EQUAL( idx, po_idx );
        TEST_ASSERT_EQUAL_STRING( text, item );
        idx++;
    }

    for ( int i = 0; i < 2 * PO_DEFAULT_SIZE; i++ ) {
        po_pop( po );
    }

    TEST_ASSERT_EQUAL( NULL, po_first( po ) );
    TEST_ASSERT_EQUAL( NULL, po_last( po ) );
    TEST_ASSERT_EQUAL( NULL, po_nth( po, 0 ) );

    po_swap( po, 0, NULL );
    po_insert_if( po, 0, text );
    po_delete_at( po, 0 );
    po_delete_at( po, 0 );

    TEST_ASSERT_EQUAL( PO_NOT_INDEX, po_find( po, text ) );
    TEST_ASSERT_EQUAL( PO_NOT_INDEX, po_find_with( po, po_compare_fn, text ) );

    po_destroy_storage( po );
    po_remove( po );
    po_swap( po, 0, NULL );

    TEST_ASSERT_EQUAL( 0, po_is_empty( po ) );
    TEST_ASSERT_EQUAL( 0, po_is_full( po ) );

    po_new_sized( po, 0 );
    TEST_ASSERT_EQUAL( PO_MIN_SIZE, po_size( po ) );
    po_destroy_storage( po );
}


int po_sort_compare( const po_d a, const po_d b )
{
    char* sa = *((char**)a);
    char* sb = *((char**)b);

    return strcmp( sa, sb );
}


void test_sorting( void )
{
    po_t  po;
    char* str1 = "aaa";
    char* str2 = "bbb";
    char* str3 = "ccc";

    po = po_new( NULL );
    po_push( po, str3 );
    po_push( po, str1 );
    po_push( po, str2 );

    TEST_ASSERT_EQUAL_STRING( po_item( po, 0, char* ), str3 );
    TEST_ASSERT_EQUAL_STRING( po_item( po, 1, char* ), str1 );
    TEST_ASSERT_EQUAL_STRING( po_item( po, 2, char* ), str2 );

    po_sort( po, po_sort_compare );

    TEST_ASSERT_EQUAL_STRING( po_item( po, 0, char* ), str1 );
    TEST_ASSERT_EQUAL_STRING( po_item( po, 1, char* ), str2 );
    TEST_ASSERT_EQUAL_STRING( po_item( po, 2, char* ), str3 );

    po_destroy( po );
}


void test_static( void )
{
    po_t  po;
    char* str1 = "aaa";
    char* str2 = "bbb";


    po_local_use( ps, buf, 8 );
    po = &ps;

    TEST_ASSERT_TRUE( po_get_local( po ) );

    po_push( po, str1 );
    po_push( po, str2 );
    po_push( po, str1 );
    po_push( po, str2 );
    po_push( po, str1 );
    po_push( po, str2 );
    po_push( po, str1 );
    po_push( po, str2 );

    TEST_ASSERT_EQUAL_STRING( po_item( po, 0, char* ), str1 );
    TEST_ASSERT_EQUAL_STRING( po_item( po, 1, char* ), str2 );
    TEST_ASSERT_EQUAL_STRING( po_item( po, 2, char* ), str1 );

    TEST_ASSERT_TRUE( po_get_local( po ) );

    po_push( po, str1 );
    po_push( po, str2 );

    TEST_ASSERT_FALSE( po_get_local( po ) );

    po_destroy_storage( po );

    po_use( po, buf, 8 );
    po_destroy_storage( po );
}



void test_alloc( void )
{
    po_s      ps;
    po_t      po;
    po_d      pd;
    po_size_t page_size;


    po = po_new_pages( &ps, 2 );

    page_size = po_alloc_pages( 0, NULL );
    TEST_ASSERT_TRUE( ( po_bytesize( po ) ) == ( 2 * page_size ) );

    pd = po_alloc_bytes( po, 1 );
    TEST_ASSERT_TRUE( pd != NULL );
    TEST_ASSERT_TRUE( po->used == 1 );

    pd = po_alloc_bytes( po, 2 );
    TEST_ASSERT_TRUE( pd != NULL );
    TEST_ASSERT_TRUE( po->used == 2 );

    pd = po_alloc_bytes( po, 8 );
    TEST_ASSERT_TRUE( pd != NULL );
    TEST_ASSERT_TRUE( po->used == 3 );

    pd = po_alloc_bytes( po, 9 );
    TEST_ASSERT_TRUE( pd != NULL );
    TEST_ASSERT_TRUE( po->used == 5 );

    pd = po_alloc_bytes( po, ( po_size( po ) - po->used ) * sizeof( po_d ) );
    TEST_ASSERT_TRUE( pd != NULL );

    pd = po_alloc_bytes( po, 1 );
    TEST_ASSERT_TRUE( pd == NULL );

    po_destroy_storage( po );

    po_new_pages( po, 0 );
    TEST_ASSERT_TRUE( po_bytesize( po ) == page_size );
    po_destroy_storage( po );
}
