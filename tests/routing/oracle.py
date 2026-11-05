"""Independent directed-movement shortest-path oracle for routing specifications.

This deliberately does not import Navit internals. Passing these tests does NOT
establish correctness of Navit's production routing code.
"""
from __future__ import annotations

import heapq
import json
from pathlib import Path


def load_fixture(path: str | Path) -> dict:
    with open(path, encoding="utf-8") as handle:
        fixture = json.load(handle)
    validate_fixture(fixture)
    return fixture


def validate_fixture(fixture: dict) -> None:
    if fixture.get("schema_version") != 1:
        raise ValueError("schema_version must be 1")
    roads = fixture.get("roads")
    if not isinstance(roads, list) or not roads:
        raise ValueError("roads must be a non-empty list")
    ids = set()
    for road in roads:
        if not isinstance(road, dict) or not isinstance(road.get("id"), str) or not road["id"]:
            raise ValueError("each road requires a non-empty string id")
        if road["id"] in ids:
            raise ValueError(f"duplicate road id: {road['id']}")
        ids.add(road["id"])
        if not all(isinstance(road.get(field), str) and road[field] for field in ("from", "to")):
            raise ValueError(f"road {road['id']} requires from/to")
        if "closed" in road and type(road["closed"]) is not bool:
            raise ValueError(f"road {road['id']} closed must be boolean")
        cost = road.get("cost")
        if type(cost) is not int or cost < 0:
            raise ValueError(f"road {road['id']} has invalid cost")
    if not isinstance(fixture.get("start"), str) or not isinstance(fixture.get("goal"), str):
        raise ValueError("start and goal must be strings")
    for transition in fixture.get("transitions", []):
        if transition.get("incoming") not in ids or transition.get("outgoing") not in ids:
            raise ValueError("transition references unknown road")
        if "penalty" in transition and (type(transition["penalty"]) is not int or transition["penalty"] < 0):
            raise ValueError("transition penalty must be a non-negative integer")
        if "prohibited" in transition and type(transition["prohibited"]) is not bool:
            raise ValueError("transition prohibited must be boolean")
        if transition.get("prohibited", False) and "penalty" in transition:
            raise ValueError("prohibited transition must not also have a penalty")
    for change in fixture.get("events", []):
        if change.get("road") not in ids:
            raise ValueError("event references unknown road")
        if "cost" in change and (type(change["cost"]) is not int or change["cost"] < 0):
            raise ValueError("event cost must be a non-negative integer")
        if "closed" in change and type(change["closed"]) is not bool:
            raise ValueError("event closed must be boolean")
        if "cost" not in change and "closed" not in change:
            raise ValueError("event must change cost or closed")


def apply_event(fixture: dict, event: dict) -> dict:
    """Return a new snapshot; never modify the previous snapshot."""
    updated = json.loads(json.dumps(fixture))
    for road in updated["roads"]:
        if road["id"] == event["road"]:
            for field in ("cost", "closed"):
                if field in event:
                    road[field] = event[field]
            break
    else:
        raise ValueError(f"unknown road {event['road']}")
    validate_fixture(updated)
    return updated


def shortest_route(fixture: dict) -> tuple[int | None, tuple[str, ...]]:
    """Dijkstra over (junction, incoming-road-ID) states, with nonnegative costs."""
    validate_fixture(fixture)
    by_id = {road["id"]: road for road in fixture["roads"]}
    outgoing = {}
    for road in fixture["roads"]:
        outgoing.setdefault(road["from"], []).append(road)
    transitions = {}
    for rule in fixture.get("transitions", []):
        incoming, outgoing_id = rule["incoming"], rule["outgoing"]
        if by_id[incoming]["to"] != by_id[outgoing_id]["from"]:
            raise ValueError(f"disconnected transition: {incoming} -> {outgoing_id}")
        if (incoming, outgoing_id) in transitions:
            raise ValueError(f"duplicate transition: {incoming} -> {outgoing_id}")
        transitions[(incoming, outgoing_id)] = rule
    start = (fixture["start"], None)
    distances = {start: 0}
    paths = {start: ()}
    queue = [(0, (), start)]
    while queue:
        cost, path, state = heapq.heappop(queue)
        if cost != distances.get(state) or path != paths[state]:
            continue
        junction, incoming = state
        if junction == fixture["goal"]:
            return cost, path
        for road in outgoing.get(junction, ()):
            if road.get("closed", False):
                continue
            rule = transitions.get((incoming, road["id"]), {}) if incoming is not None else {}
            if rule.get("prohibited", False):
                continue
            new_cost = cost + road["cost"] + rule.get("penalty", 0)
            next_state = (road["to"], road["id"])
            if next_state not in distances or new_cost < distances[next_state]:
                next_path = path + (road["id"],)
                distances[next_state] = new_cost
                paths[next_state] = next_path
                heapq.heappush(queue, (new_cost, next_path, next_state))
    return None, ()
