"""Reference-solver tests. These do not execute Navit's production routing code."""
import unittest
from pathlib import Path

from oracle import apply_event, load_fixture, shortest_route

FIXTURES = Path(__file__).parent / "fixtures"


class OracleSpecificationTests(unittest.TestCase):
    def test_directed_road(self):
        fixture = load_fixture(FIXTURES / "directed.json")
        self.assertEqual(shortest_route(fixture), (4, ("ab",)))
        self.assertEqual(shortest_route({**fixture, "start": "B", "goal": "A"}), (None, ()))

    def test_cheapest_alternative(self):
        fixture = load_fixture(FIXTURES / "alternative.json")
        self.assertEqual(shortest_route(fixture), (7, ("ac", "cd")))

    def test_forbidden_turn(self):
        fixture = load_fixture(FIXTURES / "forbidden_turn.json")
        self.assertEqual(shortest_route(fixture), (8, ("ab", "bd", "dc")))

    def test_successor_dependent_penalty(self):
        fixture = load_fixture(FIXTURES / "through_traffic.json")
        self.assertEqual(shortest_route(fixture), (3, ("ab", "bd", "dg")))

    def test_same_junction_requires_distinct_arrival_contexts(self):
        fixture = load_fixture(FIXTURES / "dual_ingress.json")
        self.assertEqual(shortest_route(fixture), (4, ("ab", "bd", "dg")))
        self.assertEqual(
            shortest_route({**fixture, "start": "X"}),
            (3, ("xb", "bc", "cg")),
        )

    def test_equal_cost_successor_transition(self):
        fixture = load_fixture(FIXTURES / "equal_successor.json")
        self.assertEqual(shortest_route(fixture), (3, ("ab", "bd", "dg")))

    def test_traffic_increase_decrease_and_closure(self):
        fixture = load_fixture(FIXTURES / "traffic_events.json")
        self.assertEqual(shortest_route(fixture), (5, ("ab", "bd")))
        state = apply_event(fixture, fixture["events"][0])
        self.assertEqual(shortest_route(state), (8, ("ac", "cd")))
        state = apply_event(state, fixture["events"][1])
        self.assertEqual(shortest_route(state), (3, ("ab", "bd")))
        state = apply_event(state, fixture["events"][2])
        self.assertEqual(shortest_route(state), (8, ("ac", "cd")))
        state = apply_event(state, fixture["events"][3])
        self.assertEqual(shortest_route(state), (3, ("ab", "bd")))
        self.assertEqual(shortest_route(fixture), (5, ("ab", "bd")))

    def test_invalid_transition_is_rejected(self):
        fixture = load_fixture(FIXTURES / "directed.json")
        fixture["transitions"] = [{"incoming": "ab", "outgoing": "ab", "penalty": 1}]
        with self.assertRaisesRegex(ValueError, "disconnected transition"):
            shortest_route(fixture)

    def test_unreachable_graph(self):
        fixture = load_fixture(FIXTURES / "directed.json")
        fixture["goal"] = "C"
        self.assertEqual(shortest_route(fixture), (None, ()))


if __name__ == "__main__":
    unittest.main()
