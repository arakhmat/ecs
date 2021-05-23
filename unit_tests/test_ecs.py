from typing import cast, Generator, Union, List, Type

import attr
import pytest
from toolz import first, count

from ecs import (
    EntityComponentDatabase,
    Entity,
    Systems,
    create_ecdb,
    create_systems,
    add_entity,
    add_system,
    process_systems,
    query,
    add_component,
    remove_entity,
)


@attr.s(frozen=True, kw_only=True)
class PositionComponent:
    y_axis: int = attr.ib()
    x_axis: int = attr.ib()

    def __add__(self, velocity_component: "VelocityComponent") -> "PositionComponent":
        new_y_axis = self.y_axis + velocity_component.y_axis
        new_x_axis = self.x_axis + velocity_component.x_axis
        return PositionComponent(y_axis=new_y_axis, x_axis=new_x_axis)


@attr.s(frozen=True, kw_only=True)
class VelocityComponent:
    y_axis: int = attr.ib()
    x_axis: int = attr.ib()


ComponentUnion = Union[PositionComponent, VelocityComponent]


@attr.s(frozen=True, kw_only=True)
class AddComponentAction:
    entity: Entity = attr.ib()
    component: ComponentUnion = attr.ib()


@attr.s(frozen=True, kw_only=True)
class RemoveEntityAction:
    entity: Entity = attr.ib()


ActionUnion = Union[AddComponentAction, RemoveEntityAction]


def process_action(
    ecdb: EntityComponentDatabase[ComponentUnion], action: ActionUnion
) -> EntityComponentDatabase[ComponentUnion]:
    if isinstance(action, AddComponentAction):
        ecdb = add_component(ecdb=ecdb, entity=action.entity, component=action.component)
    elif isinstance(action, RemoveEntityAction):
        ecdb = remove_entity(ecdb=ecdb, entity=action.entity)
    else:
        raise ValueError(f"Unrecognized Action: {action}")
    return ecdb


class MovementSystem:
    def __call__(self, *, ecdb: EntityComponentDatabase[ComponentUnion]) -> Generator[AddComponentAction, None, None]:
        component_types: List[Type[ComponentUnion]] = [PositionComponent, VelocityComponent]
        for entity, (position_component, velocity_component) in query(ecdb=ecdb, component_types=component_types):

            position_component = cast(PositionComponent, position_component)
            velocity_component = cast(VelocityComponent, velocity_component)

            new_position_component = position_component + velocity_component

            yield AddComponentAction(entity=entity, component=new_position_component)
            yield AddComponentAction(entity=entity, component=VelocityComponent(y_axis=0, x_axis=0))


class RemoveRandomEntitySystem:
    def __call__(self, *, ecdb: EntityComponentDatabase[ComponentUnion]) -> Generator[RemoveEntityAction, None, None]:
        # not so random after all :)
        entities = query(ecdb=ecdb)
        entity, _ = first(entities)
        yield RemoveEntityAction(entity=entity)


SystemUnion = Union[MovementSystem, RemoveRandomEntitySystem]


def process_system(
    *, ecdb: EntityComponentDatabase[ComponentUnion], system: SystemUnion,
) -> Generator[ActionUnion, None, None]:
    if isinstance(system, MovementSystem):
        yield from system(ecdb=ecdb)
    elif isinstance(system, RemoveRandomEntitySystem):
        yield from system(ecdb=ecdb)
    else:
        raise ValueError(f"Unrecognized System: {system}")


@pytest.mark.parametrize("num_original_entities", [10])
def test_ecs(num_original_entities: int) -> None:

    ecdb: EntityComponentDatabase[ComponentUnion] = create_ecdb()

    # Add a few entities
    for _ in range(num_original_entities):
        ecdb, _ = add_entity(
            ecdb=ecdb, components=(PositionComponent(y_axis=0, x_axis=0), VelocityComponent(y_axis=0, x_axis=0))
        )

    systems: Systems[SystemUnion] = create_systems()
    systems = add_system(systems=systems, priority=0, system=MovementSystem())
    systems = add_system(systems=systems, priority=1, system=RemoveRandomEntitySystem())

    loop_index = 0
    while True:
        ecdb = process_systems(ecdb=ecdb, systems=systems, process_system=process_system, process_action=process_action)

        assert len(ecdb) == num_original_entities - loop_index - 1

        if count(query(ecdb=ecdb)) == 0:
            break

        loop_index += 1
