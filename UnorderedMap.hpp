#pragma once
#include "List.hpp"
#include <vector>
#include <cassert>

// List is bidirectional so it is map in 2 sides

template<typename Key, typename Val, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>, typename Alloc = std::allocator<std::pair<const Key, Val>>>
class UnorderedMap {
public:
  using PairType = std::pair<const Key, Val>;
  struct ListNodeType {
    PairType data;
    std::size_t hash;
  };

  using ListType = List<ListNodeType, Alloc>;
  using ListIteratorType = ListType::iterator;
  using ListConstIteratorType = ListType::const_iterator;

  struct InfoNode {
    ListIteratorType it;
    uint32_t cnt;
  };
  using InfoNodeAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<InfoNode>;
private:
  std::vector<InfoNode, InfoNodeAlloc> hash_to_node_in_list_;
  // iterator on list and cnt of current block
  ListType nodes_;

  [[no_unique_address]]Alloc alloc_;
  [[no_unique_address]]Hash hasher_;
  [[no_unique_address]]Equal key_equal_;
  uint32_t table_size_;
  uint32_t element_cnt_;
  double max_load_factor_ = 0.5;
  
  template<bool is_const>
  class Iterator {
  private:
    using ListIterator = std::conditional_t<is_const, ListConstIteratorType, ListIteratorType>;
    ListIterator it_;
  public:
    using value_type = std::conditional_t<is_const, const PairType, PairType>;
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;

    using reference = std::conditional_t<is_const, const PairType&, PairType&>;
    using pointer = std::conditional_t<is_const, const PairType*, PairType*>;
    Iterator(const Iterator& other): it_(other.it_) {}
    Iterator(const Iterator<false>& other) requires(is_const) : it_(other.it_) {}

    Iterator(ListIterator cur): it_(cur) {}
    Iterator(const ListIteratorType& other) requires(is_const) : it_(other) {}

    Iterator& operator=(Iterator other) { it_ = other.it_; return *this; }

    Iterator& operator++() { ++it_; return *this; }
    Iterator operator++(int) { Iterator cur = *this; ++*this; return cur;}

    Iterator& operator--() { --it_; return *this; }
    Iterator operator--(int) { Iterator cur = *this; --*this; return cur;}

    template<bool other_const>
    bool operator==(const Iterator<other_const>& other) const { return it_ == other.it_; }
    template<bool other_const>
    bool operator!=(const Iterator<other_const>& other) const { return it_ != other.it_; }

    reference operator*() const;
    pointer operator->() const;

    ListIterator ptr() { return it_; };
    ~Iterator() = default;
  };

  template<typename K>
  Val& GetOrAdd(K&& key);

  void Rehash(uint32_t new_table_size);
  void CheckRehash();

public:
  UnorderedMap();
  UnorderedMap(const Alloc& alloc);

  UnorderedMap(const UnorderedMap& other);
  UnorderedMap(UnorderedMap&& other);

  template<typename OtherAlloc>
  UnorderedMap(const UnorderedMap<Key, Val, Hash, Equal, OtherAlloc>& other);
  template<typename OtherAlloc>
  UnorderedMap(UnorderedMap<Key, Val, Hash, Equal, OtherAlloc>&& other);

  UnorderedMap& operator=(const UnorderedMap& other);
  UnorderedMap& operator=(UnorderedMap&& other);

  Val& operator[](const Key& key);
  Val& operator[](Key&& key);

  Val& at(const Key& key);
  const Val& at(const Key& key) const;

  uint32_t size() const;

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  iterator begin();
  iterator end();
  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

  std::pair<iterator, bool> insert(const PairType &cur);
  std::pair<iterator, bool> insert(PairType&& cur);
  template<typename InputIt>
  void insert(InputIt start, InputIt end);

  void erase(iterator cur);
  template<typename InputIt>
  void erase(InputIt start, InputIt end);

  void reserve(uint32_t sz);

  double load_factor() const;
  double max_load_factor() const;
  void max_load_factor(double f);


  template<typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args);

  const_iterator find(const Key &key) const;
  iterator find(const Key &key);
  void swap(UnorderedMap& other);

  ~UnorderedMap() = default;
