#include <iostream>
#include <memory>
#include <cassert>
#include <cstring>

template<typename T>
class WeakPtr;
template<typename T>
class EnableSharedFromThis;
template<typename T>
class SharedPtr;

namespace utils {
  template<size_t N, typename... Args>
  struct get_type {};

  template<typename Head, typename... Tail>
  struct get_type<0, Head, Tail...> {
    using type = Head;
  };

  template<size_t N, typename Head, typename... Tail>
  struct get_type<N, Head, Tail...> {
    using type = get_type<N - 1, Tail...>::type;
  };
  template<typename T>
  struct is_shared_ptr : std::false_type {};
  template<typename T>
  struct is_shared_ptr<SharedPtr<T>> : std::true_type {};

  template<size_t N, typename... Args>
  using get_type_t = typename get_type<N, Args...>::type;
  template<typename T>
  constexpr bool is_shared_ptr_v = is_shared_ptr<T>::value;


}

template<typename T>
struct Debug {
  Debug() = delete;
};

struct ControlBlockBase {
  int32_t shared_cnt;
  int32_t weak_cnt;
  ControlBlockBase(): shared_cnt(0), weak_cnt(0) {}

  virtual void destroy_obj() = 0;
  virtual void delete_self() = 0;

  virtual ~ControlBlockBase() = default;
};

template<typename T>
class SharedPtr {
public:
  SharedPtr() noexcept: val_ptr_(nullptr), block_ptr_(nullptr) {}
  explicit SharedPtr(T* ptr);
  template<typename U>
  explicit SharedPtr(U* ptr);

  template<typename... Args>
  explicit SharedPtr(Args&&... args) requires(
    sizeof...(Args) > 0 &&
    (sizeof...(Args) != 1 || !utils::is_shared_ptr_v< std::decay_t<typename utils::get_type_t<0, Args...>> >)
  );
  template<typename Alloc, typename... Args>
  explicit SharedPtr(std::allocator_arg_t, Alloc alloc, Args&&... args) requires(
    sizeof...(Args) > 0 &&
    (sizeof...(Args) != 1 || !utils::is_shared_ptr_v< std::decay_t<typename utils::get_type_t<0, Args...>> >)
  );

  SharedPtr(const SharedPtr& other);
  SharedPtr(SharedPtr&& other) noexcept;

  template<typename U>
  SharedPtr(const SharedPtr<U>& other)
  requires(std::is_base_of_v<T, U>);
  template<typename U>
  SharedPtr(SharedPtr<U>&& other) noexcept
  requires(std::is_base_of_v<T, U>);

  SharedPtr& operator=(SharedPtr other);

  int32_t use_count() const;

  void reset(std::nullptr_t);
  template<typename U>
  void reset(U* other_ptr);

  template<typename U, typename DeleterU = std::default_delete<U>>
  SharedPtr(U* ptr, DeleterU deleter);
  template<typename U, typename DeleterU = std::default_delete<U>, typename AllocU = std::allocator<U>>
  SharedPtr(U* ptr, DeleterU deleter, AllocU alloc);

  template<typename U>
  SharedPtr(SharedPtr<U> const& r, T* ptr) noexcept;

  T* get() const { return val_ptr_; }
  T* operator->() const { return  val_ptr_; }
  ~SharedPtr();
private:
  template<typename U, typename DeleterU = std::default_delete<U>, typename AllocU = std::allocator<U>>
  struct ControlBlockRegular : ControlBlockBase {
    using ControlBlockRegularAlloc = std::allocator_traits<AllocU>::template rebind_alloc< ControlBlockRegular<U, DeleterU, AllocU> >;
    U* ptr;
    [[ no_unique_address ]] AllocU alloc;
    [[ no_unique_address ]] DeleterU deleter;

    void destroy_obj() override;
    void delete_self() override;

