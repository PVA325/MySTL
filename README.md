# MySTL

## Implemented Components

### 1. `List<T, Alloc>`

A bidirectional linked list similar to `std::list`.

**Key properties:**
- Bidirectional iterators (`iterator`, `const_iterator`, `reverse_iterator`)
- O(1) insertion and deletion at any position
- Full allocator support (allocator-aware container)
- `[[no_unique_address]]` is used so stateless allocators do not increase object size
- Strong exception safety guarantee for all modifying operations

**Supported operations:**
- Constructors:
  - default
  - size-based
  - allocator-aware
  - copy / move
- `push_back`, `push_front`
- `pop_back`, `pop_front`
- `insert`, `erase`
- `size()`
- `begin / end / cbegin / cend / rbegin / rend`
- `get_allocator()`

The list is internally implemented using a sentinel (fake) node, ensuring that
edge cases (empty list, insertion at begin/end) are handled uniformly.

---

### 2. `SharedPtr<T>`, `WeakPtr<T>`, `EnableSharedFromThis<T>`

A simplified but robust implementation of `std::shared_ptr` and related utilities.

#### `SharedPtr<T>`

- Reference-counted smart pointer with shared ownership
- Supports:
  - construction from raw pointers
  - construction from derived types
  - copy and move semantics
  - aliasing constructor
  - custom deleters
  - allocator-aware control blocks
- `use_count()` to inspect shared ownership
- `reset()` and `get()`

`makeShared` is implemented to:
- allocate the control block and object in **a single allocation**
- correctly forward constructor arguments
- provide strong exception safety

#### `WeakPtr<T>`

- Non-owning smart pointer observing a `SharedPtr`
- Does not extend object lifetime
- Supports:
  - copy / move
  - construction from `SharedPtr<U>` where `U` derives from `T`
- Methods:
  - `expired()`
  - `lock()` (returns `SharedPtr<T>` if object is still alive)

#### `EnableSharedFromThis<T>`

- Allows an object to safely create a `SharedPtr` to itself
- Correctly integrates with the internal control block
- Prevents undefined behavior when used on stack-allocated objects

---

### 3. `UnorderedMap<Key, Value, Hash, Equal, Alloc>`

A hash table container similar to `std::unordered_map`, implemented on top of `List`.

**Design highlights:**
- Separate chaining using a linked list
- All elements stored in a single list to preserve iterator validity
- Bucket table stores:
  - iterator to the first element of the bucket
  - number of elements in the bucket
- Rehashing redistributes nodes without reallocating them
- Full allocator support

**Supported operations:**
- `insert`, `emplace`
- `erase`
- `find`
- `operator[]`
- `at`
- `reserve`
- `rehash`
- `load_factor`, `max_load_factor`
- iterators (`iterator`, `const_iterator`)

All operations provide at least basic exception safety, and critical modifying
operations provide strong exception safety.

---

### 4. `Tuple`

A compile-time tuple implementation similar to `std::tuple`.

**Features:**
- Variadic template-based storage
- Zero runtime overhead
- `get<N>(tuple)` access
- Correct handling of references and const-qualified types
- Empty base optimization where applicable

---

All containers are allocator-aware and follow the C++ Allocator requirements:
All components aim to follow STL exception safety guarantees:

## Performance Considerations

- `makeShared` uses a single allocation for control block + object
- `UnorderedMap` rehashing avoids node reallocation
- `List` operations are strictly O(1) where required
