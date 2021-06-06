#include <iostream>
#include <variant>

#include <catch2/catch.hpp>

#include "ecs/mutable_ecs.hpp"
#include "ecs/variant_utils.hpp"

namespace test_basics {
using TypeIndex = std::size_t;
using ComponentType = std::variant<int, float>;

constexpr int INT_COMPONENT = 6;
constexpr float FLOAT_COMPONENT = 2.3;

TEST_CASE("Test EntityComponentDataBase Move-Only Semantics") {
  auto ecdb = ecs::mutable_ecs::create_ecdb<TypeIndex, ComponentType>();

  auto [tmp_ecdb_0, tmp_entity_0] = add_entity(ecdb);
  REQUIRE(ecdb.size() == 0);
  REQUIRE(tmp_ecdb_0.size() == 1);

  auto [tmp_ecdb_1, tmp_entity_1] = add_entity(tmp_ecdb_0);
  REQUIRE(ecdb.size() == 0);
  REQUIRE(tmp_ecdb_0.size() == 0);
  REQUIRE(tmp_ecdb_1.size() == 2);

  ecdb = remove_entity(tmp_ecdb_1, tmp_entity_0);
  REQUIRE(ecdb.size() == 1);
  REQUIRE(tmp_ecdb_0.size() == 0);
  REQUIRE(tmp_ecdb_1.size() == 0);
}

TEST_CASE("Test EntityComponentDataBase APIs") {
  auto ecdb = ecs::mutable_ecs::create_ecdb<TypeIndex, ComponentType>();

  ecs::mutable_ecs::Entity entity_0;
  std::tie(ecdb, entity_0) = add_entity(ecdb, {INT_COMPONENT, FLOAT_COMPONENT});

  ecs::mutable_ecs::Entity entity_1;
  std::tie(ecdb, entity_1) = add_entity(ecdb, {INT_COMPONENT, FLOAT_COMPONENT});

  REQUIRE(ecdb.size() == 2);

  auto test_queried_entities = [](const auto &queried_entities, int num_expected_entities) {
    REQUIRE(queried_entities.size() == num_expected_entities);
    for (auto &&[entity, components] : queried_entities) {

      auto int_component = std::get<int>(components.at(0));
      REQUIRE(int_component == INT_COMPONENT);

      auto float_component = std::get<float>(components.at(1));
      REQUIRE(float_component == Approx(FLOAT_COMPONENT));
    }
  };

  // Query using compile-time API
  auto compile_time_queried_entities = ecs::mutable_ecs::query<int, float>(ecdb);
  test_queried_entities(compile_time_queried_entities, 2);

  auto run_time_queried_entities =
      ecs::mutable_ecs::query(ecdb, std::vector{ecs::type_utils::get_type_id<int>(), ecs::type_utils::get_type_id<float>()});
  test_queried_entities(run_time_queried_entities, 2);

  ecdb = remove_entity(ecdb, entity_0);
  REQUIRE(ecdb.size() == 1);

  ecdb = remove_entity(ecdb, entity_1);
  REQUIRE(ecdb.size() == 0);
}
} // namespace test_basics

namespace test_mutable_ecs_cpp {
using TypeIndex = std::size_t;

struct PositionComponent {
  int y;
  int x;
};

bool operator==(const PositionComponent &a, const PositionComponent &b) {
  bool equal = true;
  equal &= a.y and b.y;
  equal &= a.x and b.x;
  return equal;
};

struct VelocityComponent {
  int y;
  int x;
};

bool operator==(const VelocityComponent &a, const VelocityComponent &b) {
  bool equal = true;
  equal &= a.y and b.y;
  equal &= a.x and b.x;
  return equal;
};

PositionComponent operator+(const PositionComponent &a, const VelocityComponent &b) {
  int y = a.y + b.y;
  int x = a.x + b.x;
  return PositionComponent{.y = y, .x = x};
};

using ComponentType = std::variant<PositionComponent, VelocityComponent>;
using EntityComponentDatabase = ecs::mutable_ecs::EntityComponentDatabase<TypeIndex, ComponentType>;

struct AddComponentAction {
  ecs::mutable_ecs::Entity entity;
  ComponentType component;
};

struct RemoveEntityAction {
  ecs::mutable_ecs::Entity entity;
};

using ActionUnion = std::variant<AddComponentAction, RemoveEntityAction>;

EntityComponentDatabase process_action(EntityComponentDatabase &ecdb, ActionUnion &action) {
  ecdb = std::visit(
      ecs::variant_utils::overloaded{
          [&ecdb](const AddComponentAction &action) { return add_component(ecdb, action.entity, action.component); },
          [&ecdb](const RemoveEntityAction &action) { return remove_entity(ecdb, action.entity); },
      },
      action);
  return std::move(ecdb);
}

struct MovementSystem {

  std::vector<ActionUnion> operator()(EntityComponentDatabase &ecdb) const {
    std::vector<ActionUnion> actions;
    auto queried_entities = ecs::mutable_ecs::query<PositionComponent, VelocityComponent>(ecdb);
    for (auto &&[entity, components] : queried_entities) {
      auto position_component = std::get<PositionComponent>(components.at(0));
      auto velocity_component = std::get<VelocityComponent>(components.at(1));

      actions.emplace_back(AddComponentAction{.entity{entity}, .component{position_component + velocity_component}});
      actions.emplace_back(AddComponentAction{.entity{entity}, .component{VelocityComponent{.y = 0, .x = 0}}});
    }
    return actions;
  }
};

struct RemoveRandomEntitySystem {

  std::vector<ActionUnion> operator()(EntityComponentDatabase &ecdb) const {
    std::vector<ActionUnion> actions;
    auto &entities = ecdb._entity_to_component_types;
    auto &first_entity = entities.begin()->first;
    actions.emplace_back(RemoveEntityAction{.entity{first_entity}});
    return actions;
  }
};

using SystemUnion = std::variant<MovementSystem, RemoveRandomEntitySystem>;

std::vector<ActionUnion> process_system(EntityComponentDatabase &ecdb, SystemUnion &system) {
  auto actions = std::visit(ecs::variant_utils::overloaded{
                                [&ecdb](const MovementSystem &system) { return system(ecdb); },
                                [&ecdb](const RemoveRandomEntitySystem &system) { return system(ecdb); },
                            },
                            system);

  return actions;
}

TEST_CASE("Test Mutable C++ Ecs") {

  auto ecdb = ecs::mutable_ecs::create_ecdb<TypeIndex, ComponentType>();

  auto num_original_entities = 10;
  for (auto i = 0; i < num_original_entities; i++) {
    ecs::mutable_ecs::Entity entity;
    std::tie(ecdb, entity) = add_entity(ecdb, {PositionComponent{.y = 0, .x = 0}, VelocityComponent{.y = 0, .x = 0}});
  }

  auto systems = ecs::mutable_ecs::create_systems<SystemUnion>();
  systems = ecs::mutable_ecs::add_system<SystemUnion>(systems, MovementSystem(), 0);
  systems = ecs::mutable_ecs::add_system<SystemUnion>(systems, RemoveRandomEntitySystem(), 0);

  int loop_index = 0;
  while (true) {
    ecdb = ecs::mutable_ecs::process_systems<TypeIndex, ComponentType, SystemUnion, ActionUnion>(
        ecdb, systems, process_system, process_action);

    REQUIRE(ecdb.size() == num_original_entities - loop_index - 1);

    if (ecdb.size() == 0) {
      break;
    }

    loop_index += 1;
  }
}
} // namespace test_mutable_ecs_cpp
