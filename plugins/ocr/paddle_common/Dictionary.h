#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "log/Logger.h"

namespace ar::paddle {

// Character dictionary for the recognition model: line N of the file is the
// character with class id N+1 (id 0 is the CTC blank). Both directions are
// indexed so lookups stay O(1).
class Dictionary {
  public:
    Dictionary() = default;
    explicit Dictionary(ar::log::ILogger *logger) : logger_(logger) {}

    bool load(const std::string &path);

    // Character for a class id already offset by -1 (CTC blank removed);
    // Out-of-range ids are configuration errors, never an implicit space.
    std::string get_char(size_t index) const {
        return dictionary_.at(index);
    }

    size_t size() const { return dictionary_.size(); }

  private:
    ar::log::ILogger *logger_ = nullptr;
    std::vector<std::string> dictionary_;
    std::unordered_map<std::string, size_t> char_to_index_;
};

} // namespace ar::paddle
