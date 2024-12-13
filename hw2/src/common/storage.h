#pragma once

#include <map>
#include <optional>
#include <sstream>

namespace hw2 {

template <typename Key, typename Value> class Storage {
public:
  std::optional<Value> Get(const Key &key) const {
    auto it = values_.find(key);
    if (it == values_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  void Set(const Key &key, Value new_value) {
    values_[key] = std::move(new_value);
  }

  std::string ToString() {
    std::stringstream ss;
    ss << "{";
    for (const auto &[key, value] : values_) {
      ss << "\n";
      ss << "  " << key << ": " << value;
    }
    ss << "\n}";
    return ss.str();
  }

private:
  std::map<Key, Value> values_;
};

} // namespace hw2
