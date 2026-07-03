#!/usr/bin/env python3
"""
Test script to verify all graphs are properly configured and
would be accessible via the /graphs API endpoint.
"""
import json
import sys
from pathlib import Path


def test_graphs_endpoint():
    """Simulate the /graphs GET endpoint behavior"""
    print("=" * 60)
    print("Testing /graphs Endpoint Response")
    print("=" * 60)

    # Read property.json
    property_file = Path("property.json")
    if not property_file.exists():
        print("ERROR: property.json not found")
        return False

    with open(property_file) as f:
        config = json.load(f)

    # Extract graphs as the server would
    graphs = config["ten"]["predefined_graphs"]

    # Format response as the server does (see http_server.go:113-161)
    response = []
    for graph in graphs:
        response.append(
            {
                "name": graph["name"],
                "graph_id": graph["name"],
                "auto_start": graph.get("auto_start", False),
            }
        )

    print(f"\nFound {len(response)} graphs:\n")
    print(json.dumps(response, indent=2))

    # Verify expected graphs
    expected_graphs = [
        "voice_assistant",
        "voice_assistant_heygen",
        "voice_assistant_generic_video",
    ]

    actual_graph_names = [g["name"] for g in response]

    print("\n" + "-" * 60)
    print("Validation Results:")
    print("-" * 60)

    all_present = True
    for expected in expected_graphs:
        if expected in actual_graph_names:
            print(f"✓ {expected}")
        else:
            print(f"✗ {expected} - MISSING")
            all_present = False

    return all_present


def test_graph_structure(graph_name):
    """Test that a graph has valid structure for /start API"""
    print("\n" + "=" * 60)
    print(f"Testing Graph Structure: {graph_name}")
    print("=" * 60)

    with open("property.json") as f:
        config = json.load(f)

    graphs = config["ten"]["predefined_graphs"]
    graph = next((g for g in graphs if g["name"] == graph_name), None)

    if not graph:
        print(f"ERROR: Graph '{graph_name}' not found")
        return False

    # Check required fields
    required_fields = ["name", "graph"]
    has_all_fields = True

    for field in required_fields:
        if field in graph:
            print(f"✓ Has '{field}' field")
        else:
            print(f"✗ Missing '{field}' field")
            has_all_fields = False

    # Check graph structure
    if "graph" in graph:
        graph_obj = graph["graph"]
        if "nodes" in graph_obj:
            nodes = graph_obj["nodes"]
            print(f"✓ Has {len(nodes)} nodes")

            # Check for avatar node in avatar graphs
            if "heygen" in graph_name or "generic_video" in graph_name:
                avatar_nodes = [n for n in nodes if n.get("name") == "avatar"]
                if avatar_nodes:
                    addon = avatar_nodes[0]["addon"]
                    print(f"✓ Has avatar node with addon: {addon}")

                    # Verify correct avatar extension
                    if (
                        "heygen" in graph_name
                        and addon == "heygen_avatar_python"
                    ):
                        print(f"✓ Correct avatar extension for HeyGen graph")
                    elif (
                        "generic_video" in graph_name
                        and addon == "generic_video_python"
                    ):
                        print(
                            f"✓ Correct avatar extension for Generic Video graph"
                        )
                    else:
                        print(f"✗ Unexpected avatar extension: {addon}")
                        has_all_fields = False
                else:
                    print(f"✗ Missing avatar node")
                    has_all_fields = False
        else:
            print(f"✗ Missing 'nodes' in graph")
            has_all_fields = False

        if "connections" in graph_obj:
            connections = graph_obj["connections"]
            print(f"✓ Has {len(connections)} connections")

            # Check for TTS -> Avatar connection in avatar graphs
            if "heygen" in graph_name or "generic_video" in graph_name:
                tts_connections = [
                    c for c in connections if c.get("extension") == "tts"
                ]
                if tts_connections:
                    audio_frame = tts_connections[0].get("audio_frame", [])
                    if audio_frame:
                        dest = audio_frame[0].get("dest", [])
                        if dest and dest[0].get("extension") == "avatar":
                            print(f"✓ TTS audio correctly routed to avatar")
                        else:
                            print(f"✗ TTS audio not routed to avatar")
                            has_all_fields = False
                else:
                    print(f"✗ No TTS connections found")
                    has_all_fields = False
        else:
            print(f"✗ Missing 'connections' in graph")
            has_all_fields = False

    return has_all_fields


def test_manifest_dependencies():
    """Test that manifest.json includes avatar extension dependencies"""
    print("\n" + "=" * 60)
    print("Testing Manifest Dependencies")
    print("=" * 60)

    manifest_file = Path("manifest.json")
    if not manifest_file.exists():
        print("ERROR: manifest.json not found")
        return False

    with open(manifest_file) as f:
        manifest = json.load(f)

    dependencies = manifest.get("dependencies", [])

    required_extensions = ["heygen_avatar_python", "generic_video_python"]

    all_present = True
    for ext in required_extensions:
        matching = [d for d in dependencies if ext in d.get("path", "")]
        if matching:
            print(f"✓ {ext} dependency present")
        else:
            print(f"✗ {ext} dependency MISSING")
            all_present = False

    return all_present


if __name__ == "__main__":
    print("\n")
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 58 + "║")
    print("║" + "  TEN Framework Graph Configuration Test".center(58) + "║")
    print("║" + " " * 58 + "║")
    print("╚" + "=" * 58 + "╝")
    print("\n")

    results = []

    # Test /graphs endpoint
    results.append(("Graphs Endpoint", test_graphs_endpoint()))

    # Test individual graph structures
    results.append(
        (
            "voice_assistant_heygen",
            test_graph_structure("voice_assistant_heygen"),
        )
    )
    results.append(
        (
            "voice_assistant_generic_video",
            test_graph_structure("voice_assistant_generic_video"),
        )
    )

    # Test manifest
    results.append(("Manifest Dependencies", test_manifest_dependencies()))

    # Summary
    print("\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)

    passed = sum(1 for _, result in results if result)
    total = len(results)

    for name, result in results:
        status = "PASS" if result else "FAIL"
        symbol = "✓" if result else "✗"
        print(f"{symbol} {name}: {status}")

    print(f"\nTotal: {passed}/{total} tests passed")

    if passed == total:
        print("\n✓ All tests passed! Graphs are properly configured.")
        sys.exit(0)
    else:
        print("\n✗ Some tests failed. Review the output above.")
        sys.exit(1)
