#include <stdexcept>
#include <exception>
#include <memory>
#include <iostream>

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
class UnorderedMap;

template<typename T>
struct BaseNode {
  BaseNode* prev;
  BaseNode* next;
  BaseNode(): prev(this), next(this) {}
  BaseNode(BaseNode* prev, BaseNode* next): prev(prev), next(next) {}

  ~BaseNode() = default;
};

template<typename T>
struct DefaultNode : BaseNode<T> {
  T val;
  DefaultNode(): BaseNode<T>() {}
  DefaultNode(BaseNode<T>* prev, BaseNode<T>* next, const T& val) noexcept(false) : BaseNode<T>(prev, next), val(val) {}
  DefaultNode(BaseNode<T>* prev, BaseNode<T>* next, T&& val) noexcept(false) : BaseNode<T>(prev, next), val(std::move(val)) {}

  template<typename... Args>
  DefaultNode(BaseNode<T>* prev, BaseNode<T>* next, Args&&... args) noexcept(false) : BaseNode<T>(prev, next), val(std::forward<Args>(args)...) {}

  DefaultNode(const DefaultNode& other);
  DefaultNode(DefaultNode&& other) noexcept ;

  DefaultNode& operator=(DefaultNode other);

  ~DefaultNode() = default;
};


template<typename T, typename AllocT = std::allocator<T>>
class List {
private:
  using BaseNodeType = BaseNode<T>;
  using DefaultNodeType = DefaultNode<T>;
  BaseNodeType fake_node_;
  [[no_unique_address]] AllocT alloc_;

  using allocT = AllocT;

  template<bool is_const>
  class Iterator {
  public:
    using value_type = typename std::conditional<is_const, const T, T>::type;
    using base_node_val = typename std::conditional<is_const, const BaseNodeType*, BaseNodeType*>::type;
    using def_node_val = typename std::conditional<is_const, const DefaultNodeType*, DefaultNodeType*>::type;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using pointer           = value_type*;
    using reference         = value_type&;

    Iterator() { ptr_ = nullptr; }
    Iterator(const Iterator& other);
    Iterator(base_node_val ptr): ptr_(ptr) {}
    // here iterator store only 1 pointer -> move semantic dont need

    Iterator(const Iterator<false>& other) requires(is_const);

    Iterator& operator++();
    Iterator operator++(int);
    Iterator& operator--();
    Iterator operator--(int);

    template<bool other_const>
    bool operator==(const Iterator<other_const>& other) const { return ptr_ == other.ptr_; }
    template<bool other_const>
    bool operator!=(const Iterator<other_const>& other) const { return ptr_ != other.ptr_; }

    value_type& operator*() const;
    value_type* operator->() const;

    ~Iterator() = default;

    friend class List;
    base_node_val ptr() { return ptr_; }
  private:
    base_node_val ptr_;
  };

  using DefaultNodeAlloc = typename std::allocator_traits<AllocT>::template rebind_alloc<DefaultNodeType>;

  uint32_t size_;

  void DestroyHead(BaseNodeType* ptr);
  void init(uint32_t n, const AllocT& alloc, const T& val);

  template<typename AnotherAllocT = std::allocator<T>>
  void copy_from(const List<T, AnotherAllocT>& other);

  template<typename AnotherAllocT = std::allocator<T>>
  void move_from(List<T, AnotherAllocT>&& other);

  template<typename AnotherAllocT = std::allocator<T>>
  void assign_from(const List<T, AnotherAllocT>& other);

  template<typename AnotherAllocT = std::allocator<T>>
  void move_assign_from(List<T, AnotherAllocT>&& other);

  template<typename... Args>
  void emplace_back(Args&&... args);

  template<typename... Args>
  void emplace_front(Args&&... args);
public:
  using value_type = T;
  List();
  List(int32_t n);
  List(int32_t n, const T& val);
  List(const AllocT& alloc);
  List(uint32_t n, const AllocT& alloc);
  List(uint32_t n, const T& val, const AllocT& alloc);


  template<typename AnotherAllocT = std::allocator<T>>
  List(const List<T, AnotherAllocT>& other) { copy_from(other); }
  List(const List& other) { copy_from(other); }

