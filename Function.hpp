#include <cstddef>
#include <functional>
#include <iostream>
#include <optional>
#include <typeinfo>

namespace function_detail {
  template<bool IsMove, typename Ret, typename... Args>
  struct ControlBlockMethodsPtrs {};

  template<typename Ret, typename... Args>
  struct ControlBlockMethodsPtrs<false, Ret, Args...> {
    using copy_ptr_t = void*(*)(void*, void*);
    using move_ptr_t = void*(*)(void*, void*);
    using destroy_ptr_t = void(*)(void*, bool);
    copy_ptr_t copy_ptr;
    move_ptr_t move_ptr;
    destroy_ptr_t destroy_ptr;
    const std::type_info& type_info;

    ControlBlockMethodsPtrs(): move_ptr(nullptr), destroy_ptr(nullptr) {};
    ControlBlockMethodsPtrs(const copy_ptr_t& copy_ptr, const move_ptr_t& move_ptr,
                            const destroy_ptr_t& destroy_ptr, const std::type_info& type_info):
      copy_ptr(copy_ptr), move_ptr(move_ptr), destroy_ptr(destroy_ptr), type_info(type_info) {};

    ControlBlockMethodsPtrs(const ControlBlockMethodsPtrs& other) = default;
    ControlBlockMethodsPtrs(ControlBlockMethodsPtrs&& other) = default;
    ControlBlockMethodsPtrs& operator=(const ControlBlockMethodsPtrs& other) = default;
    ~ControlBlockMethodsPtrs() = default;
  };
  template<typename Ret, typename... Args>
  struct ControlBlockMethodsPtrs<true, Ret, Args...> {
    using copy_ptr_t = void*(*)(void*, void*);
    using move_ptr_t = void*(*)(void*, void*);
    using destroy_ptr_t = void(*)(void*, bool);
    move_ptr_t move_ptr;
    destroy_ptr_t destroy_ptr;
    const std::type_info& type_info;

    ControlBlockMethodsPtrs(): move_ptr(nullptr), destroy_ptr(nullptr) {};
    ControlBlockMethodsPtrs(std::nullptr_t, const move_ptr_t& move_ptr,
                            const destroy_ptr_t& destroy_ptr, const std::type_info& type_info):
      move_ptr(move_ptr), destroy_ptr(destroy_ptr), type_info(type_info) {};

    ControlBlockMethodsPtrs(const ControlBlockMethodsPtrs& other) = default;
    ControlBlockMethodsPtrs(ControlBlockMethodsPtrs&& other) = default;
    ControlBlockMethodsPtrs& operator=(const ControlBlockMethodsPtrs& other) = default;
    ~ControlBlockMethodsPtrs() = default;
  };
};

template <bool IsMoveOnly, typename T>
class FunctionBase {};

template <bool IsMoveOnly, typename Ret, typename... Args>
class FunctionBase<IsMoveOnly, Ret(Args...)> {
protected:
  template<typename T>
  struct is_FunctionBase : std::false_type {};
  template<typename T, typename... U>
  struct is_FunctionBase<FunctionBase<IsMoveOnly, T(U...)>> : std::true_type {};

  template<typename T>
  struct is_reference_wrapper : std::false_type {};
  template<typename T>
  struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type {};

  template<typename T>
  constexpr static bool is_FunctionBase_v = is_FunctionBase<T>::value;
  template<typename T>
  constexpr static bool is_reference_wrapper_v = is_reference_wrapper<T>::value;
protected:
  using invoke_ptr_t = Ret(*)(void*, Args...);

  static void SwapFuntions(FunctionBase& f, FunctionBase& s) {
    std::swap(f.func_ptr_, s.func_ptr_);
    std::swap(f.block_ptr_, s.block_ptr_);
    std::swap(f.invoke_ptr_, s.invoke_ptr_);
    std::swap(f.uses_local_storage_, s.uses_local_storage_);
  }

protected:
  template<typename F>
  static void* CopyConstructor(void* func_copy_from, void* func_dest)
  requires(!IsMoveOnly);
  template<typename F>
  static void* CopyConstructor(void* func_copy_from, void* func_dest)
  requires(IsMoveOnly) = delete;
  template<typename F>
  static void* MoveConstrutor(void* func_move_from, void* func_dest);
  template<typename F>
  static void Destroy(void* func, bool uses_local_storage);
  template<typename F>
  static auto Invoke(void* func, Args... args) -> Ret;