    ControlBlockRegular();
    explicit ControlBlockRegular(U* ptr);
    explicit ControlBlockRegular(DeleterU deleter);
    explicit ControlBlockRegular(U* ptr, DeleterU& deleter);
    explicit ControlBlockRegular(U* ptr, DeleterU& deleter, AllocU& alloc);
    ControlBlockRegular(AllocU alloc, DeleterU deleter);

    ControlBlockRegular(const ControlBlockRegular& other);
    ControlBlockRegular(ControlBlockRegular&& other) noexcept;

    auto operator=(ControlBlockRegular other) -> ControlBlockRegular&;

    ~ControlBlockRegular() override;
  };

  template<typename U, typename AllocU = std::allocator<U>>
  struct ControlBlockMakeShared : ControlBlockBase {
    using ControlBlockSharedAlloc = std::allocator_traits<AllocU>::template rebind_alloc< ControlBlockMakeShared<U, AllocU> >;
    alignas(U) unsigned char storage[sizeof(U)];
    [[ no_unique_address]] AllocU alloc;

    void destroy_obj() override;
    void delete_self() override;
    U* val_ptr() { return reinterpret_cast<U*>(&storage); }

    template<typename... Args>
    explicit ControlBlockMakeShared(Args&&... args);
    template<typename... Args>
    explicit ControlBlockMakeShared(std::allocator_arg_t, AllocU alloc, Args&&... args);

    ControlBlockMakeShared(const ControlBlockMakeShared& other) = delete;
    ControlBlockMakeShared(ControlBlockMakeShared&& other) = delete;

    ControlBlockMakeShared& operator=(ControlBlockMakeShared other) = delete;

    ~ControlBlockMakeShared() override = default;
  };

  T* val_ptr_;
  ControlBlockBase* block_ptr_;

  template<typename U>
  friend class WeakPtr;
  template<typename U>
  friend class SharedPtr;

  template<typename U>
  SharedPtr(const WeakPtr<U>& other);
  template<typename U>
  SharedPtr(WeakPtr<U>&& other);

  void SwapSharedPtr(SharedPtr& other);

  template<typename Ptr>
  friend void DeleteObjIfNeeded(const Ptr& smart_ptr);
  template<typename U>
  friend class EnableSharedFromThis;

  SharedPtr(T* val_ptr, ControlBlockBase* block_ptr): val_ptr_(val_ptr), block_ptr_(block_ptr) {
    ++block_ptr_->shared_cnt;
  }
};

template<typename T>
void SharedPtr<T>::reset(std::nullptr_t) {
  if (!block_ptr_) {
    return;
  }
  --block_ptr_->shared_cnt;
  DeleteObjIfNeeded(*this);

  val_ptr_ = nullptr;
  block_ptr_ = nullptr;
}

template<typename T>
template<typename U>
SharedPtr<T>::SharedPtr(const SharedPtr<U>& other)
requires(std::is_base_of_v<T, U>):
  val_ptr_(other.val_ptr_),
  block_ptr_(other.block_ptr_) {
  if (block_ptr_) {
    ++block_ptr_->shared_cnt;
  }
}

template<typename T>
template<typename U>
SharedPtr<T>::SharedPtr(SharedPtr<U>&& other) noexcept
requires(std::is_base_of_v<T, U>):
  val_ptr_(std::move(other.val_ptr_)),
  block_ptr_(std::move(other.block_ptr_)) {
  other.val_ptr_ = nullptr;
  other.block_ptr_ = nullptr;
}

template<typename T>
template<typename... Args>
SharedPtr<T>::SharedPtr(Args&&... args) requires(
  sizeof...(Args) > 0 &&
  (sizeof...(Args) != 1 || !utils::is_shared_ptr_v< std::decay_t<typename utils::get_type_t<0, Args...>> >)
) {
  block_ptr_ = new ControlBlockMakeShared<T>(std::forward<Args>(args)...);
  ++block_ptr_->shared_cnt;

  val_ptr_ = std::launder(
    reinterpret_cast<T*>(static_cast<ControlBlockMakeShared<T>*>(block_ptr_)->storage)
  );

  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    val_ptr_->s_val_ptr_ = val_ptr_;
    val_ptr_->s_block_ptr_ = block_ptr_;
  }
}

