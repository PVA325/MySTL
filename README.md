# MySTL

**MySTL** is a collection of STL-like containers and utilities implemented in C++.

## Implemented Components

### `List<T, Alloc>`
A doubly linked list similar to `std::list`.

### `SharedPtr<T>`, `WeakPtr<T>`, `EnableSharedFromThis<T>`
Smart pointers providing shared ownership, weak references, and `shared_from_this` support.

### `UnorderedMap<Key, Value, Hash, Equal, Alloc>`
A hash table container similar to `std::unordered_map`.

### `Tuple<Ts...>`
A compile-time tuple with indexed access.

### `Function<R(Args...)>`
A type-erased callable wrapper similar to `std::function`.

### `Variant<Ts...>`
A type-safe union for holding one of several types.

## Features

- STL-like interface
- Allocator support
- Iterators compatible with standard patterns
- Focus on correctness and exception safety
