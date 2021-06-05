#include <pybind11/functional.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h> /* Automatically converts STL containers to python objects */

#include "ecs/mutable_ecs.hpp"
#include "ecs/variant_utils.hpp"

namespace std {
template <> struct hash<pybind11::object> {
  std::size_t operator()(const pybind11::object &py_object) const {
    auto hash = pybind11::hash(py_object);
    return hash;
  }
};

} // namespace std

namespace ecs {
namespace mutable_ecs {

using TypeIndex = pybind11::object;
using ComponentType = pybind11::object;
using SystemType = pybind11::object;
using ActionType = pybind11::object;

struct GetPybindComponentType {
  auto operator()(ComponentType component) { return pybind11::type::of(component); }
};

void MutableEcsModule(pybind11::module &mutable_ecs) {

  pybind11::class_<Entity>(mutable_ecs, "Entity")
      .def(pybind11::init<UniqueId>(), pybind11::arg("unique_id"))
      .def_readonly("unique_id", &Entity::unique_id);

  pybind11::class_<EntityComponentDatabase<TypeIndex, ComponentType>>(mutable_ecs, "EntityComponentDatabase")
      .def(pybind11::init<>())
      .def("__len__", [](const EntityComponentDatabase<TypeIndex, ComponentType> &self) { return self.size(); });

  mutable_ecs.def("create_ecdb", &create_ecdb<TypeIndex, ComponentType>);
  mutable_ecs.def("add_entity", &add_entity<TypeIndex, ComponentType, GetPybindComponentType>, pybind11::arg("ecdb"),
                  pybind11::arg("components") = std::vector<ComponentType>{},
                  pybind11::return_value_policy::take_ownership);
  mutable_ecs.def("remove_entity", &remove_entity<TypeIndex, ComponentType>, pybind11::arg("ecdb"),
                  pybind11::arg("entity"));
  mutable_ecs.def("add_component", &add_component<TypeIndex, ComponentType, GetPybindComponentType>,
                  pybind11::arg("ecdb"), pybind11::arg("entity"), pybind11::arg("component"));
  mutable_ecs.def("remove_component", &remove_component<TypeIndex, ComponentType>, pybind11::arg("ecdb"),
                  pybind11::arg("entity"), pybind11::arg("component_type"));
  mutable_ecs.def("get_component", &get_component<TypeIndex, ComponentType>, pybind11::arg("ecdb"),
                  pybind11::arg("entity"), pybind11::arg("component_type"));

  mutable_ecs.def("query",
                  pybind11::overload_cast<const EntityComponentDatabase<TypeIndex, ComponentType> &,
                                          const std::vector<TypeIndex> &>(&query<TypeIndex, ComponentType>),
                  pybind11::arg("ecdb"), pybind11::arg("component_types") = std::vector<TypeIndex>{});

  // Systems
  pybind11::class_<Systems<SystemType>>(mutable_ecs, "Systems").def(pybind11::init<>());

  mutable_ecs.def("create_systems", &create_systems<SystemType>);
  mutable_ecs.def("add_system", &add_system<SystemType>, pybind11::arg("systems"), pybind11::arg("system"),
                  pybind11::arg("priority"));
  mutable_ecs.def("process_systems", &process_systems<TypeIndex, ComponentType, SystemType, ActionType>,
                  pybind11::arg("ecdb"), pybind11::arg("systems"), pybind11::arg("process_system"),
                  pybind11::arg("process_action"));
}

} // namespace mutable_ecs
} // namespace ecs
