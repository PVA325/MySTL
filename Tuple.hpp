#include <iostream>
#include <type_traits>
#include <utility>


template<size_t N, typename... Args>
struct GetType {};

template<size_t N, typename Head, typename... Tail>
struct GetType<N, Head, Tail...> {
  using type = typename GetType<N - 1, Tail...>::type;
};

template<typename Head, typename... Tail>
struct GetType<0, Head, Tail...> {
  using type = Head;
};

template<typename... Types>
class tuple {};

namespace detail {
  template<typename... Types>
  struct is_types_copy_constructible : std::conjunction<std::is_copy_constructible<Types>...> {};

  template<typename... Types>
  struct is_types_move_constructible : std::conjunction<std::is_move_constructible<Types>...> {};

  template<typename... Types>
  struct is_types_default_constructible : std::conjunction<std::is_default_constructible<Types>...> {};

  template<typename... Types>
  struct is_types_copy_assignable : std::conjunction<std::is_copy_assignable<Types>...> {};

  template<typename... Types>
  struct is_types_move_assignable : std::conjunction<std::is_move_assignable<Types>...> {};

  template<typename... Types>
  struct is_tuple_types_copy_assignable_universal {};
  template<typename... Types, typename... UTypes>
  struct is_tuple_types_copy_assignable_universal<tuple<Types...>, tuple<UTypes...>>  : std::conjunction<std::is_assignable<Types&, const UTypes&>...> {};

  template<typename... Types>
  struct is_tuple_types_move_assignable_universal {};
  template<typename... Types, typename... UTypes>
  struct is_tuple_types_move_assignable_universal<tuple<Types...>, tuple<UTypes...>>  : std::conjunction<std::is_assignable<Types&, UTypes>...> {};


  template<typename... Types>
  struct is_copy_list_initializable : std::conjunction<std::is_constructible<Types, std::initializer_list< std::remove_reference_t<Types> >>...> {};

  template<typename... Types>
  struct is_tuple_types_convertible {};
  template<typename... Types, typename... UTypes>
  struct is_tuple_types_convertible<tuple<Types...>, tuple<UTypes...> >
    : std::conjunction<std::is_convertible<Types, UTypes>...> {
  };

  template<typename... Types>
  struct is_types_copy_convertible : is_tuple_types_convertible<tuple<Types...>, tuple<const Types &...>> {};

  template<typename... Types>
  struct is_tuple_types_constructible {};
  template<typename... Types, typename... UTypes>
  struct is_tuple_types_constructible<tuple<Types...>, tuple<UTypes...> >
    : std::conjunction<std::is_constructible<Types, UTypes>...> {};

  template<typename... Types>
  struct is_tuple_sizes_equal {};
  template<typename... Types, typename... UTypes>
  struct is_tuple_sizes_equal<tuple<Types...>, tuple<UTypes...> > {
    static constexpr bool value = (sizeof...(Types) == sizeof...(UTypes));
  };

  template<bool IsMove, typename Tuple>
  struct other_ref;
  template<typename Tuple>
  struct other_ref<true, Tuple> {
    using type = Tuple &&;
  };
  template<typename Tuple>
  struct other_ref<false, Tuple> {
    using type = const Tuple &;
  };
  template<bool IsMove, typename Tuple>
  using other_ref_t = typename other_ref<IsMove, Tuple>::type;


  template<bool IsMoveOther, typename... Types>
  struct constructible_tuple {
  };

  template<bool IsMoveOther, typename First, typename Second>
  struct constructible_tuple<IsMoveOther, tuple<First>, tuple<Second>> {
    using ThisRef = First;
    using OtherRef = decltype(get<0>(std::declval<other_ref_t<IsMoveOther, tuple<Second> >>()));

    static constexpr bool value = std::is_constructible_v<ThisRef, OtherRef>;
  };

  template<bool IsMoveOther, typename Head, typename... Tail, typename UHead, typename... UTail>
  struct constructible_tuple<IsMoveOther, tuple<Head, Tail...>, tuple<UHead, UTail...>>
    : std::conjunction<
      constructible_tuple<IsMoveOther, tuple<Head>, tuple<UHead> >,
      constructible_tuple<IsMoveOther, tuple<Tail...>, tuple<UTail...> >
    > {};

