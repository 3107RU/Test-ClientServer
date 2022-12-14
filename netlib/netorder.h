#include <endian.h>  // __BYTE_ORDER __LITTLE_ENDIAN
#include <algorithm> // std::reverse()

namespace netorder
{

  template <typename T>
  T convert(T value) noexcept
  {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    char *ptr = reinterpret_cast<char *>(&value);
    std::reverse(ptr, ptr + sizeof(T));
#endif
    return value;
  }

  template <typename T>
  void convert(T first, T last) noexcept
  {
#if __BYTE_ORDER == __LITTLE_ENDIAN
    for (auto it = first; it != last; it++)
        *it = convert(*it);
#endif
  }

}