  template<typename AnotherAllocT = std::allocator<T>>
  List(List<T, AnotherAllocT>&& other) { move_from(std::move(other)); }
  List(List&& other) { move_from(std::move(other)); }

  template<typename AnotherAllocT = std::allocator<T>>
  void swap(List<T, AnotherAllocT>& other) {
    static_assert(std::is_constructible_v<AllocT, AnotherAllocT>);
    static_assert(std::is_constructible_v<AnotherAllocT, AllocT>);

    std::swap(fake_node_.next, other.fake_node_.next);
    std::swap(fake_node_.prev, other.fake_node_.prev);
    fake_node_.next->prev = &fake_node_;
    fake_node_.prev->next = &fake_node_;
    other.fake_node_.next->prev = &other.fake_node_;
    other.fake_node_.prev->next = &other.fake_node_;

//    std::swap(fake_node_, other.fake_node_);
    std::swap(size_, other.size_);
    if constexpr (std::allocator_traits<AllocT>::propagate_on_container_swap::value) {
      std::swap(alloc_, other.alloc_);
    }
  }

  template<typename AnotherAllocT = std::allocator<T>>
  List& operator=(const List<T, AnotherAllocT>& other);
  List& operator=(const List& other);

  template<typename AnotherAllocT = std::allocator<T>>
  List& operator=(List<T, AnotherAllocT>&& other) noexcept;
  List& operator=(List&& other) noexcept;

  uint32_t size() const;
  bool empty() const;

  void push_back(const T& val);
  void push_back(T&& val);
  void push_front(const T& val);
  void push_front(T&& val);

  void pop_back();
  void pop_front();

  const T& back() const;
  const T& front() const;

  AllocT& get_allocator();
  const AllocT& get_allocator() const;

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

  reverse_iterator rbegin();
  const_reverse_iterator rbegin() const;
  reverse_iterator rend();
  const_reverse_iterator rend() const;
  const_reverse_iterator crbegin() const;
  const_reverse_iterator crend() const;

  void erase(iterator it);
  void insert(iterator it, const T& val);

  ~List();

  template<typename U, typename AllocU>
  friend class List;

  template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
  friend class UnorderedMap;
};

template<typename T, typename AllocT>
template<typename... Args>
void List<T, AllocT>::emplace_front(Args &&... args) {
  DefaultNodeAlloc node_alloc(alloc_);
  DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(node_alloc, 1);
  try {
    std::allocator_traits<DefaultNodeAlloc>::construct(node_alloc, new_node, &fake_node_, fake_node_.next, std::forward<Args>(args)...);
  } catch (...) {
    std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, new_node, 1);
    throw;
  }
  fake_node_.next->prev = static_cast<BaseNodeType*>(new_node);
  fake_node_.next = static_cast<BaseNodeType*>(new_node);
  ++size_;
}

template<typename T, typename AllocT>
template<typename... Args>
void List<T, AllocT>::emplace_back(Args&&... args) {
  DefaultNodeAlloc node_alloc(alloc_);
  DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(node_alloc, 1);
  try {
    std::allocator_traits<DefaultNodeAlloc>::construct(node_alloc, new_node, fake_node_.prev, &fake_node_, std::forward<Args>(args)...);
  } catch (...) {
    std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, new_node, 1);
    throw;
  }
  fake_node_.prev->next = static_cast<BaseNodeType*>(new_node);
  fake_node_.prev = static_cast<BaseNodeType*>(new_node);
  ++size_;
}

template<typename T, typename AllocT>
template<typename AnotherAllocT>
List<T, AllocT> &List<T, AllocT>::operator=(List<T, AnotherAllocT> &&other) noexcept {
  move_assign_from(std::move(other));
  return *this;
}

template<typename T, typename AllocT>
List<T, AllocT> &List<T, AllocT>::operator=(List<T, AllocT> &&other) noexcept {
  move_assign_from(std::move(other));
  return *this;
}

template<typename T, typename AllocT>
template<typename AnotherAllocT>
List<T, AllocT> &List<T, AllocT>::operator=(const List<T, AnotherAllocT> &other) {
  assign_from(other);
  return *this;
}

