"""
Generic Entity-Component-System code

In this file, access to private members of EntityComponentDatabase[Component] class is allowed.
"""


from typing import (
    Protocol,
    Generic,
    Type,
    TypeVar,
    Iterable,
    Optional,
    Tuple,
    Generator,
    Any,
)

import attr
import pyrsistent
from pyrsistent.typing import PMap, PSet, PVector

UniqueId = int


@attr.s(frozen=True, kw_only=True)
class Entity:
    unique_id: UniqueId = attr.ib()


ComponentTemplate = TypeVar("ComponentTemplate")
IterableOfComponents = Iterable[ComponentTemplate]
SetOfComponents = PSet[ComponentTemplate]
ListOfComponents = PVector[ComponentTemplate]
MapFromComponentTypeToComponent = PMap[Type[ComponentTemplate], ComponentTemplate]
MapFromEntityToMapFromComponentTypeToComponent = PMap[Entity, MapFromComponentTypeToComponent[ComponentTemplate]]


class EntityComponentDatabase(Generic[ComponentTemplate], pyrsistent.PClass):
    _last_unique_id: UniqueId = pyrsistent.field(mandatory=True)
    _entities: MapFromEntityToMapFromComponentTypeToComponent[ComponentTemplate] = pyrsistent.field(
        initial=pyrsistent.pmap
    )

    def __len__(self) -> int:
        return len(self._entities)


class FilterFunction(Protocol):
    def __call__(self, components: MapFromComponentTypeToComponent[ComponentTemplate]) -> bool:
        ...


def create_ecdb() -> EntityComponentDatabase[ComponentTemplate]:
    return EntityComponentDatabase(_last_unique_id=0)


def add_entity(
    *,
    ecdb: EntityComponentDatabase[ComponentTemplate],
    components: Optional[IterableOfComponents[ComponentTemplate]] = None,
) -> Tuple[EntityComponentDatabase[ComponentTemplate], Entity]:
    unique_id = ecdb._last_unique_id  # pylint: disable=protected-access
    new_unique_id = unique_id + 1

    entity = Entity(unique_id=unique_id)
    new_entities = ecdb._entities.set(entity, pyrsistent.pmap())  # pylint: disable=protected-access

    ecdb = ecdb.set(_last_unique_id=new_unique_id, _entities=new_entities)

    if components is not None:
        for component in components:
            ecdb = add_component(ecdb=ecdb, entity=entity, component=component)

    return ecdb, entity


def remove_entity(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity,
) -> EntityComponentDatabase[ComponentTemplate]:
    new_entities = ecdb._entities.remove(entity)  # pylint: disable=protected-access
    ecdb = ecdb.set(_entities=new_entities)
    return ecdb


def add_component(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity, component: ComponentTemplate
) -> EntityComponentDatabase[ComponentTemplate]:
    component_type = type(component)

    entities = ecdb._entities  # pylint: disable=protected-access
    entity_components = entities.get(entity, pyrsistent.pmap())
    new_entity_components = entity_components.set(component_type, component)
    new_entities = entities.set(entity, new_entity_components)

    return ecdb.set(_entities=new_entities)


def remove_component(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity, component_type: Type[ComponentTemplate]
) -> EntityComponentDatabase[ComponentTemplate]:

    entities = ecdb._entities  # pylint: disable=protected-access
    entity_components = entities.get(entity, pyrsistent.pmap())
    new_entity_components = entity_components.remove(component_type)
    new_entities = entities.set(entity, new_entity_components)

    return ecdb.set(_entities=new_entities)


def get_component(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity, component_type: Type[ComponentTemplate]
) -> Optional[ComponentTemplate]:

    entities = ecdb._entities  # pylint: disable=protected-access
    entity_components = entities.get(entity, pyrsistent.pmap())
    component = entity_components.get(component_type)

    return component


def query(
    *,
    ecdb: EntityComponentDatabase[ComponentTemplate],
    component_types: Optional[Iterable[Type[ComponentTemplate]]] = None,
) -> Generator[Tuple[Entity, ListOfComponents[ComponentTemplate]], None, None]:
    for entity, components in ecdb._entities.items():  # pylint: disable=protected-access

        requested_components: ListOfComponents[ComponentTemplate] = pyrsistent.pvector()
        if component_types is None:
            for component in components.values():
                requested_components = requested_components.append(component)
        else:
            for component_type in component_types:
                if component_type not in components:
                    continue
                requested_components = requested_components.append(components[component_type])

            skip_entity = len(requested_components) < len(components)
            if skip_entity:
                continue

        yield entity, requested_components


# Systems
SystemPriority = int
SystemTemplate = TypeVar("SystemTemplate")
SetOfSystems = PSet[SystemTemplate]
MapFromPriorityToSetOfSystems = PMap[SystemPriority, SetOfSystems[SystemTemplate]]


class Systems(Generic[SystemTemplate], pyrsistent.PClass):
    _priority_to_systems: MapFromPriorityToSetOfSystems[SystemTemplate] = pyrsistent.field(initial=pyrsistent.pmap)


def create_systems() -> Systems[SystemTemplate]:
    return Systems()


def add_system(
    *, systems: Systems[SystemTemplate], system: SystemTemplate, priority: SystemPriority
) -> Systems[SystemTemplate]:
    assert priority >= 0, "Priority must be a positive number!"

    priority_to_systems = systems._priority_to_systems  # pylint: disable=protected-access
    systems_with_same_priority = priority_to_systems.get(priority, pyrsistent.pset())
    new_systems_with_same_priority = systems_with_same_priority.add(system)
    new_priority_to_systems = priority_to_systems.set(priority, new_systems_with_same_priority)
    return systems.set(_priority_to_systems=new_priority_to_systems)


def get_systems_by_priority(*, systems: Systems[SystemTemplate]) -> Generator[SetOfSystems[SystemTemplate], None, None]:
    for _, systems_with_same_priority in systems._priority_to_systems.items():  # pylint: disable=protected-access
        yield systems_with_same_priority


# class ProcessSystemFunction(Protocol):
#     def __call__(
#         self, *, system: SystemTemplate, ecdb: EntityComponentDatabase[ComponentTemplate]
#     ) -> EntityComponentDatabase[ComponentTemplate]:
#         ...


# TODO: use protocol above
class ProcessSystemFunction(Protocol):
    def __call__(self, *, ecdb: Any, system: Any) -> Any:
        ...


# TODO: Don't use Any
class ProcessActionFunction(Protocol):
    def __call__(self, *, ecdb: Any, action: Any) -> Any:
        ...


def _get_actions_from_systems_with_same_priority(
    *,
    ecdb: EntityComponentDatabase[ComponentTemplate],
    systems_with_same_priority: SetOfSystems[SystemTemplate],
    process_system: ProcessSystemFunction,
) -> Generator[Any, None, None]:
    for system in systems_with_same_priority:
        yield from process_system(ecdb=ecdb, system=system)


def process_systems(
    *,
    ecdb: EntityComponentDatabase[ComponentTemplate],
    systems: Systems[SystemTemplate],
    process_system: ProcessSystemFunction,
    process_action: ProcessActionFunction,
) -> EntityComponentDatabase[ComponentTemplate]:
    for systems_with_same_priority in get_systems_by_priority(systems=systems):
        actions = _get_actions_from_systems_with_same_priority(
            ecdb=ecdb, systems_with_same_priority=systems_with_same_priority, process_system=process_system,
        )

        for action in actions:
            ecdb = process_action(ecdb=ecdb, action=action)
    return ecdb


# Systems End