  template<typename F>
  static function_detail::ControlBlockMethodsPtrs<IsMoveOnly, Ret, Args...>* get_control_block_F() {
    const std::type_info& cur_inf = typeid(F);
    if constexpr (!IsMoveOnly) {
      static function_detail::ControlBlockMethodsPtrs<false, Ret, Args...> block = function_detail::ControlBlockMethodsPtrs<false, Ret, Args...>(
        &FunctionBase::CopyConstructor< F >,
        &FunctionBase::MoveConstrutor< F >,
        &FunctionBase::Destroy< F >,
        cur_inf
      );
      return &block;
    } else {
      static function_detail::ControlBlockMethodsPtrs<true, Ret, Args...> block = function_detail::ControlBlockMethodsPtrs<true, Ret, Args...>(
        nullptr,
        &FunctionBase::MoveConstrutor < F >,
        &FunctionBase::Destroy < F >,
        cur_inf
      );
      return &block;
    }
  };

protected:
  static constexpr int BUFFER_SIZE = 16;
  void* func_ptr_;
  invoke_ptr_t invoke_ptr_;
  function_detail::ControlBlockMethodsPtrs<IsMoveOnly, Ret, Args...>* block_ptr_;
  alignas(std::max_align_t) char buffer_[BUFFER_SIZE];
  bool uses_local_storage_;
public:
  FunctionBase(std::nullptr_t);
  FunctionBase(): FunctionBase(nullptr) {}

  FunctionBase(const FunctionBase& other)
  requires(!IsMoveOnly);
  FunctionBase(const FunctionBase& other)
  requires(IsMoveOnly) = delete;
  FunctionBase(FunctionBase&& other);

  template<typename Func>
  FunctionBase(Func&& func)
  requires(!is_FunctionBase_v<std::remove_cvref_t<Func>>);
  Ret operator()(Args...) const;

  auto operator=(FunctionBase other) -> FunctionBase&;
  template<typename Func>
  auto operator=(Func &&func) -> FunctionBase &
  requires(!is_FunctionBase_v<std::remove_cvref_t<Func>>);

  operator bool() const;

  template<typename T>
  auto target() -> T*;
  template<typename T>
  auto target() const -> const T*;

  [[ nodiscard ]] auto target_type() const -> const std::type_info&;

  bool Empty() const { return invoke_ptr_ == nullptr; }
  ~FunctionBase();
};

template<bool IsMoveOnly, typename Ret, typename... Args>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::operator=(FunctionBase other) -> FunctionBase & {
  FunctionBase::SwapFuntions(*this, other);
  return *this;
}