template<typename T, typename AllocT>
List<T, AllocT> &List<T, AllocT>::operator=(const List<T, AllocT> &other) {
  assign_from(other);
  return *this;
}


template<typename T, typename AllocT>
template<typename AnotherAllocT>
void List<T, AllocT>::move_assign_from(List<T, AnotherAllocT> &&other) {
  bool can_steal = false;
  if (std::is_constructible_v<AllocT, AnotherAllocT> && std::allocator_traits<AnotherAllocT>::propagate_on_container_move_assignment::value) {
    can_steal = true;
  }
  if (std::is_same_v<AllocT, AnotherAllocT> && alloc_ == other.alloc_) {
    can_steal = true;
  }
  if (can_steal) {
    DestroyHead(fake_node_.prev);
    alloc_= std::move(other.alloc_);

    fake_node_.next = std::move(other.fake_node_.next);
    fake_node_.next->prev = &fake_node_;
    fake_node_.prev = std::move(other.fake_node_.prev);
    fake_node_.prev->next = &fake_node_;

    other.fake_node_.next = &other.fake_node_;
    other.fake_node_.prev = &other.fake_node_;

    size_ = other.size_;
    other.size_ = 0;
    return;
  }

  DestroyHead(fake_node_.prev);

  DefaultNodeAlloc create_alloc(alloc_);
  typename List<T, AnotherAllocT>::BaseNodeType* cur_other_node = other.fake_node_.next;
  BaseNodeType* cur_node = &fake_node_;
  while (cur_other_node != &other.fake_node_) {
    DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(create_alloc, 1);
    try {
      std::allocator_traits<DefaultNodeAlloc>::construct(create_alloc, new_node, cur_node, &fake_node_, std::move((static_cast<DefaultNodeType*>(cur_other_node))->val));
    } catch (...) {
      std::allocator_traits<DefaultNodeAlloc>::deallocate(create_alloc, new_node, 1);
      DestroyHead(cur_node);
      size_ = 0;
      throw;
    }
    cur_node->next = static_cast<BaseNodeType*>(new_node);
    cur_node = cur_node->next;
    cur_other_node = cur_other_node->next;
  }
  size_ = other.size_;

  other.size_ = 0;
  other.fake_node_.prev = other.fake_node_.next = &other.fake_node_;
}


template<typename T, typename AllocT>
template<typename AnotherAllocT>
void List<T, AllocT>::move_from(List<T, AnotherAllocT> &&other) {
  fake_node_.next = other.fake_node_.next;
  fake_node_.next->prev = &fake_node_;

  fake_node_.prev = other.fake_node_.prev;
  fake_node_.prev->next = &fake_node_;

  size_ = other.size_;
  other.size_ = 0;
  other.fake_node_.prev = other.fake_node_.next = &other.fake_node_;
}

template<typename T, typename AllocT>
List<T, AllocT>::List(): size_(0) {
  fake_node_.prev = &fake_node_;
  fake_node_.next = &fake_node_;
}

template<typename T, typename AllocT>
void List<T, AllocT>::DestroyHead(List<T, AllocT>::BaseNodeType* ptr) {
  DefaultNodeAlloc node_alloc(alloc_);
  BaseNodeType* cur_destroy_node = ptr;
  BaseNodeType* next_destroy_node = ptr->prev;
  while (cur_destroy_node != &fake_node_) {
    std::allocator_traits<DefaultNodeAlloc>::destroy(node_alloc, static_cast<DefaultNodeType*>(cur_destroy_node));
    std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, static_cast<DefaultNodeType*>(cur_destroy_node), 1); // need dynamic cast?

    cur_destroy_node = next_destroy_node;
    next_destroy_node = next_destroy_node->prev;
  }
  fake_node_.prev = &fake_node_;
  fake_node_.next = &fake_node_;
}

template<typename T, typename AllocT>
void List<T, AllocT>::init(uint32_t n, const AllocT& alloc, const T& val) {
  BaseNodeType* cur = &fake_node_;
  DefaultNodeAlloc node_alloc(alloc);

  for (int32_t i = 0; i < n; ++i) {
    DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(node_alloc, 1);
    try {
      std::allocator_traits<DefaultNodeAlloc>::construct(node_alloc, new_node, cur, &fake_node_, val);
    } catch (...) {
      std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, new_node, 1);
      DestroyHead(cur);
      size_ = 0;
      throw;
    }

    cur->next = static_cast<BaseNodeType*>(new_node);
    cur = cur->next;
  }

  fake_node_.prev = cur;
  size_ = n;

  alloc_ = alloc;
  size_ = n;
}

