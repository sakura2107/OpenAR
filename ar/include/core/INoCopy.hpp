#pragma once

namespace ar::core {

// Mix-in base that disables copy and move. Protected dtor: instances are meant
// to be derived, never deleted through this type.
class INoCopy {
  protected:
    INoCopy() = default;
    ~INoCopy() = default;

    INoCopy(const INoCopy &) = delete;
    INoCopy &operator=(const INoCopy &) = delete;
    INoCopy(INoCopy &&) = delete;
    INoCopy &operator=(INoCopy &&) = delete;
};

} // namespace ar::core
