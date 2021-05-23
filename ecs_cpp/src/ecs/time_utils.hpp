#include <chrono>

namespace ecs {
namespace time_utils {
std::chrono::time_point<std::chrono::steady_clock> now() { return std::chrono::steady_clock::now(); }

template <typename DurationType>
std::size_t duration(std::chrono::time_point<std::chrono::steady_clock> start,
                     std::chrono::time_point<std::chrono::steady_clock> end) {
  return std::chrono::duration_cast<DurationType>(end - start).count();
}

double compute_frames_per_second(std::size_t time_in_nanoseconds) {
  return 1.0f / static_cast<double>(time_in_nanoseconds) * 1e9;
}
} // namespace time_utils
} // namespace ecs