  template<bool IsMove, typename... Types>
  struct converting_constructor_third_condition {
  };
  template<bool IsMove, typename F, typename S, typename... Types, typename... UTypes> // statuc assert Types == Utypes
  struct converting_constructor_third_condition<IsMove, tuple<F, S, Types...>, tuple<UTypes...> > : std::true_type {
  };

  template<bool IsMove, typename F, typename S>
  struct converting_constructor_third_condition<IsMove, tuple<F>, tuple<S> > {
    using OtherRef = other_ref_t<IsMove, tuple<S>>;
    static constexpr bool value =
      (!std::is_convertible_v<OtherRef, F>) && (!std::is_constructible_v<F, OtherRef>) && (!std::is_same_v<F, S>);
  };

  template<bool IsMove, typename... Types>
  struct is_tuple_types_convertible_fwb {};

  template<bool IsMove, typename Head, typename UHead>
  struct is_tuple_types_convertible_fwb<IsMove, tuple<Head>, tuple<UHead>> {
    using ThisRef = Head;
    using OtherRef = decltype(get<0>( std::declval< other_ref_t<IsMove, tuple<Head>> >()));
    static constexpr bool value = std::is_convertible_v<OtherRef, ThisRef>;
  };

  template<bool IsMove, typename Head, typename... Tail, typename UHead, typename... UTail>
  struct is_tuple_types_convertible_fwb<IsMove, tuple<Head, Tail...>, tuple<UHead, UTail...>> : std::conjunction<
    is_tuple_types_convertible_fwb<IsMove, tuple<Head>, tuple<UHead>>,
    is_tuple_types_convertible_fwb<IsMove, tuple<Tail...>, tuple<UTail...>>
  > {};

  template<typename T, typename... Types>
  struct tuple_cnt_type_T {};
  template<typename T>
  struct tuple_cnt_type_T<T, tuple<>> {
    static constexpr u_int32_t value = 0;
  };
  template<typename T, typename Head, typename... Tail>
  struct tuple_cnt_type_T<T, tuple<Head, Tail...>> {
    static constexpr u_int32_t value = (std::is_same_v<std::decay_t<Head>, T> ? 1 : 0) + tuple_cnt_type_T<T, tuple<Tail...>>::value;
  };

  template<typename... Types>
  constexpr bool is_types_copy_constructible_v = is_types_copy_constructible<Types...>::value;
  template<typename... Types>
  constexpr bool is_types_move_constructible_v = is_types_move_constructible<Types...>::value;
  template<typename... Types>
  constexpr bool is_types_default_constructible_v = is_types_default_constructible<Types...>::value;
  template<typename... Types>
  constexpr bool is_types_copy_assignable_v = is_types_copy_assignable<Types...>::value;
  template<typename... Types>
  constexpr bool is_types_move_assignable_v = is_types_move_assignable<Types...>::value;

  template<typename Tuple1, typename Tuple2>
  constexpr bool is_tuple_types_copy_assignable_universal_v = is_tuple_types_copy_assignable_universal<Tuple1, Tuple2>::value;
  template<typename Tuple1, typename Tuple2>
  constexpr bool is_tuple_types_move_assignable_universal_v = is_tuple_types_move_assignable_universal<Tuple1, Tuple2>::value;

