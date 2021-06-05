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
    Dict,
    Set,
    List,
)

import attr

UniqueId = int


@attr.s(frozen=True, kw_only=True)
class Entity:
    unique_id: UniqueId = attr.ib()


ComponentTemplate = TypeVar("ComponentTemplate")
IterableOfComponents = Iterable[ComponentTemplate]
SetOfComponents = Set[ComponentTemplate]
ListOfComponents = List[ComponentTemplate]
MapFromComponentTypeToComponent = Dict[Type[ComponentTemplate], ComponentTemplate]
MapFromEntityToMapFromComponentTypeToComponent = Dict[Entity, MapFromComponentTypeToComponent[ComponentTemplate]]


class EntityComponentDatabase(Generic[ComponentTemplate]):
    def __init__(
        self,
        _last_unique_id: UniqueId = 0,
        _entities: Optional[MapFromEntityToMapFromComponentTypeToComponent[ComponentTemplate]] = None,
    ) -> None:
        self._last_unique_id: UniqueId = _last_unique_id
        self._entities: MapFromEntityToMapFromComponentTypeToComponent[
            ComponentTemplate
        ] = _entities if _entities else {}

    def __eq__(self, other: object) -> bool:
        if isinstance(other, EntityComponentDatabase):
            is_equal = self._last_unique_id == other._last_unique_id
            is_equal &= self._entities == other._entities
            return is_equal
        return False


class FilterFunction(Protocol):
    def __call__(self, components: MapFromComponentTypeToComponent[ComponentTemplate]) -> bool:
        ...


def create_ecdb() -> EntityComponentDatabase[ComponentTemplate]:
    return EntityComponentDatabase()


def add_entity(
    *,
    ecdb: EntityComponentDatabase[ComponentTemplate],
    components: Optional[IterableOfComponents[ComponentTemplate]] = None,
) -> Tuple[EntityComponentDatabase[ComponentTemplate], Entity]:
    unique_id = ecdb._last_unique_id  # pylint: disable=protected-access
    ecdb._last_unique_id += 1  # pylint: disable=protected-access

    entity = Entity(unique_id=unique_id)
    ecdb._entities[entity] = {}  # pylint: disable=protected-access

    if components is not None:
        for component in components:
            ecdb = add_component(ecdb=ecdb, entity=entity, component=component)

    return ecdb, entity


def remove_entity(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity,
) -> EntityComponentDatabase[ComponentTemplate]:
    del ecdb._entities[entity]  # pylint: disable=protected-access
    return ecdb


def add_component(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity, component: ComponentTemplate
) -> EntityComponentDatabase[ComponentTemplate]:
    component_type = type(component)

    entities = ecdb._entities  # pylint: disable=protected-access
    entity_components = entities.get(entity, {})
    entity_components[component_type] = component

    return ecdb


def remove_component(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity, component_type: Type[ComponentTemplate]
) -> EntityComponentDatabase[ComponentTemplate]:

    entities = ecdb._entities  # pylint: disable=protected-access
    entity_components = entities.get(entity, {})
    del entity_components[component_type]

    return ecdb


def get_component(
    *, ecdb: EntityComponentDatabase[ComponentTemplate], entity: Entity, component_type: Type[ComponentTemplate]
) -> Optional[ComponentTemplate]:

    entities = ecdb._entities  # pylint: disable=protected-access
    entity_components = entities.get(entity, {})
    component = entity_components.get(component_type)

    return component


def query(
    *,
    ecdb: EntityComponentDatabase[ComponentTemplate],
    component_types: Optional[Iterable[Type[ComponentTemplate]]] = None,
) -> Generator[Tuple[Entity, ListOfComponents[ComponentTemplate]], None, None]:
    for entity, components in ecdb._entities.items():  # pylint: disable=protected-access
        requested_components: ListOfComponents[ComponentTemplate] = []
        if component_types is None:
            for component in components.values():
                requested_components.append(component)
        else:
            for component_type in component_types:
                if component_type not in components:
                    continue
                requested_components.append(components[component_type])

        yield entity, requested_components


# Systems
SystemPriority = int
SystemTemplate = TypeVar("SystemTemplate")
SetOfSystems = Set[SystemTemplate]
MapFromPriorityToSetOfSystems = Dict[SystemPriority, SetOfSystems[SystemTemplate]]


class Systems(Generic[SystemTemplate]):
    _priority_to_systems: MapFromPriorityToSetOfSystems[SystemTemplate] = {}


def create_systems() -> Systems[SystemTemplate]:
    return Systems()


def add_system(
    *, systems: Systems[SystemTemplate], system: SystemTemplate, priority: SystemPriority
) -> Systems[SystemTemplate]:
    assert priority >= 0, "Priority must be a positive number!"

    priority_to_systems = systems._priority_to_systems  # pylint: disable=protected-access
    systems_with_same_priority = priority_to_systems.setdefault(priority, set())
    systems_with_same_priority.add(system)
    return systems


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