template<typename T>
template<typename AllocT, typename... Args>
SharedPtr<T>::SharedPtr(std::allocator_arg_t, AllocT alloc, Args &&... args) requires(
  sizeof...(Args) > 0 &&
  (sizeof...(Args) != 1 || !utils::is_shared_ptr_v< std::decay_t<typename utils::get_type_t<0, Args...>> >)
) {
  using BlockMakeSharedAlloc = std::allocator_traits<AllocT>::template rebind_alloc< ControlBlockMakeShared<T, AllocT> >;
  BlockMakeSharedAlloc block_alloc(alloc);
  ControlBlockMakeShared<T, AllocT>* make_shared_block =
    std::allocator_traits<BlockMakeSharedAlloc>::allocate(block_alloc, 1);
  try {
    std::allocator_traits<BlockMakeSharedAlloc>::construct(block_alloc, make_shared_block, std::allocator_arg, alloc,
                                                           std::forward<Args>(args)...);
  } catch (...) {
    std::allocator_traits<BlockMakeSharedAlloc>::deallocate(block_alloc, make_shared_block, 1);
    throw;
  }

  block_ptr_ = make_shared_block;
  ++block_ptr_->shared_cnt;
  val_ptr_ = std::launder(
    reinterpret_cast<T*>(static_cast<ControlBlockMakeShared<T>*>(block_ptr_)->storage)
  );

  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    val_ptr_->s_val_ptr_ = val_ptr_;
    val_ptr_->s_block_ptr_ = block_ptr_;
  }
}

template<typename T>
template<typename U>
SharedPtr<T>::SharedPtr(const SharedPtr<U> &r, T *ptr) noexcept: SharedPtr(r) {
  val_ptr_ = ptr;
}

template<typename T>
template<typename U>
SharedPtr<T>::SharedPtr(WeakPtr<U> &&other) {
  if (!other.block_ptr_ || other.block_ptr_->shared_cnt == 0) {
    throw std::bad_weak_ptr();
  }
  val_ptr_ = std::move(other.val_ptr_);
  block_ptr_ = std::move(other.block_ptr_);

  assert(block_ptr_->weak_cnt && block_ptr_->shared_cnt);
  ++block_ptr_->shared_cnt;
  --block_ptr_->weak_cnt;

  other.val_ptr_ = nullptr;
  other.block_ptr_ = nullptr;
}

template<typename T>
template<typename U>
SharedPtr<T>::SharedPtr(const WeakPtr<U> &other) {
  if (!other.block_ptr_ || other.block_ptr_->shared_cnt == 0) {
    throw std::bad_weak_ptr();
  }
  val_ptr_ = other.val_ptr_;
  block_ptr_ = other.block_ptr_;

  ++block_ptr_->shared_cnt;
}

template<typename T>
template<typename U, typename AllocU>
template<typename... Args>
SharedPtr<T>::ControlBlockMakeShared<U, AllocU>::ControlBlockMakeShared(std::allocator_arg_t, AllocU alloc, Args &&... args):
  alloc(alloc)
{
  std::allocator_traits<AllocU>::construct(alloc, val_ptr(), std::forward<Args>(args)...);
}

template<typename T>
template<typename U, typename AllocU>
template<typename... Args>
SharedPtr<T>::ControlBlockMakeShared<U, AllocU>::ControlBlockMakeShared(Args &&... args) {
  std::allocator_traits<AllocU>::construct(alloc, val_ptr(), std::forward<Args>(args)...);
}

template<typename T>
template<typename U, typename AllocU>
void SharedPtr<T>::ControlBlockMakeShared<U, AllocU>::delete_self() {
  ControlBlockSharedAlloc block_alloc(alloc);
  std::allocator_traits<ControlBlockSharedAlloc>::destroy(block_alloc, this);
  std::allocator_traits<ControlBlockSharedAlloc>::deallocate(block_alloc, this, 1);
}

