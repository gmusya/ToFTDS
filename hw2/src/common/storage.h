#pragma once

#include <map>
#include <optional>

namespace hw2 {

template <typename Key, typename Value> class Storage {
public:
  std::optional<Value> Get(const Key &key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
      return std::nullopt;
    }
    return *it;
  }

  void Set(const Key &key, Value new_value) {
    values_[key] = std::move(new_value);
  }

private:
  std::map<Key, Value> values_;
};

} // namespace hw2