  template<typename... Types>
  constexpr bool is_copy_list_initializable_v = is_copy_list_initializable<Types...>::value;
  template<typename... Types>
  constexpr bool is_types_copy_convertible_v = is_types_copy_convertible<Types...>::value;
  template<typename Tuple1, typename Tuple2>
  constexpr bool is_tuple_types_constructible_v = is_tuple_types_constructible<Tuple1, Tuple2>::value;
  template<typename Tuple1, typename Tuple2>
  constexpr bool is_tuple_sizes_equal_v = is_tuple_sizes_equal<Tuple1, Tuple2>::value;
  template<typename Tuple1, typename Tuple2>
  constexpr bool is_tuple_types_convertible_v = is_tuple_types_convertible<Tuple1, Tuple2>::value;
  template<bool IsMoveOther, typename Tuple1, typename Tuple2>
  constexpr bool constructible_tuple_v = constructible_tuple<IsMoveOther, Tuple1, Tuple2>::value;
  template<bool IsMove, typename F, typename S>
  constexpr bool converting_constructor_third_condition_v = converting_constructor_third_condition<IsMove, F, S>::value;
  template<bool IsMove, typename F, typename S>
  constexpr bool is_tuple_types_convertible_fwb_v = is_tuple_types_convertible_fwb<IsMove, F, S>::value;
  template<typename T, typename Tuple>
  constexpr u_int32_t tuple_cnt_type_T_v = tuple_cnt_type_T<T, Tuple>::value;
};

template<typename Head, typename... Tail>
class tuple<Head, Tail...> {
private:
  Head head_;
  tuple<Tail...> tail_;

public:

  using HeadType = Head;

  explicit(!detail::is_copy_list_initializable_v<Head, Tail...>)
  tuple()
  requires(detail::is_types_default_constructible_v<Head, Tail...>)
  = default;

  explicit (!detail::is_types_copy_convertible_v<Head, Tail...>)
  tuple(const Head& head, const Tail&... tail)
  requires(
    detail::is_types_copy_constructible_v<Head, Tail...>
  )
    : head_(head),
      tail_(tail...)
  {}
  template<typename UHead, typename... UTail>
  explicit (!detail::is_tuple_types_convertible_v<tuple<Head, Tail...>, tuple<UHead, UTail...>>)
  tuple(UHead &&other_head, UTail&& ...other_tail)
  requires(
  detail::is_tuple_sizes_equal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::is_tuple_types_constructible_v<tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::is_tuple_sizes_equal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>>
  )
    : head_(std::forward<UHead>(other_head)),
      tail_(std::forward<UTail>(other_tail)...)
  {}

  template<typename UHead, typename... UTail>
  explicit(!detail::is_tuple_types_convertible_fwb_v<false, tuple<Head, Tail...>, tuple<UHead, UTail...>>)
  tuple(const tuple<UHead, UTail...>& other)
  requires(
  detail::is_tuple_sizes_equal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::constructible_tuple_v<false, tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::converting_constructor_third_condition_v<false, tuple<Head, Tail...>, tuple<UHead, UTail...>>
  )
    : head_(std::forward<decltype((other.head_))>(other.head_)),
      tail_(other.tail_)
  {}

  template<typename UHead, typename... UTail>
  explicit(!detail::is_tuple_types_convertible_fwb_v<true, tuple<Head, Tail...>, tuple<UHead, UTail...>>)
  tuple(tuple<UHead, UTail...>&& other)
  requires(
  detail::is_tuple_sizes_equal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::constructible_tuple_v<true, tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::converting_constructor_third_condition_v<true, tuple<Head, Tail...>, tuple<UHead, UTail...>>
  )
    : head_(std::move(other.head_)),
      tail_(std::move(other.tail_))
  {}

  template<typename T1, typename T2>
  tuple(const std::pair<T1, T2>& other):
    head_(other.first),
    tail_(other.second)
  {}

  template<typename T1, typename T2>
  tuple(std::pair<T1, T2>&& other):
    head_(std::move(other.first)),
    tail_(std::move(other.second))
  {}

  tuple(const tuple& other)
  requires(detail::is_types_copy_constructible_v<Head, Tail...>)
    : head_(other.head_),
      tail_(other.tail_)
  {
    static_assert(std::is_copy_constructible_v<decltype(head_)>);
  }
  tuple(const tuple& other)
  requires(!detail::is_types_copy_constructible_v<Head, Tail...>) = delete;

  tuple(tuple&& other)
  requires(detail::is_types_move_constructible_v<Head, Tail...>)
    : head_(std::move(other.head_)),
      tail_(std::move(other.tail_))
  {
    static_assert(std::is_move_constructible_v<decltype(head_)>);
  }
  tuple(tuple&& other)
  requires(!detail::is_types_move_constructible_v<Head, Tail...>) = delete;

