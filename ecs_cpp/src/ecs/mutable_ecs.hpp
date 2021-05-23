#include <optional>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "ecs/time_utils.hpp"
#include "ecs/type_utils.hpp"

namespace ecs {
namespace mutable_ecs {

using UniqueId = int;

struct Entity {
public:
  UniqueId unique_id;

  explicit Entity() {}
  explicit Entity(UniqueId unique_id) { this->unique_id = unique_id; }
};

bool operator<(const Entity &entity_a, const Entity &entity_b) { return entity_a.unique_id < entity_b.unique_id; }
bool operator==(const Entity &entity_a, const Entity &entity_b) { return entity_a.unique_id == entity_b.unique_id; }

template <typename ComponentTemplate, int ArraySize> using ArrayOfComponents = std::array<ComponentTemplate, ArraySize>;
template <typename ComponentTemplate> using ListOfComponents = std::vector<ComponentTemplate>;

template <typename TypeIndexTemplate, typename ComponentTemplate>
using MapFromComponentTypeToComponent = std::unordered_map<TypeIndexTemplate, ComponentTemplate>;

template <typename TypeIndexTemplate, typename ComponentTemplate>
using MapFromEntityToMapFromComponentTypeToComponent =
    std::unordered_map<Entity, MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate>>;

template <typename TypeIndexTemplate, typename ComponentTemplate> struct EntityComponentDatabase {
public:
  UniqueId _last_unique_id;
  MapFromEntityToMapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> _entities;

  explicit EntityComponentDatabase() {
    this->_last_unique_id = 0;
    this->_entities = {};
  }

#ifndef ECDB_PYTHON_WRAPPER
  // TODO: enable in all cases
  EntityComponentDatabase(EntityComponentDatabase &&) = default;
  EntityComponentDatabase &operator=(EntityComponentDatabase &&) = default;
  EntityComponentDatabase(const EntityComponentDatabase &) = delete;
  EntityComponentDatabase &operator=(const EntityComponentDatabase &) = delete;
  virtual ~EntityComponentDatabase() {}
#endif

  std::size_t size() { return this->_entities.size(); }
};

template <typename TypeIndexTemplate, typename ComponentTemplate>
EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> create_ecdb() {
  return EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate>();
}

template <typename ComponentTemplate> struct GetComponentType {
  auto operator()(ComponentTemplate component) { return type_utils::get_variant_type(component); }
};

template <typename TypeIndexTemplate, typename ComponentTemplate,
          typename GetComponentTypeFunction = GetComponentType<ComponentTemplate>>
EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate>
add_component(EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb, const Entity &entity,
              const ComponentTemplate &component) {

  auto component_type = GetComponentTypeFunction()(component);

  auto &entities = ecdb._entities;
  auto &entity_components = entities[entity];

  entity_components[component_type] = component;

  return std::move(ecdb);
}

template <typename TypeIndexTemplate, typename ComponentTemplate,
          typename GetComponentTypeFunction = GetComponentType<ComponentTemplate>>
std::tuple<EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate>, Entity>
add_entity(EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb,
           const std::vector<ComponentTemplate> &components = {}) {
  auto unique_id = ecdb._last_unique_id;
  ecdb._last_unique_id += 1;

  auto entity = Entity(unique_id);
  ecdb._entities[entity] = {};

  for (auto &&component : components) {
    ecdb = add_component<TypeIndexTemplate, ComponentTemplate, GetComponentTypeFunction>(ecdb, entity, component);
  }

  return std::make_tuple(std::move(ecdb), entity);
}

template <typename TypeIndexTemplate, typename ComponentTemplate>
EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate>
remove_entity(EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb, const Entity &entity) {
  auto num_erased_keys = ecdb._entities.erase(entity);
  if (num_erased_keys == 0) {
    throw std::runtime_error("Entity is not in EntityComponentDatabase");
  }
  return std::move(ecdb);
}

template <typename TypeIndexTemplate, typename ComponentTemplate>
EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate>
remove_component(EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb, const Entity &entity,
                 TypeIndexTemplate component_type) {
  auto &entities = ecdb._entities;
  auto &entity_components = entities[entity];
  entity_components.erase(component_type);

  return std::move(ecdb);
}

template <typename TypeIndexTemplate, typename ComponentTemplate>
const ComponentTemplate &get_component(EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb,
                                       const Entity &entity, TypeIndexTemplate component_type) {
  auto &entities = ecdb._entities;
  auto &entity_components = entities.at(entity);
  auto &component = entity_components.at(component_type);
  return component;
}

template <typename TypeIndexTemplate, typename ComponentTemplate>
bool DefaultFilterFunction(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &) {
  return true;
}

template <typename TypeIndexTemplate, typename ComponentTemplate>
std::vector<std::tuple<Entity, ListOfComponents<ComponentTemplate>>> query(
    const EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb,
    const std::vector<TypeIndexTemplate> &component_types = {},
    // https://stackoverflow.com/questions/59356874/candidate-template-ignored-could-not-match-for-template-function-argument
    std::optional<typename type_utils::type_identity<
        std::function<bool(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &)>>::type>
        filter_function = DefaultFilterFunction<TypeIndexTemplate, ComponentTemplate>) {
  std::vector<std::tuple<Entity, ListOfComponents<ComponentTemplate>>> queried_entities;
  for (auto &&[entity, components] : ecdb._entities) {

    if (filter_function.has_value() and not(*filter_function)(components)) {
      continue;
    }

    ListOfComponents<ComponentTemplate> requested_components;
    if (component_types.size() == 0) {
      for (auto &&[component_type, component] : components) {
        requested_components.push_back(component);
      }
    } else {
      for (auto &component_type : component_types) {
        if (components.count(component_type) == 0) {
          continue;
        }
        requested_components.push_back(components.at(component_type));
      }

      bool skip_entity = requested_components.size() < component_types.size();
      if (skip_entity) {
        continue;
      }
    }

    queried_entities.push_back(std::make_pair(entity, requested_components));
  }
  return queried_entities;
}

template <typename TypeIndexTemplate, typename ComponentTemplate, int ArraySize, typename Arg>
bool IsComponentMissing(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &components) {
  return components.count(typeid(Arg)) == 0;
}

template <typename TypeIndexTemplate, typename ComponentTemplate, int ArraySize, typename Arg, typename... Args,
          typename std::enable_if<0 != sizeof...(Args), int>::type = 0>
bool IsComponentMissing(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &components) {
  auto skip_entity = IsComponentMissing<TypeIndexTemplate, ComponentTemplate, ArraySize, Arg>(components);
  skip_entity |= IsComponentMissing<TypeIndexTemplate, ComponentTemplate, ArraySize, Args...>(components);
  return skip_entity;
}

template <typename TypeIndexTemplate, typename ComponentTemplate, int ArraySize, typename Arg>
void GetRequestedComonents(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &components,
                           ArrayOfComponents<ComponentTemplate, ArraySize> &requested_components, int index) {
  requested_components[index] = components.at(typeid(Arg));
}

template <typename TypeIndexTemplate, typename ComponentTemplate, int ArraySize, typename Arg, typename... Args,
          typename std::enable_if<0 != sizeof...(Args), int>::type = 0>
void GetRequestedComonents(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &components,
                           ArrayOfComponents<ComponentTemplate, ArraySize> &requested_components, int index) {
  GetRequestedComonents<TypeIndexTemplate, ComponentTemplate, ArraySize, Arg>(components, requested_components,
                                                                              index++);
  GetRequestedComonents<TypeIndexTemplate, ComponentTemplate, ArraySize, Args...>(components, requested_components,
                                                                                  index);
}

template <typename TypeIndexTemplate, typename ComponentTemplate> struct OptionalQueryArguments {
  std::size_t num_entities_to_reserve = 128;
  // https://stackoverflow.com/questions/59356874/candidate-template-ignored-could-not-match-for-template-function-argument)
  std::optional<typename type_utils::type_identity<
      std::function<bool(const MapFromComponentTypeToComponent<TypeIndexTemplate, ComponentTemplate> &)>>::type>
      filter_function = DefaultFilterFunction<TypeIndexTemplate, ComponentTemplate>;
};

template <typename... Args, typename TypeIndexTemplate, typename ComponentTemplate>
std::vector<std::tuple<Entity, ArrayOfComponents<ComponentTemplate, sizeof...(Args)>>>
query(const EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb,
      const OptionalQueryArguments<TypeIndexTemplate, ComponentTemplate> &optional_query_arguments =
          OptionalQueryArguments<TypeIndexTemplate, ComponentTemplate>{}) {
  std::vector<std::tuple<Entity, ArrayOfComponents<ComponentTemplate, sizeof...(Args)>>> queried_entities;
  queried_entities.reserve(optional_query_arguments.num_entities_to_reserve);

  for (auto const &[entity, components] : ecdb._entities) {
    bool skip_entity = optional_query_arguments.filter_function.has_value() and
                       not(*optional_query_arguments.filter_function)(components);
    skip_entity |= IsComponentMissing<TypeIndexTemplate, ComponentTemplate, sizeof...(Args), Args...>(components);
    if (skip_entity) {
      continue;
    }

    ArrayOfComponents<ComponentTemplate, sizeof...(Args)> requested_components;
    GetRequestedComonents<TypeIndexTemplate, ComponentTemplate, sizeof...(Args), Args...>(components,
                                                                                          requested_components, 0);

    queried_entities.push_back(std::make_tuple(std::move(entity), std::move(requested_components)));
  }
  return queried_entities;
}

// Systems
using SystemPriority = int;

template <typename SystemTemplate> using ListOfSystems = std::vector<SystemTemplate>;

template <typename SystemTemplate>
using MapFromPriorityToListOfSystems = std::unordered_map<SystemPriority, ListOfSystems<SystemTemplate>>;

template <typename SystemTemplate> class Systems {
public:
  MapFromPriorityToListOfSystems<SystemTemplate> _priority_to_systems;

  explicit Systems() { this->_priority_to_systems = {}; }
  Systems(Systems &&) = default;
  Systems &operator=(Systems &&) = default;
  Systems(const Systems &) = delete;
  Systems &operator=(const Systems &) = delete;
  virtual ~Systems() {}

  std::size_t size() { return this->_entities.size(); }
};

template <typename SystemTemplate> Systems<SystemTemplate> create_systems() { return Systems<SystemTemplate>(); }

template <typename SystemTemplate>
Systems<SystemTemplate> add_system(Systems<SystemTemplate> &systems, SystemTemplate system, SystemPriority priority) {
  if (priority < 0) {
    throw std::runtime_error("Priority must be a positive number!");
  }

  auto &priority_to_systems = systems._priority_to_systems;
  priority_to_systems[priority].push_back(system);
  return std::move(systems);
}

template <typename TypeIndexTemplate, typename ComponentTemplate, typename SystemTemplate, typename ActionTemplate>
using ProcessSystemFunction = std::function<std::vector<ActionTemplate>(
    EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &, SystemTemplate &)>;

template <typename TypeIndexTemplate, typename ComponentTemplate, typename ActionTemplate>
using ProcessActionFunction = std::function<EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate>(
    EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &, ActionTemplate &)>;

template <typename TypeIndexTemplate, typename ComponentTemplate, typename SystemTemplate, typename ActionTemplate>
std::vector<ActionTemplate> _get_actions_from_systems_with_same_priority(
    EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb,
    ListOfSystems<SystemTemplate> &systems_with_same_priority,
    typename type_utils::type_identity<ProcessSystemFunction<TypeIndexTemplate, ComponentTemplate, SystemTemplate,
                                                             ActionTemplate>>::type process_system

) {
  std::vector<ActionTemplate> actions;
  for (auto &system : systems_with_same_priority) {
    auto system_actions = process_system(ecdb, system);
    actions.insert(std::end(actions), std::begin(system_actions), std::end(system_actions));
  }
  return actions;
}

template <typename TypeIndexTemplate, typename ComponentTemplate, typename SystemTemplate, typename ActionTemplate>
EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> process_systems(
    EntityComponentDatabase<TypeIndexTemplate, ComponentTemplate> &ecdb, Systems<SystemTemplate> &systems,
    typename type_utils::type_identity<ProcessSystemFunction<TypeIndexTemplate, ComponentTemplate, SystemTemplate,
                                                             ActionTemplate>>::type process_system,

    typename type_utils::type_identity<
        ProcessActionFunction<TypeIndexTemplate, ComponentTemplate, ActionTemplate>>::type process_action

) {
  for (auto &&[priority, systems_with_same_priority] : systems._priority_to_systems) {
    std::vector<ActionTemplate> actions =
        _get_actions_from_systems_with_same_priority<TypeIndexTemplate, ComponentTemplate, SystemTemplate,
                                                     ActionTemplate>(ecdb, systems_with_same_priority, process_system);

    for (auto &action : actions) {
      ecdb = process_action(ecdb, action);
    }
  }
  return std::move(ecdb);
}

} // namespace mutable_ecs
} // namespace ecs

namespace std {

template <> struct hash<ecs::mutable_ecs::Entity> {
  std::size_t operator()(const ecs::mutable_ecs::Entity &entity) const {
    return hash<ecs::mutable_ecs::UniqueId>{}(entity.unique_id);
  }
};

} // namespace std