template<typename T>
template<typename U, typename AllocU>
void SharedPtr<T>::ControlBlockMakeShared<U, AllocU>::destroy_obj() {
  std::allocator_traits<AllocU>::destroy(alloc, val_ptr());
}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::~ControlBlockRegular() {
  deleter(ptr);
}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
auto SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::operator=(ControlBlockRegular other)
-> SharedPtr::ControlBlockRegular<U, DeleterU, AllocU>& {
  std::swap(ptr, other.ptr);
  std::swap(alloc, other.alloc);
  std::swap(deleter, other.deleter);
  return *this;
}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(const ControlBlockRegular &other):
  ptr(other.ptr),
  alloc(other.alloc),
  deleter(other.deleter)
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(ControlBlockRegular &&other) noexcept:
  ptr(std::move(other.ptr)),
  alloc(std::move(other.alloc)),
  deleter(std::move(other.deleter))
{
  other.ptr = nullptr;
}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(AllocU alloc, DeleterU deleter):
  ptr(nullptr),
  alloc(alloc),
  deleter(deleter),
  ControlBlockBase()
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(U* ptr, DeleterU& deleter):
  ptr(ptr),
  alloc(AllocU{}),
  deleter(deleter),
  ControlBlockBase()
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(U* ptr, DeleterU& deleter, AllocU& alloc):
  ptr(ptr),
  alloc(alloc),
  deleter(deleter),
  ControlBlockBase()
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(DeleterU deleter):
  ptr(nullptr),
  alloc(AllocU{}),
  deleter(deleter),
  ControlBlockBase()
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular(U *ptr):
  ptr(ptr),
  alloc(AllocU{}),
  deleter(DeleterU{}),
  ControlBlockBase()
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegular():
  ptr(nullptr),
  alloc(AllocU{}),
  deleter(DeleterU{}),
  ControlBlockBase()
{}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
void SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::delete_self() {
  ControlBlockRegularAlloc block_alloc(alloc);
  std::allocator_traits<ControlBlockRegularAlloc>::destroy(block_alloc, this);
  std::allocator_traits<ControlBlockRegularAlloc>::deallocate(block_alloc, this, 1);
}

template<typename T>
template<typename U, typename DeleterU, typename AllocU>
void SharedPtr<T>::ControlBlockRegular<U, DeleterU, AllocU>::destroy_obj() {
  deleter(ptr);
  ptr = nullptr;
}


template<typename T>
template<typename U, typename DeleterU, typename AllocU>
SharedPtr<T>::SharedPtr(U *ptr, DeleterU deleter, AllocU alloc) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  using BlockAlloc = typename ControlBlockRegular<U, DeleterU, AllocU>::ControlBlockRegularAlloc;
  BlockAlloc block_alloc(alloc);
  ControlBlockRegular<U, DeleterU, AllocU>* reg_block_ptr = std::allocator_traits<BlockAlloc>::allocate(block_alloc, 1);
  try {
    std::allocator_traits<BlockAlloc>::construct(block_alloc, reg_block_ptr, ptr, deleter, alloc);
  } catch (...) {
    std::allocator_traits<BlockAlloc>::deallocate(block_alloc, reg_block_ptr, 1);
    throw;
  }

  val_ptr_ = ptr;
  block_ptr_ = reg_block_ptr;

  ++block_ptr_->shared_cnt;
}

template<typename T>
template<typename U, typename DeleterU>
SharedPtr<T>::SharedPtr(U *ptr, DeleterU deleter) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  val_ptr_ = ptr;
  block_ptr_ = new ControlBlockRegular<U, DeleterU>(ptr, deleter);
  ++block_ptr_->shared_cnt;
}

template<typename T>
int32_t SharedPtr<T>::use_count() const {
  return (block_ptr_ ? block_ptr_->shared_cnt : 0);
}

template<typename T>
template<typename U>
void SharedPtr<T>::reset(U* other_ptr) {
  static_assert(std::is_base_of_v<T, U> || std::is_same_v<T, U>);
  if (block_ptr_) {
    --block_ptr_->shared_cnt;
  }
  DeleteObjIfNeeded(*this);

  val_ptr_ = other_ptr;
  block_ptr_ = new ControlBlockRegular<U>(other_ptr);
  ++block_ptr_->shared_cnt;
}