template<typename T, typename AllocT>
List<T, AllocT>::List(int32_t n) {
  init(n, AllocT{}, T{});
}

template<typename T, typename AllocT>
List<T, AllocT>::List(int32_t n, const T& val) {
  init(n, AllocT{}, val);
}

template<typename T, typename AllocT>
List<T, AllocT>::List(const AllocT& alloc): alloc_(alloc), size_(0) {}

template<typename T, typename AllocT>
List<T, AllocT>::List(uint32_t n, const AllocT& alloc) {
  init(n, alloc, T{});
}

template<typename T, typename AllocT>
List<T, AllocT>::List(uint32_t n, const T& val, const AllocT& alloc) {
  init(n, alloc, val);
}

template<typename T, typename AllocT>
template<typename AnotherAllocT>
void List<T, AllocT>::assign_from(const List<T, AnotherAllocT>& other) {
  AllocT new_alloc = alloc_;
  if constexpr (std::is_constructible_v<AllocT, AnotherAllocT> && std::allocator_traits<AnotherAllocT>::propagate_on_container_copy_assignment::value) {
    new_alloc = other.alloc_;
  }

  DefaultNodeAlloc destroy_alloc(alloc_);
  while (&fake_node_ != fake_node_.next) {
    BaseNodeType* cur = fake_node_.next;
    fake_node_.next = cur->next;
    cur->next->prev = &fake_node_;

    std::allocator_traits<DefaultNodeAlloc>::destroy(destroy_alloc, static_cast<DefaultNodeType*>(cur));
    std::allocator_traits<DefaultNodeAlloc>::deallocate(destroy_alloc, static_cast<DefaultNodeType*>(cur), 1);
  }

  DefaultNodeAlloc create_alloc(new_alloc);
  typename List<T, AnotherAllocT>::BaseNodeType* cur_other_node = other.fake_node_.next;
  BaseNodeType* cur_node = &fake_node_;
  while (cur_other_node != &other.fake_node_) {
    DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(create_alloc, 1);
    try {
      std::allocator_traits<DefaultNodeAlloc>::construct(create_alloc, new_node, cur_node, &fake_node_, static_cast<const DefaultNode<T>*>(cur_other_node)->val);
    } catch (...) {
      std::allocator_traits<DefaultNodeAlloc>::deallocate(create_alloc, new_node, 1);
      DestroyHead(cur_node);
      size_ = 0;
      throw;
    }
    cur_node->next = static_cast<BaseNodeType*>(new_node);
    cur_node = cur_node->next;
    cur_other_node = cur_other_node->next;
  }
  alloc_ = new_alloc;
  size_ = other.size_;
}

template<typename T, typename AllocT>
template<typename AnotherAllocT>
void List<T, AllocT>::copy_from(const List<T, AnotherAllocT>& other) {
  if constexpr (std::is_constructible_v<AllocT, const AnotherAllocT&>) {
    AllocT tmp(other.get_allocator());
    alloc_ = std::allocator_traits<AllocT>::select_on_container_copy_construction(tmp);
  } else {
    alloc_ = std::allocator_traits<AllocT>::select_on_container_copy_construction(AllocT{});
  }

  const typename List<T, AnotherAllocT>::BaseNodeType* cur_other = other.fake_node_.next;

  BaseNodeType* cur_node = &fake_node_;

  DefaultNodeAlloc node_alloc(alloc_);
  while (cur_other != &other.fake_node_) {
    DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(node_alloc, 1);
    try {
      std::allocator_traits<DefaultNodeAlloc>::construct(node_alloc, new_node, cur_node, &fake_node_, static_cast<const DefaultNode<T>*>(cur_other)->val);
    } catch (...) {
      std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, new_node, 1);
      DestroyHead(cur_node);
      size_ = 0;
      throw;
    }

    cur_node->next = static_cast<BaseNodeType*>(new_node);
    cur_node = cur_node->next;
    cur_other = cur_other->next;

    fake_node_.prev = cur_node; ////
  }

  size_ = other.size_;
}