  tuple& operator=(const tuple& other)
  requires(detail::is_types_copy_assignable_v<Head, Tail...>){
    head_ = other.head_;
    tail_ = other.tail_;
    return *this;
  }
  tuple& operator=(const tuple& other)
  requires(!detail::is_types_copy_assignable_v<Head, Tail...>) = delete;

  tuple& operator=(tuple&& other)
  requires(detail::is_types_move_assignable_v<Head, Tail...>){
    head_ = std::forward<Head>(get<0>(std::move(other)));
    tail_ = std::move(other.tail_);
    return *this;
  }
  tuple& operator=(tuple&& other)
  requires(!detail::is_types_move_assignable_v<Head, Tail...>) = delete;


  template<typename UHead, typename... UTail>
  tuple& operator=(const tuple<UHead, UTail...>& other)
  requires(
  detail::is_tuple_sizes_equal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::is_tuple_types_copy_assignable_universal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>>
  ) {
    head_ = other.head_;
    tail_ = other.tail_;
    return *this;
  }
  template<typename UHead, typename... UTail>
  tuple& operator=(tuple<UHead, UTail...>&& other)
  requires(
  detail::is_tuple_sizes_equal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>> &&
  detail::is_tuple_types_move_assignable_universal_v<tuple<Head, Tail...>, tuple<UHead, UTail...>>
  ) {
    head_ = std::move(other.head_);
    tail_ = std::move(other.tail_);
    return *this;
  }
  bool operator==(const tuple& other) const {
    if (head_ != other.head_) return false;
    if constexpr (sizeof...(Tail) > 0) {
      return (tail_ == other.tail_);
    } else {
      return true;
    }
  }

  template<size_t I, typename UHead, typename... UTail>
  friend decltype(auto) get_impl(const tuple<UHead, UTail...> &tuple);
  template<size_t I, typename UHead, typename... UTail>
  friend decltype(auto) get_impl(const tuple<UHead, UTail...> &&tuple);
  template<size_t I, typename UHead, typename... UTail>
  friend decltype(auto) get_impl(tuple<UHead, UTail...> &tuple);
  template<size_t I, typename UHead, typename... UTail>
  friend decltype(auto) get_impl(tuple<UHead, UTail...> &&tuple);
  template<size_t I, typename Tuple>
  friend decltype(auto) get(Tuple&& t);

  template<typename T, typename Tuple>
  friend decltype(auto) get(Tuple&& t);
  template<typename T, typename... UTypes>
  friend decltype(auto) get_template_impl(const tuple<UTypes...>& t);
  template<typename T, typename... UTypes>
  friend decltype(auto) get_template_impl(tuple<UTypes...>& t);
  template<typename T, typename... UTypes>
  friend decltype(auto) get_template_impl(const tuple<UTypes...>&& t);
  template<typename T, typename... UTypes>
  friend decltype(auto) get_template_impl(tuple<UTypes...>&& t);


  template<typename... UTypes>
  friend class tuple_traits;
  template<typename... UTypes>
  friend class tuple;

  template<typename FHead, typename... FTail, typename SHead, typename... STail>
  friend bool operator==(const tuple<FHead, FTail...>& first, const tuple<SHead, STail...>& second);
  template<typename FHead, typename... FTail, typename SHead, typename... STail>
  friend std::partial_ordering operator<=>(const tuple<FHead, FTail...>& first, const tuple<SHead, STail...>& second);
};

template<typename FHead, typename... FTail, typename SHead, typename... STail>
bool operator==(const tuple<FHead, FTail...>& first, const tuple<SHead, STail...>& second) {
  if constexpr(sizeof...(FTail) != sizeof...(STail)) {
    return false;
  } else {
    using firstHeadType = typename std::remove_cvref_t<decltype(first.head_)>;
    using secondHeadType = typename std::remove_cvref_t<decltype(second.head_)>;
    static_assert(std::equality_comparable_with<firstHeadType, secondHeadType>);
    if (first.head_ != second.head_) {
      return false;
    }
    if constexpr (sizeof...(FTail) >= 0) {
      return first.tail_ == second.tail_;
    } else {
      return true;
    }
  }
}

