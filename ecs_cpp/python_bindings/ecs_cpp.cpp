#include <pybind11/pybind11.h>

namespace ecs {
namespace mutable_ecs {
void MutableEcsModule(pybind11::module &mutable_ecs);
} // namespace mutable_ecs
} // namespace ecs

PYBIND11_MODULE(ecs_cpp, ecs_cpp) {
  auto mutable_ecs = ecs_cpp.def_submodule("mutable_ecs");
  ecs::mutable_ecs::MutableEcsModule(mutable_ecs);
}