template<typename T>
void SharedPtr<T>::SwapSharedPtr(SharedPtr &other) {
  std::swap(val_ptr_, other.val_ptr_);
  std::swap(block_ptr_, other.block_ptr_);
}

template<typename T>
SharedPtr<T> &SharedPtr<T>::operator=(SharedPtr other) {
  SwapSharedPtr(other);
  return *this;
}

template<typename T>
SharedPtr<T>::SharedPtr(T* ptr): val_ptr_(ptr), block_ptr_(new ControlBlockRegular(ptr)) {
  ++block_ptr_->shared_cnt;
}

template<typename T>
template<typename U>
SharedPtr<T>::SharedPtr(U* ptr) {
  static_assert(std::is_same_v<T, U> || std::is_base_of_v<T, U>);

  val_ptr_ = ptr;
  block_ptr_ = new ControlBlockRegular(ptr);
  if constexpr (std::is_base_of_v<EnableSharedFromThis<T>, T>) {
    val_ptr_->s_block_ptr_ = block_ptr_;
    val_ptr_->s_val_ptr_ = val_ptr_;
  }

  ++block_ptr_->shared_cnt;
}

template<typename Ptr>
void DeleteObjIfNeeded(const Ptr& ptr) {
  if (!ptr.block_ptr_) {
    return;
  }
  int32_t shared_cnt = ptr.block_ptr_->shared_cnt;
  int32_t weak_cnt = ptr.block_ptr_->weak_cnt;
  if (shared_cnt == 0) {
    ptr.block_ptr_->destroy_obj();
  }
  if (shared_cnt == 0 && weak_cnt == 0) {
    ptr.block_ptr_->delete_self();
  }
}

template<typename T>
SharedPtr<T>::~SharedPtr() {
  if (block_ptr_) {
    --block_ptr_->shared_cnt;
  }
  DeleteObjIfNeeded(*this);
}

template<typename T>
SharedPtr<T>::SharedPtr(const SharedPtr &other): val_ptr_(other.val_ptr_), block_ptr_(other.block_ptr_) {
  if (block_ptr_) {
    ++block_ptr_->shared_cnt;
  }
}

template<typename T>
SharedPtr<T>::SharedPtr(SharedPtr &&other) noexcept: val_ptr_(other.val_ptr_), block_ptr_(other.block_ptr_) {
  other.val_ptr_ = nullptr;
  other.block_ptr_ = nullptr;
}

template<typename T>
class WeakPtr {
public:
  WeakPtr() noexcept : val_ptr_(nullptr), block_ptr_(nullptr) {}
  template<typename U>
  WeakPtr(const SharedPtr<U>& other);
  template<typename U>
  WeakPtr(SharedPtr<U>&& other);

  WeakPtr(const WeakPtr& other);
  WeakPtr(WeakPtr&& other);

  template<typename U>
  WeakPtr(const WeakPtr<U>& other);
  template<typename U>
  WeakPtr(WeakPtr<U>&& other);

  WeakPtr& operator=(WeakPtr other);
  template<typename U>
  WeakPtr& operator=(const WeakPtr<U>& other);
  template<typename U>
  WeakPtr& operator=(WeakPtr<U>&& other);

  bool expired() const;
  SharedPtr<T> lock() const;
  ~WeakPtr();
private:
  T* val_ptr_;
  ControlBlockBase* block_ptr_;

  template<typename U>
  friend class SharedPtr;

  template<typename Ptr>
  friend void DeleteObjIfNeeded(const Ptr& smart_ptr);
};

template<typename T>
WeakPtr<T>::~WeakPtr() {
  if (block_ptr_) {
    --block_ptr_->weak_cnt;
    DeleteObjIfNeeded(*this);
  }
}