private:
  template<typename F>
  std::pair<iterator, bool> insert_helper(F&& cur);
  static constexpr int32_t initial_size_ = 5;
};

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::UnorderedMap(const Alloc &alloc): alloc_(alloc), nodes_(alloc), hash_to_node_in_list_(alloc) {
  hash_to_node_in_list_.resize(initial_size_);
  table_size_ = hash_to_node_in_list_.size();
  element_cnt_ = 0;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::CheckRehash() {
  if (static_cast<double>(element_cnt_) / table_size_ >= max_load_factor_) {
    Rehash(table_size_ * 2);
  }
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::swap(UnorderedMap &other) {
  if (this == &other) return;
  using AllocTraits = std::allocator_traits<Alloc>;
  constexpr bool propagate = AllocTraits::propagate_on_container_swap::value;
  constexpr bool always_equal = AllocTraits::is_always_equal::value;
  if (propagate || always_equal || (nodes_.get_allocator() == other.nodes_.get_allocator() &&
                                    hash_to_node_in_list_.get_allocator() == other.hash_to_node_in_list_.get_allocator())) {
    nodes_.swap(other.nodes_);
    std::swap(hasher_, other.hasher_);
    std::swap(key_equal_, other.key_equal_);
    std::swap(table_size_, other.table_size_);
    std::swap(element_cnt_, other.element_cnt_);
    std::swap(max_load_factor_, other.max_load_factor_);
    hash_to_node_in_list_.swap(other.hash_to_node_in_list_);
    return;
  }

  UnorderedMap tmp = std::move(*this);
  *this = std::move(other);
  other = std::move(tmp);
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename... Args>
std::pair<typename UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator, bool> UnorderedMap<Key, Val, Hash, Equal, Alloc>::emplace(Args&&... args) {
  PairType cur_node{std::forward<Args>(args)...};
  iterator find_key = find(cur_node.first);
  if (find_key != end()) {
    return {find_key, false};
  }
  return insert_helper(std::move(cur_node));
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::max_load_factor(double f) {
  max_load_factor_ = f;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
double UnorderedMap<Key, Val, Hash, Equal, Alloc>::max_load_factor() const {
  return max_load_factor_;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
double UnorderedMap<Key, Val, Hash, Equal, Alloc>::load_factor() const {
  return static_cast<double>(element_cnt_) / table_size_;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::reserve(uint32_t sz) {
  uint32_t need_size = static_cast<double>(sz) / max_load_factor_ + 2;
  Rehash(need_size);
}

template < typename Key, typename Val, typename Hash, typename Equal, typename Alloc > UnorderedMap <Key, Val, Hash, Equal, Alloc>::const_iterator UnorderedMap <Key, Val, Hash, Equal, Alloc>::find(const Key & key) const {
  uint32_t iterator_idx = hasher_(key) % table_size_;
  ListIteratorType it = hash_to_node_in_list_[iterator_idx].it;
  uint32_t cur_cnt = hash_to_node_in_list_[iterator_idx].cnt;
  if (cur_cnt == 0) {
    return end();
  }
  for (int i = 0; i < cur_cnt; ++i, ++it) {
    if (key_equal_(it -> data.first, key)) {
      return const_iterator(it);
    }
  }
  return end();
}
template <typename Key, typename Val, typename Hash, typename Equal, typename Alloc > UnorderedMap <Key, Val, Hash, Equal, Alloc>::iterator UnorderedMap <Key, Val, Hash, Equal, Alloc>::find(const Key & key) {
  uint32_t iterator_idx = hasher_(key) % table_size_;
  ListIteratorType it = hash_to_node_in_list_[iterator_idx].it;
  uint32_t cur_cnt = hash_to_node_in_list_[iterator_idx].cnt;
  if (cur_cnt == 0) {
    return end();
  }
  for (int i = 0; i < cur_cnt; ++i, ++it) {
    if (key_equal_(it -> data.first, key)) {
      return iterator(it);
    }
  }
  return end();
}
template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename InputIt>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::erase(InputIt start, InputIt end) {
  static_assert(std::is_same_v<iterator, InputIt>, "Iterator must point to PairType");
  for (InputIt cur = start; cur != end;) {
    InputIt next = std::next(cur);
    erase(cur);
    cur = next;
  }
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::erase(UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator cur) {
  ListIteratorType it_list = cur.ptr();
  ListIteratorType next_it = std::next(it_list);
  size_t hash = it_list->hash;

  typename ListType::BaseNodeType* cur_node = it_list.ptr();
  typename ListType::BaseNodeType* next = it_list.ptr()->next;
  typename ListType::BaseNodeType* prev = it_list.ptr()->prev;
  next->prev = prev;
  prev->next = next;

  typename ListType::DefaultNodeAlloc node_alloc(nodes_.alloc_);
  std::allocator_traits<typename ListType::DefaultNodeAlloc>::destroy(node_alloc, static_cast<ListType::DefaultNodeType*>(cur_node));
  std::allocator_traits<typename ListType::DefaultNodeAlloc>::deallocate(node_alloc, static_cast<ListType::DefaultNodeType*>(cur_node), 1);

  --element_cnt_;
  --hash_to_node_in_list_[hash % table_size_].cnt;
  if (hash_to_node_in_list_[hash % table_size_].cnt == 0) {
    hash_to_node_in_list_[hash % table_size_].it = nullptr;
  } else if (hash_to_node_in_list_[hash % table_size_].it == it_list) {
    hash_to_node_in_list_[hash % table_size_].it = next_it;
  }

}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename InputIt>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::insert(InputIt start, InputIt end) {
  static_assert(std::is_same_v<PairType, typename InputIt::value_type>, "Iterator must point to NodeType");
  for (InputIt cur = start; cur != end;){
    InputIt next = std::next(cur);
    if constexpr (std::is_move_constructible_v<typename InputIt::value_type>) {
      insert(std::move(*cur));
    } else {
      insert(*cur);
    }
    cur = next;
  }
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename F>
std::pair<typename UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator, bool> UnorderedMap<Key, Val, Hash, Equal, Alloc>::insert_helper(F&& cur) {
  if (static_cast<double>(element_cnt_ + 1) > max_load_factor_ * table_size_) {
    Rehash(table_size_ * 2);
  }
  std::size_t hash = hasher_(cur.first);
  uint32_t iterator_idx = hash % table_size_;

  ListIteratorType it = hash_to_node_in_list_[iterator_idx].it;
  uint32_t cur_cnt = hash_to_node_in_list_[iterator_idx].cnt;
  if (cur_cnt == 0) {
    nodes_.emplace_front(std::forward<F>(cur), hash);
    hash_to_node_in_list_[iterator_idx] = {nodes_.begin(), 1};
    ++element_cnt_;
    return {iterator(nodes_.begin()), true};
  }

  ListIteratorType next = std::next(it);
  typename ListType::DefaultNodeAlloc node_alloc(nodes_.alloc_);
  DefaultNode<ListNodeType>* new_node = std::allocator_traits<typename ListType::DefaultNodeAlloc>::allocate(node_alloc, 1);
  try {
    std::allocator_traits<typename ListType::DefaultNodeAlloc>::construct(node_alloc, new_node, it.ptr(), next.ptr(), std::forward<F>(cur), hash);
  } catch (...) {
    std::allocator_traits<typename ListType::DefaultNodeAlloc>::deallocate(node_alloc, new_node, 1);
    throw;
  }

  it.ptr()->next = static_cast<ListType::BaseNodeType*>(new_node);
  next.ptr()->prev = static_cast<ListType::BaseNodeType*>(new_node);

  ListIteratorType ans = std::next(it);
  ++element_cnt_;
  ++hash_to_node_in_list_[iterator_idx].cnt;

  return {ans, true};
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
std::pair<typename UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator, bool> UnorderedMap<Key, Val, Hash, Equal, Alloc>::insert(UnorderedMap::PairType &&cur) {
  iterator find_key = find(cur.first);
  if (find_key != end()) {
    return {find_key, false};
  }
  return insert_helper(std::move(cur));
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
std::pair<typename UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator, bool> UnorderedMap<Key, Val, Hash, Equal, Alloc>::insert(const UnorderedMap::PairType &cur) {
  iterator find_key = find(cur.first);
  if (find_key != end()) {
    return {find_key, false};
  }
  return insert_helper(cur);
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator UnorderedMap<Key, Val, Hash, Equal, Alloc>::begin() {
  return UnorderedMap::iterator(nodes_.begin());
}
template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::const_iterator UnorderedMap<Key, Val, Hash, Equal, Alloc>::begin() const {
  return UnorderedMap::const_iterator(nodes_.cbegin());
}
template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::const_iterator UnorderedMap<Key, Val, Hash, Equal, Alloc>::cbegin() const {
  return UnorderedMap::const_iterator(nodes_.cbegin());
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::iterator UnorderedMap<Key, Val, Hash, Equal, Alloc>::end() {
  return UnorderedMap::iterator(nodes_.end());
}
template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::const_iterator UnorderedMap<Key, Val, Hash, Equal, Alloc>::end() const {
  return UnorderedMap::const_iterator(nodes_.cend());
}
template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::const_iterator UnorderedMap<Key, Val, Hash, Equal, Alloc>::cend() const {
  return UnorderedMap::const_iterator(nodes_.cend());
}


template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<bool is_const>
typename UnorderedMap<Key, Val, Hash, Equal, Alloc>::template Iterator<is_const>::reference UnorderedMap<Key, Val, Hash, Equal, Alloc>::Iterator<is_const>::operator*() const {
  return it_->data;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<bool is_const>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::Iterator<is_const>::pointer UnorderedMap<Key, Val, Hash, Equal, Alloc>::Iterator<is_const>::operator->() const{
  return &(it_->data);
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
void UnorderedMap<Key, Val, Hash, Equal, Alloc>::Rehash(uint32_t new_table_size) {
  std::vector<InfoNode, InfoNodeAlloc> new_hash_2_iterator(new_table_size, {nullptr, 0});
  if (element_cnt_ > 0) {
    ListType new_nodes;
    for (typename ListType::iterator it = nodes_.begin(); it != nodes_.end();) {
      typename ListType::iterator next_c = std::next(it);

      uint32_t new_idx = it->hash % new_table_size;
      if (new_hash_2_iterator[new_idx].cnt == 0) {
        typename ListType::BaseNodeType *cur_node = it.ptr();
        typename ListType::BaseNodeType *after_cur_node = new_nodes.fake_node_.next;

        new_nodes.fake_node_.next = cur_node;
        cur_node->next = after_cur_node;
        after_cur_node->prev = cur_node;
        cur_node->prev = &new_nodes.fake_node_;

        new_hash_2_iterator[new_idx] = {new_nodes.begin(), 1};
      } else {
        typename ListType::BaseNodeType *cur_node = it.ptr();
        typename ListType::BaseNodeType *after_cur_node = std::next(new_hash_2_iterator[new_idx].it).ptr();
        typename ListType::BaseNodeType *before_cur_node = new_hash_2_iterator[new_idx].it.ptr();

        cur_node->next = after_cur_node;
        cur_node->prev = before_cur_node;
        after_cur_node->prev = cur_node;
        before_cur_node->next = cur_node;

        ++new_hash_2_iterator[new_idx].cnt;
      }
      it = next_c;
    }

    //do not call destructor and deallocation of nodes_
    nodes_.fake_node_.next = new_nodes.fake_node_.next;
    nodes_.fake_node_.prev = new_nodes.fake_node_.prev;
    nodes_.fake_node_.next->prev = &nodes_.fake_node_;
    nodes_.fake_node_.prev->next = &nodes_.fake_node_;

    new_nodes.fake_node_.next = new_nodes.fake_node_.prev = &new_nodes.fake_node_;
    new_nodes.size_ = 0;
  }
  table_size_ = new_table_size;
  hash_to_node_in_list_ = std::move(new_hash_2_iterator);
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
uint32_t UnorderedMap<Key, Val, Hash, Equal, Alloc>::size() const {
  return element_cnt_;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::UnorderedMap() {
  hash_to_node_in_list_.resize(initial_size_);
  table_size_ = hash_to_node_in_list_.size();
  element_cnt_ = 0;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::UnorderedMap(const UnorderedMap &other):
  hash_to_node_in_list_(other.hash_to_node_in_list_),
  nodes_(other.nodes_),
  hasher_(other.hasher_),
  key_equal_(other.key_equal_),
  table_size_(other.table_size_),
  element_cnt_(other.element_cnt_),
  max_load_factor_(other.max_load_factor_)
{}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename OtherAlloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::UnorderedMap(const UnorderedMap<Key, Val, Hash, Equal, OtherAlloc> &other):
  hash_to_node_in_list_(other.hash_to_node_in_list_),
  nodes_(other.nodes_),
  hasher_(other.hasher_),
  key_equal_(other.key_equal_),
  table_size_(other.table_size_),
  element_cnt_(other.element_cnt_),
  max_load_factor_(other.max_load_factor_)
{}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename OtherAlloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::UnorderedMap(UnorderedMap<Key, Val, Hash, Equal, OtherAlloc> &&other):
  hash_to_node_in_list_(std::move(other.hash_to_node_in_list_)),
  nodes_(std::move(other.nodes_)),
  hasher_(std::move(other.hasher_)),
  key_equal_(std::move(other.key_equal_)),
  table_size_(other.table_size_),
  element_cnt_(other.element_cnt_),
  max_load_factor_(other.max_load_factor_)
{}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc>::UnorderedMap(UnorderedMap &&other):
  hash_to_node_in_list_(std::move(other.hash_to_node_in_list_)),
  nodes_(std::move(other.nodes_)),
  hasher_(std::move(other.hasher_)),
  key_equal_(std::move(other.key_equal_)),
  table_size_(other.table_size_),
  element_cnt_(other.element_cnt_),
  max_load_factor_(other.max_load_factor_)
{}


template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc> &UnorderedMap<Key, Val, Hash, Equal, Alloc>::operator=(const UnorderedMap &other) {
  hash_to_node_in_list_ = other.hash_to_node_in_list_;
  nodes_ = other.nodes_;
  hasher_ = other.hasher_;
  key_equal_ = other.key_equal_;
  table_size_ = other.table_size_;
  element_cnt_ = other.element_cnt_;
  max_load_factor_ = other.max_load_factor_;
  return *this;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
UnorderedMap<Key, Val, Hash, Equal, Alloc> &UnorderedMap<Key, Val, Hash, Equal, Alloc>::operator=(UnorderedMap &&other) {
  hash_to_node_in_list_ = std::move(other.hash_to_node_in_list_);
  nodes_ = std::move(other.nodes_);
  hasher_ = std::move(other.hasher_);
  key_equal_ = std::move(other.key_equal_);
  table_size_ = other.table_size_;
  element_cnt_ = other.element_cnt_;
  max_load_factor_ = other.max_load_factor_;
  return *this;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
Val &UnorderedMap<Key, Val, Hash, Equal, Alloc>::at(const Key &key) {
  iterator it = find(key);
  if (it == end()) {
    throw std::runtime_error("AT ERROR");
  }
  return it->second;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
const Val &UnorderedMap<Key, Val, Hash, Equal, Alloc>::at(const Key &key) const {
  const_iterator it = find(key);
  if (it == end()) {
    throw std::runtime_error("AT ERROR");
  }
  return it->second;
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
Val &UnorderedMap<Key, Val, Hash, Equal, Alloc>::operator[](const Key& key) {
  return GetOrAdd(key);
}
template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
Val &UnorderedMap<Key, Val, Hash, Equal, Alloc>::operator[](Key&& key) {
  return GetOrAdd(std::move(key));
}

template<typename Key, typename Val, typename Hash, typename Equal, typename Alloc>
template<typename K>
Val &UnorderedMap<Key, Val, Hash, Equal, Alloc>::GetOrAdd(K &&key) {
  iterator cur_it = find(key);
  if (cur_it != end()) {
    return cur_it->second;
  }
  PairType cur_val(std::forward<K>(key), Val{});
  auto ans = insert_helper(std::move(cur_val));
  cur_it = ans.first;
  assert(ans.second == true);
  return cur_it->second;
}