template<typename FHead, typename... FTail, typename SHead, typename... STail>
std::partial_ordering operator<=>(const tuple<FHead, FTail...>& first, const tuple<SHead, STail...>& second) {
  if constexpr (sizeof...(FTail) != sizeof...(STail)) {
    return std::partial_ordering::unordered;
  } else {
    using firstHeadType = typename std::remove_cvref_t<decltype(first.head_)>;
    using secondHeadType = typename std::remove_cvref_t<decltype(second.head_)>;
    static_assert(std::three_way_comparable_with<firstHeadType, secondHeadType>);
    std::partial_ordering head_comp = (first.head_ <=> second.head_);
    if (head_comp != std::partial_ordering::equivalent) {
      return head_comp;
    }
    if constexpr (sizeof...(FTail) > 0) {
      return first.tail_ <=> second.tail_;
    } else {
      return std::partial_ordering::equivalent;
    }
  }
}

template<typename... Types>
tuple(const Types& ... args) -> tuple<Types...>;

template<typename... Types>
tuple(Types&& ... args) -> tuple<Types...>;

template<typename T1, typename T2>
tuple(const std::pair<T1, T2>& pair) -> tuple<T1, T2>;

template<typename T1, typename T2>
tuple(std::pair<T1, T2>&& pair) -> tuple<T1, T2>;



template<size_t I, typename UHead, typename... UTail>
decltype(auto) get_impl(const tuple<UHead, UTail...> &tuple) {
  return get<I - 1>(tuple.tail_);
}
template<size_t I, typename UHead, typename... UTail>
decltype(auto) get_impl(tuple<UHead, UTail...> &&tuple) {
  return get<I - 1>(std::move(tuple.tail_));
}
template<size_t I, typename UHead, typename... UTail>
decltype(auto) get_impl(tuple<UHead, UTail...> &tuple) {
  return get<I - 1>(tuple.tail_);
}
template<size_t I, typename UHead, typename... UTail>
decltype(auto) get_impl(const tuple<UHead, UTail...> &&tuple) {
  return get<I - 1>(std::move(tuple.tail_));
}

template<size_t I, typename Tuple>
decltype(auto) get(Tuple&& tuple) {
  constexpr bool is_const = std::is_const_v<std::remove_reference_t<Tuple>>;
  constexpr bool is_lvalue = std::is_lvalue_reference_v<Tuple>;
  if constexpr (I == 0) {
    if constexpr (is_const && is_lvalue) {
      return std::forward<const decltype(tuple.head_)&>(tuple.head_);
    } else if constexpr (!is_const && is_lvalue){
      return std::forward<decltype(tuple.head_)&>(tuple.head_);
    } else if constexpr (is_const && !is_lvalue) {
      return std::forward<const decltype(tuple.head_)&&>(tuple.head_);
    } else {
      static_assert(!is_const && !is_lvalue);
      return std::forward<decltype(tuple.head_)&&>(tuple.head_);
    }
  } else {
    return get_impl<I>(std::forward<Tuple>(tuple));
  }
}
template<typename T, typename... Types>
decltype(auto) get_template_impl(const tuple<Types...>& tuple) {
  return get<T>(tuple.tail_);
}
template<typename T, typename... Types>
decltype(auto) get_template_impl(tuple<Types...>& tuple) {
  return get<T>(tuple.tail_);
}
template<typename T, typename... Types>
decltype(auto) get_template_impl(tuple<Types...>&& tuple) {
  return get<T>(std::move(tuple.tail_));
}
template<typename T, typename... Types>
decltype(auto) get_template_impl(const tuple<Types...>&& tuple) {
  return get<T>(std::move(tuple.tail_));
}