template<bool IsMoveOnly, typename Ret, typename... Args>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::target_type() const -> const std::type_info& {
  if (!Empty()) {
    return block_ptr_->type_info;
  }
  return typeid(void);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template<typename T>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::target() const -> const T * {
  if (!Empty() && block_ptr_->type_info == typeid(T)) {
    return static_cast<const T*>(func_ptr_);
  }
  return nullptr;
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template<typename T>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::target() -> T * {
  if (!Empty() && block_ptr_->type_info == typeid(T)) {
    return static_cast<T*>(func_ptr_);
  }
  return nullptr;
}

template<bool IsMoveOnly, typename Ret, typename... Args>
FunctionBase<IsMoveOnly, Ret(Args...)>::operator bool() const {
  return (invoke_ptr_ != nullptr);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template<typename Func>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::operator=(
  Func &&func) -> FunctionBase &
requires(!is_FunctionBase_v<std::remove_cvref_t<Func>>) {
  FunctionBase tmp(func);
  SwapFuntions(*this, tmp);
  return *this;
}

template<bool IsMoveOnly, typename Ret, typename... Args>
FunctionBase<IsMoveOnly, Ret(Args...)>::FunctionBase(const FunctionBase &other)
requires(!IsMoveOnly):
  invoke_ptr_(other.invoke_ptr_),
  block_ptr_(other.block_ptr_),
  func_ptr_(nullptr),
  uses_local_storage_(other.uses_local_storage_) {
  if (other.Empty()) {
    return;
  }

  if (other.uses_local_storage_) {
    func_ptr_ = buffer_;
  }
  func_ptr_ = block_ptr_->copy_ptr(other.func_ptr_, func_ptr_);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
FunctionBase<IsMoveOnly, Ret(Args...)>::FunctionBase(FunctionBase &&other):
  invoke_ptr_(other.invoke_ptr_),
  block_ptr_(other.block_ptr_),
  func_ptr_(nullptr),
  uses_local_storage_(other.uses_local_storage_) {
  if (other.Empty()) {
    return;
  }

  if (other.uses_local_storage_) {
    func_ptr_ = buffer_;
    func_ptr_ = block_ptr_->move_ptr(other.func_ptr_, func_ptr_);

    other.block_ptr_->destroy_ptr(other.func_ptr_, other.uses_local_storage_);
    other.func_ptr_ = nullptr;
  } else {
    func_ptr_ = other.func_ptr_,
    other.func_ptr_ = nullptr;
  }

  other.block_ptr_ = nullptr;
  other.invoke_ptr_ = nullptr;
  other.uses_local_storage_ = false;
}

template<bool IsMoveOnly, typename Ret, typename... Args>
FunctionBase<IsMoveOnly, Ret(Args...)>::FunctionBase(std::nullptr_t):
  func_ptr_(nullptr),
  invoke_ptr_(nullptr),
  block_ptr_(nullptr),
  uses_local_storage_(false) {}

template<bool IsMoveOnly, typename Ret, typename... Args>
template<typename F>
void* FunctionBase<IsMoveOnly, Ret(Args...)>::MoveConstrutor(void *func_move_from, void *func_dest) {
  F* src_F = static_cast<F*>(func_move_from);
  F* dst_F = static_cast<F*>(func_dest);
  if (func_dest != nullptr) {
    new (dst_F) F(std::move(*src_F));
  } else {
    dst_F = new F(std::move(*src_F));
  }
  return static_cast<void*>(dst_F);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template<typename F>
void* FunctionBase<IsMoveOnly, Ret(Args...)>::CopyConstructor(void* func_copy_from, void* func_dest)
requires(!IsMoveOnly) {
  F* func_dest_F = static_cast<F*>(func_dest);
  F* func_src_F = static_cast<F*>(func_copy_from);
  if (func_dest != nullptr) {
    new (func_dest_F) F(*func_src_F);
  } else {
    func_dest_F = new F(*func_src_F);
  }
  return static_cast<void*>(func_dest_F);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template<typename Func>
FunctionBase<IsMoveOnly, Ret(Args...)>::FunctionBase(Func &&func)
requires(!is_FunctionBase_v<std::remove_cvref_t<Func>>) {
  using FuncType = std::remove_cvref_t<Func>;
  using StoredType = FuncType;

  invoke_ptr_ = &FunctionBase::Invoke<StoredType>;
  block_ptr_ = get_control_block_F<StoredType>();

  constexpr bool fits = (sizeof(StoredType) <= BUFFER_SIZE) &&
                        (alignof(StoredType) <= alignof(decltype(buffer_)));

  void* dest = (fits ? static_cast<void*>(buffer_) : nullptr);

  if constexpr (std::is_lvalue_reference_v<Func>) {
    func_ptr_ = block_ptr_->copy_ptr(&func, dest);
  } else {
    func_ptr_ = block_ptr_->move_ptr(&func, dest);
  }
  uses_local_storage_ = (func_ptr_ == buffer_);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template <typename F>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::Invoke(void* func, Args... args) -> Ret {
  return std::invoke(*static_cast<F*>(func), std::forward<Args>(args)...);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::operator()(Args... args) const -> Ret {
  if (Empty()) {
    throw std::bad_function_call();
  }
  return invoke_ptr_(func_ptr_, std::forward<Args>(args)...);
}

template<bool IsMoveOnly, typename Ret, typename... Args>
FunctionBase<IsMoveOnly, Ret(Args...)>::~FunctionBase() {
  if (!Empty()) {
    block_ptr_->destroy_ptr(func_ptr_, uses_local_storage_);
  }
}

template<bool IsMoveOnly, typename Ret, typename... Args>
template <typename F>
auto FunctionBase<IsMoveOnly, Ret(Args...)>::Destroy(void* func, bool uses_local_storage) -> void {
  if (uses_local_storage) {
    static_cast<F*>(func)->~F();
  } else {
    delete static_cast<F*>(func);
  }
}

template<typename T>
using Function = FunctionBase<false, T>;

template<typename T>
using MoveOnlyFunction = FunctionBase<true, T>;