template<typename T>
SharedPtr<T> WeakPtr<T>::lock() const {
  return (expired() ? SharedPtr<T>(static_cast<T*>(nullptr)) : SharedPtr<T>(*this));
}

template<typename T>
bool WeakPtr<T>::expired() const {
  return (block_ptr_ ? block_ptr_->shared_cnt == 0 : true);
}

template<typename T>
template<typename U>
auto WeakPtr<T>::operator=(const WeakPtr<U> &other) -> WeakPtr& {
  if (block_ptr_ == other.block_ptr_) {
    return *this;
  }
  static_assert(std::is_base_of_v<T, U>);
  if (block_ptr_) {
    --block_ptr_->weak_cnt;
  }
  DeleteObjIfNeeded(*this);

  val_ptr_ = other.val_ptr_;
  block_ptr_ = other.block_ptr_;
  if (block_ptr_) {
    ++block_ptr_->weak_cnt;
  }
  return *this;
}

template<typename T>
template<typename U>
auto WeakPtr<T>::operator=(WeakPtr<U> &&other) -> WeakPtr& {
  static_assert(std::is_base_of_v<T, U>);
  if (block_ptr_ == other.block_ptr_) {
    return *this;
  }
  if (block_ptr_) {
    --block_ptr_->weak_cnt;
  }
  DeleteObjIfNeeded(*this);

  val_ptr_ = other.val_ptr_;
  block_ptr_ = other.block_ptr_;

  other.val_ptr_ = nullptr;
  other.block_ptr_ = nullptr;
  return *this;
}

template<typename T>
auto WeakPtr<T>::operator=(WeakPtr other) -> WeakPtr& {
  std::swap(val_ptr_, other.val_ptr_);
  std::swap(block_ptr_, other.block_ptr_);
  return *this;
}

template<typename T>
WeakPtr<T>::WeakPtr(WeakPtr &&other): val_ptr_(other.val_ptr_), block_ptr_(other.block_ptr_) {
  other.val_ptr_ = nullptr;
  other.block_ptr_ = nullptr;
}

template<typename T>
WeakPtr<T>::WeakPtr(const WeakPtr &other): val_ptr_(other.val_ptr_), block_ptr_(other.block_ptr_) {
  if (block_ptr_) {
    ++block_ptr_->weak_cnt;
  }
}

template<typename T>
template<typename U>
WeakPtr<T>::WeakPtr(SharedPtr<U> &&other): val_ptr_(other.val_ptr_), block_ptr_(other.block_ptr_) {
  static_assert(std::is_same_v<U, T> || std::is_base_of_v<T, U>);
  if (block_ptr_) {
    ++block_ptr_->weak_cnt;
  }
}

template<typename T>
template<typename U>
WeakPtr<T>::WeakPtr(const SharedPtr<U> &other): val_ptr_(other.val_ptr_), block_ptr_(other.block_ptr_) {
  static_assert(std::is_same_v<U, T> || std::is_base_of_v<T, U>);
  if (block_ptr_) {
    ++block_ptr_->weak_cnt;
  }
}

template<typename T>
class EnableSharedFromThis {
public:
  EnableSharedFromThis() = default;

  ~EnableSharedFromThis() = default;

  SharedPtr<T> shared_from_this() const {
    if (!s_block_ptr_ || s_block_ptr_->shared_cnt == 0) {
      throw std::bad_weak_ptr();
    }
    return SharedPtr<T>(s_val_ptr_, s_block_ptr_);
  }
private:
  ControlBlockBase* s_block_ptr_;
  T* s_val_ptr_;

  template<class U>
  friend class SharedPtr;
};

namespace smart_ptrs {
  template<typename T, typename... Args>
  SharedPtr<T> makeShared(Args&&... args) {
    return SharedPtr<T>(std::forward<Args>(args)...);
  }

  template<typename T, typename Alloc, typename... Args>
  SharedPtr<T> makeShared(std::allocator_arg_t, Alloc alloc, Args&&... args) {
    return SharedPtr<T>(alloc, std::forward<Args>(args)...);
  }
};