template<typename T, typename Tuple>
decltype(auto) get(Tuple&& tuple) {
  static_assert(detail::tuple_cnt_type_T_v<T, std::remove_reference_t<Tuple>> == 1);
  constexpr bool is_const = std::is_const_v<std::remove_reference_t<Tuple>>;
  constexpr bool is_lvalue = std::is_lvalue_reference_v<Tuple>;
  if constexpr (std::is_same_v<std::remove_reference_t<decltype(tuple.head_)>, T>) {
    if constexpr (is_const && is_lvalue) {
      return std::forward<const decltype(tuple.head_)&>(tuple.head_);
    } else if constexpr (!is_const && is_lvalue){
      return std::forward<decltype(tuple.head_)&>(tuple.head_);
    } else if constexpr (is_const && !is_lvalue) {
      return std::forward<const decltype(tuple.head_)&&>(tuple.head_);
    } else {
      static_assert(!is_const && !is_lvalue);
      return std::forward<decltype(tuple.head_)&&>(tuple.head_);
    }
  } else {
    return get_template_impl<T>(tuple);
  }
}

template<>
class tuple<> {};
bool operator==(const tuple<>&, const tuple<>&) { return true; }
bool operator!=(const tuple<>&, const tuple<>&) { return false; }
std::partial_ordering operator<=>(const tuple<>&, const tuple<>&) { return std::partial_ordering::equivalent; }

template<typename Tuple>
struct tuple_size {};

template<typename Head, typename... Tail>
struct tuple_size<tuple<Head, Tail...>> {
  static constexpr size_t value = 1 + tuple_size<tuple<Tail...>>::value;
};

template<>
struct tuple_size<tuple<>> {
  static constexpr size_t value = 0;
};


template<typename... Types>
tuple<std::decay_t<Types>...> makeTuple(Types&&... args) {
  return tuple<std::unwrap_ref_decay_t<Types>...>(std::forward<Types>(args)...);
}

template<typename... Types>
tuple<Types&...> tie(Types&... args) {
  return tuple<Types&...>(args...);
}

template<typename... Types>
tuple<Types&&...> forwardAsTuple(Types&&... args) {
  return tuple<Types&&...>(std::forward<Types>(args)...);
}

template<typename... Types>
struct CatTypes {};

template<typename... F, typename... S>
struct CatTypes<tuple<F...>, tuple<S...>> {
  using type = tuple<F..., S...>;
};

template<typename T1, typename T2>
struct CatTypes<T1, T2> {
  using type = typename CatTypes<std::remove_reference_t<T1>, std::remove_reference_t<T2>>::type;
};

template<typename T1, typename T2, typename... Other>
struct CatTypes<T1, T2, Other...> {
  using type = CatTypes<typename CatTypes<T1, T2>::type, Other...>::type;
};

template<typename F, typename S, size_t... FIdx, size_t... SIdx>
auto catTwoTuplesHelper(F&& first, S&& second, std::index_sequence<FIdx...>, std::index_sequence<SIdx...>) -> CatTypes<F, S>::type {
  return typename CatTypes<F, S>::type (get<FIdx>(std::forward<F>(first))... , get<SIdx>(std::forward<S>(second))...);
}

template<typename T1, typename T2>
auto catTwoTuples(T1&& first, T2&& second) -> CatTypes<T1, T2>::type {
  std::make_index_sequence<tuple_size<std::remove_reference_t<T1>>::value> first_idx;
  std::make_index_sequence<tuple_size<std::remove_reference_t<T2>>::value> second_idx;
  return catTwoTuplesHelper(std::forward<T1>(first), std::forward<T2>(second), first_idx, second_idx);
}

template<typename T1, typename T2, typename... Other>
auto tupleCat(T1&& fir_tuple, T2&& sec_tuple, Other&&... other_tuples) -> CatTypes<T1, T2, Other...>::type {
  typename CatTypes<T1, T2>::type merged = catTwoTuples(std::forward<T1>(fir_tuple), std::forward<T2>(sec_tuple));
  if constexpr (sizeof...(Other) > 0) {
    return tupleCat(std::move(merged), std::forward<Other>(other_tuples)...);
  } else {
    return merged;
  }
}