template<typename T, typename AllocT>
uint32_t List<T, AllocT>::size() const {
  return size_;
}

template<typename T, typename AllocT>
bool List<T, AllocT>::empty() const {
  return (size_ == 0);
}

template<typename T, typename AllocT>
void List<T, AllocT>::push_back(const T& val) {
  emplace_back(val);
}

template<typename T, typename AllocT>
void List<T, AllocT>::push_front(const T& val) {
  emplace_front(val);
}

template<typename T, typename AllocT>
void List<T, AllocT>::push_back(T &&val) {
  emplace_back(std::move(val));
}


template<typename T, typename AllocT>
void List<T, AllocT>::push_front(T &&val) {
  emplace_front(std::move(val));
}


template<typename T, typename AllocT>
void List<T, AllocT>::pop_back() {
  if (empty()) {
    throw std::out_of_range("Pop back out of range");
  }
  DefaultNodeAlloc node_alloc(alloc_);
  BaseNodeType* new_prev = fake_node_.prev->prev;
  std::allocator_traits<DefaultNodeAlloc>::destroy(node_alloc, static_cast<DefaultNodeType*>(fake_node_.prev));
  std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, static_cast<DefaultNodeType*>(fake_node_.prev), 1);

  fake_node_.prev = new_prev;
  new_prev->next = &fake_node_;
  --size_;
}

template<typename T, typename AllocT>
const T &List<T, AllocT>::back() const {
//  return *rbegin();
//check size
  return static_cast<DefaultNodeType*>(fake_node_.prev)->val;
}

template<typename T, typename AllocT>
const T &List<T, AllocT>::front() const {
//  return *begin();
  return static_cast<DefaultNodeType*>(fake_node_.next)->val;
}


template<typename T, typename AllocT>
void List<T, AllocT>::pop_front() {
  if (empty()) {
    throw std::out_of_range("Pop front out of range");
  }
  DefaultNodeAlloc node_alloc(alloc_);
  BaseNodeType* new_next = fake_node_.next->next;
  DefaultNodeType* delete_ptr = static_cast<DefaultNodeType*>(fake_node_.next);
  assert(fake_node_.next != &fake_node_);
  std::allocator_traits<DefaultNodeAlloc>::destroy(node_alloc, delete_ptr);
  std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, delete_ptr, 1);
  fake_node_.next = new_next;
  new_next->prev = &fake_node_;

  --size_;
}

template<typename T, typename AllocT>
AllocT& List<T, AllocT>::get_allocator() { return alloc_; }

template<typename T, typename AllocT>
const AllocT& List<T, AllocT>::get_allocator() const { return alloc_; }

template<typename T>
DefaultNode<T>::DefaultNode(const DefaultNode<T>& other):
  BaseNode<T>(other.prev, other.next),
  val(other.val)
{}

template<typename T>
DefaultNode<T>::DefaultNode(DefaultNode<T> &&other) noexcept:
  BaseNode<T>(other.prev, other.next),
  val(std::move(other.val))
{}


template<typename T>
DefaultNode<T>& DefaultNode<T>::operator=(DefaultNode<T> other) {
  std::swap(this->prev, other.prev);
  std::swap(this->next, other.next);
  std::swap(this->val, other.val);
}

template<typename T, typename AllocT>
template<bool is_const>
List<T, AllocT>::Iterator<is_const>::Iterator(const Iterator<is_const> &other): ptr_(other.ptr_) {}

template<typename T, typename AllocT>
template<bool is_const>
List<T, AllocT>::Iterator<is_const>::Iterator(const Iterator<false>& other) requires(is_const) : ptr_(other.ptr_) {}

template<typename T, typename AllocT>
template<bool is_const>
typename List<T, AllocT>::template Iterator<is_const>& List<T, AllocT>::Iterator<is_const>::operator++() {
  ptr_ = ptr_->next;
  return *this;
}

template<typename T, typename AllocT>
template<bool is_const>
typename List<T, AllocT>::template Iterator<is_const> List<T, AllocT>::Iterator<is_const>::operator++(int) {
  Iterator cur = *this;
  ++*this;
  return cur;
}

