# Postor - Automatically growing Pointer Storage

Postor is linear container for storing pointers (objects). The storage
is a continous array.  Pointer storage grows automatically when the
usage count exceeds reservation size. Reservation is doubled at
resizing.

Postor struct, i.e. Postor Descriptor (`po_s`):

    Field     Type          Addr
    -----------------------------
    size      (uint64_t)  | N + 0
    used      (uint64_t)  | N + 8
    data      (void**)    | N + 16

`size` gives the number of pointers that fits currently. `used`
defines the amount of pointer items stored, so far. The size of `data`
field (pointer array), matches `size`.

Base type for Postor is `po_t`, i.e. the Postor. `po_t` is a pointer
to Postor struct (descriptor). None of the functions in Postor library
require a reference to Postor itself, since `po_t` members are only
updated.

Postor might point to a stack or a heap allocated Postor descriptor,
depending on the use case.

Postor can be created with default size using `po_new()`. Default size
is 16. If user knows about usage pattern for Postor, a specific size
can be requested using `po_new_sized()` function.

    po_s ps;
    po_t po;
    po = po_new_sized( &ps, 128 );

If `po_new_sized()` is called with non-NULL value, the given descriptor
is initialized to requested size, and otherwise the descriptor is heap
allocated. `po` can be setup in the same function call. This would be
quite common pattern in Postor use.

Postor storage can be destroyed with:

    po_destroy_storage( po );

`po->data` (and `po->size`) is set to `NULL` after destroy. Heap
allocated descriptors (and the container) is destroyed with
`po_destroy()`.

Items can be added to the end of the container:

    void* data = obj;
    po_push( po, data );

Items can be removed from the end of the container:

    data = po_pop( po );

The most ergonomic way of creating a Postor, is to just start adding
items to it. However the Postor handle must be initialized to `NULL`
to indicate that the Postor does not exist yet.

    po_s ps;
    po_new_descriptor( &ps );
    po_add( &ps, data1 );
    po_add( &ps, data2 );
    ...

Opposite operation for `po_add()` is `po_remove()`. `po_remove()`
removes items from Postor, and if Postor becomes empty, Postor is
container is destroyed, and Postor data is set to `NULL`.

    data = po_remove( po );

Item can be inserted to any selected position:

    po_insert_at( po, 0, data );

This would add data to the start of container and the existing data
would be shifted towards the end by one slot. If usage count exceeds
reservation size, more is reserved and `po` is updated. Data can be
inserted also without automatic resizing:

    po_insert_if( po, 10, data );

Items can be deleted from selected positions:

    data = po_delete_at( po, 0 );

This would delete the first item from container.

Postor supports a number of different queries. User can query
container usage, size, empty, and full status information. User can
also get data from selected position:

    data = po_nth( po, 10 );

Postor has two find functions. `po_find()` finds the data as pointer.

    data_idx = po_find( po, data );

If data (pointer) exists in Postor, `data_idx` contains the container
index (position) to the searched data. Otherwise `data_idx` is
assigned an invalid index (`PO_NOT_INDEX`).

Postor can also be searched for objects. Search function is provided a
function pointer to compare function that is able to detect whether
the searched item is at current position or not.

    data_idx = po_find_with( po, compare_fn, data );

Postor can also be used within stack allocated memory. First you have
to have some stack storage available. This can be done with a
convenience macro.

    po_t po;
    po_local_use( ps, buf, 16 );
    po = &ps;

This will initialize `ps` as stack allocated descriptor where Postor
has size of 16. Variable `buf` will be the container (with size of 16
items). `po_local_use` uses `po_use` function for assigning `ps` to
the storage. `po` is assigned here to address of the descriptor just
as an example.

When Postor is taken into use through `po_use` it is marked as
"local". This means that container will not be freed with
`po_destroy`. Stack allocated Postor is automatically changed to a
heap allocated Postor if Postor requires resizing. This is quite
powerful optimization, since often stack allocated memory is enough
and heap reservation (which is slowish) is not needed. `po_destroy`
can be called for Postor whether its "local" or not. If Postor is
"local", no memory is released, but Postor is still marked empty.

Postor supports non-local memory management. User can allocate a
number of pages of memory.

    po_new_page( po, 1 );

Junks of memory can be given to various purposes.

    mem = po_alloc_bytes( po, 1024 );

Memory is cleared, page aligned and continuous. It provides good cache
locality. `po_alloc_bytes` returns NULL when Postor Page(s) are
consumed. There is no automatic resizing since it is assumed that
client wants to preserve the address returned by
`po_alloc_bytes`. Memory is released either with `po_destroy` when it
is not needed anymore at all, or with `po_clear` when it is still
needed for some other use.


By default Postor library uses malloc and friends to do heap
allocations. If you define POSTOR_MEM_API, you can use your own memory
allocation functions.

Custom memory function prototypes:
    void* po_malloc ( size_t size );
    void  po_free   ( void*  ptr  );
    void* po_realloc( void*  ptr, size_t size );


See Doxygen docs and `postor.h` for details about Postor API. Also
consult the test directory for usage examples.


## Postor API documentation

See Doxygen documentation. Documentation can be created with:

    shell> doxygen .doxygen


## Examples

All functions and their use is visible in tests. Please refer `test`
directory for testcases.


## Building

Ceedling based flow is in use:

    shell> ceedling

Testing:

    shell> ceedling test:all

User defines can be placed into `project.yml`. Please refer to
Ceedling documentation for details.


## Ceedling

Postor uses Ceedling for building and testing. Standard Ceedling files
are not in GIT. These can be added by executing:

    shell> ceedling new postor

in the directory above Postor. Ceedling prompts for file
overwrites. You should answer NO in order to use the customized files.