template<typename T, typename AllocT>
template<bool is_const>
typename List<T, AllocT>::template Iterator<is_const>& List<T, AllocT>::Iterator<is_const>::operator--() {
  ptr_ = ptr_->prev;
  return *this;
}

template<typename T, typename AllocT>
template<bool is_const>
typename List<T, AllocT>::template Iterator<is_const> List<T, AllocT>::Iterator<is_const>::operator--(int) {
  Iterator cur = *this;
  --*this;
  return cur;
}

template<typename T, typename AllocT>
template<bool is_const>
typename List<T, AllocT>::template Iterator<is_const>::value_type& List<T, AllocT>::Iterator<is_const>::operator*() const {
  return static_cast<def_node_val>(ptr_)->val;
}

template<typename T, typename AllocT>
template<bool is_const>
typename List<T, AllocT>::template Iterator<is_const>::value_type* List<T, AllocT>::Iterator<is_const>::operator->() const {
  return &(static_cast<def_node_val>(ptr_)->val);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::iterator List<T, AllocT>::begin() {
  return iterator(fake_node_.next);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_iterator List<T, AllocT>::begin() const {
  return iterator(fake_node_.next);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::iterator List<T, AllocT>::end() {
  return iterator(&fake_node_);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_iterator List<T, AllocT>::end() const {
  return const_iterator(&fake_node_);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_iterator List<T, AllocT>::cbegin() const {
  return const_iterator(fake_node_.next);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_iterator List<T, AllocT>::cend() const {
  return const_iterator(&fake_node_);
}

template<typename T, typename AllocT>
typename List<T, AllocT>::reverse_iterator List<T, AllocT>::rbegin() {
  return reverse_iterator(end());
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_reverse_iterator List<T, AllocT>::rbegin() const {
  return const_reverse_iterator(end());
}

template<typename T, typename AllocT>
typename List<T, AllocT>::reverse_iterator List<T, AllocT>::rend() {
  return reverse_iterator(begin());
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_reverse_iterator List<T, AllocT>::rend() const {
  return const_reverse_iterator(begin());
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_reverse_iterator List<T, AllocT>::crbegin() const {
  return const_reverse_iterator(end());
}

template<typename T, typename AllocT>
typename List<T, AllocT>::const_reverse_iterator List<T, AllocT>::crend() const {
  return const_reverse_iterator(begin());
}

template<typename T, typename AllocT>
void List<T, AllocT>::erase(iterator it) {
  if (it == end()) {
    throw std::out_of_range("Erase out of range");
  }
  BaseNodeType* prev_ptr = std::prev(it).ptr();
  BaseNodeType* next_ptr = std::next(it).ptr();

  BaseNodeType* cur_ptr = it.ptr();

  prev_ptr->next = next_ptr;
  next_ptr->prev = prev_ptr;

  DefaultNodeAlloc node_alloc(alloc_);
  std::allocator_traits<DefaultNodeAlloc>::destroy(node_alloc, static_cast<DefaultNodeType*>(cur_ptr));
  std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, static_cast<DefaultNodeType*>(cur_ptr), 1);
  --size_;
}

template<typename T, typename AllocT>
void List<T, AllocT>::insert(iterator it, const T& val) {
  BaseNodeType* prev_ptr = std::prev(it).ptr();
  BaseNodeType* cur_ptr = it.ptr();

  DefaultNodeAlloc node_alloc(alloc_);
  DefaultNodeType* new_node = std::allocator_traits<DefaultNodeAlloc>::allocate(node_alloc, 1);
  try {
    std::allocator_traits<DefaultNodeAlloc>::construct(node_alloc, new_node, prev_ptr, cur_ptr, val);
  }
  catch (...) {
    std::allocator_traits<DefaultNodeAlloc>::deallocate(node_alloc, new_node, 1);
    throw;
  }

  prev_ptr->next = static_cast<BaseNodeType*>(new_node);
  cur_ptr->prev = static_cast<BaseNodeType*>(new_node);
  ++size_;
}

template<typename T, typename AllocT>
List<T, AllocT>::~List() {
  DestroyHead(fake_node_.prev);
}
