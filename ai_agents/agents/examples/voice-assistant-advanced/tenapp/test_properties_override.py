#!/usr/bin/env python3
"""
Test that demonstrates how channel properties are overridden for avatar extensions
via the /start API's properties parameter.

This simulates the backend's property override logic from http_server.go:612-666
"""
import json
import sys
import copy
from pathlib import Path


def simulate_property_override(graph_name, channel_name, custom_properties):
    """
    Simulate the backend's processProperty function to show how properties are overridden.

    Based on server/internal/http_server.go:534-666
    """
    print("=" * 70)
    print(f"Simulating /start API Property Override")
    print("=" * 70)
    print(f"Graph: {graph_name}")
    print(f"Channel: {channel_name}")
    print(f"Custom Properties: {json.dumps(custom_properties, indent=2)}")
    print("")

    # Load property.json
    with open("property.json") as f:
        property_json = json.load(f)

    # Find the requested graph
    predefined_graphs = property_json["ten"]["predefined_graphs"]
    matching_graphs = [g for g in predefined_graphs if g["name"] == graph_name]

    if not matching_graphs:
        print(f"✗ ERROR: Graph '{graph_name}' not found")
        return False

    # Work with a copy
    graph = copy.deepcopy(matching_graphs[0])

    print(f"✓ Found graph: {graph_name}")
    print("")

    # Step 1: Apply custom properties (lines 612-644 in http_server.go)
    print("Step 1: Applying custom properties from request")
    print("-" * 70)

    for extension_name, props in custom_properties.items():
        print(f"\nProcessing extension: {extension_name}")
        for prop, val in props.items():
            print(f"  Setting {prop} = {val}")

            # Find the node and set the property
            nodes = graph["graph"]["nodes"]
            for node in nodes:
                if node.get("name") == extension_name:
                    node["property"][prop] = val
                    print(f"  ✓ Applied to {node['addon']}")
                    break

    # Step 2: Apply startPropMap overrides (lines 647-666 in http_server.go)
    print("")
    print("Step 2: Applying startPropMap overrides (channel_name)")
    print("-" * 70)

    # Simulate startPropMap from config.go
    start_prop_map = {
        "ChannelName": [
            {"ExtensionName": "agora_rtc", "Property": "channel"},
            {"ExtensionName": "agora_rtm", "Property": "channel"},
        ]
    }

    nodes = graph["graph"]["nodes"]
    for node in nodes:
        node_name = node.get("name")
        addon = node.get("addon")

        # Check if this node matches any startPropMap entry
        for req_field, mappings in start_prop_map.items():
            if req_field == "ChannelName":
                for mapping in mappings:
                    if node_name == mapping["ExtensionName"]:
                        old_val = node["property"].get(
                            mapping["Property"], "N/A"
                        )
                        node["property"][mapping["Property"]] = channel_name
                        print(f"\n{addon} ({node_name}):")
                        print(f"  Property: {mapping['Property']}")
                        print(f"  Before: {old_val}")
                        print(f"  After: {channel_name}")

    # Step 3: Show final property values
    print("")
    print("Step 3: Final Property Values")
    print("=" * 70)

    results = []

    for node in nodes:
        node_name = node.get("name")
        addon = node.get("addon")

        # Show agora_rtc
        if addon == "agora_rtc":
            channel = node["property"].get("channel", "N/A")
            print(f"\nagora_rtc:")
            print(f"  channel: {channel}")
            results.append(("agora_rtc.channel", channel, channel_name))

        # Show avatar nodes
        elif "avatar" in addon or node_name == "avatar":
            print(f"\n{addon} (node name: {node_name}):")
            props = node["property"]

            # Check for channel property
            if "channel" in props:
                val = props["channel"]
                print(f"  channel: {val}")
                results.append((f"{addon}.channel", val, channel_name))

            # Check for agora_channel_name property
            if "agora_channel_name" in props:
                val = props["agora_channel_name"]
                print(f"  agora_channel_name: {val}")
                results.append(
                    (f"{addon}.agora_channel_name", val, channel_name)
                )

    # Verify results
    print("")
    print("Verification")
    print("=" * 70)

    all_correct = True
    for prop_name, actual, expected in results:
        if actual == expected:
            print(f"✓ {prop_name}: {actual}")
        else:
            print(f"✗ {prop_name}: {actual} (expected: {expected})")
            all_correct = False

    return all_correct


def test_heygen_properties():
    """Test voice_assistant_heygen with custom properties"""
    print("\n")
    print("╔" + "=" * 68 + "╗")
    print("║" + " " * 68 + "║")
    print(
        "║"
        + "  TEST: voice_assistant_heygen with channel property".center(68)
        + "║"
    )
    print("║" + " " * 68 + "║")
    print("╚" + "=" * 68 + "╝")
    print("\n")

    result = simulate_property_override(
        graph_name="voice_assistant_heygen",
        channel_name="dynamic_test_channel_123",
        custom_properties={"avatar": {"channel": "dynamic_test_channel_123"}},
    )

    return result


def test_generic_video_properties():
    """Test voice_assistant_generic_video with custom properties"""
    print("\n")
    print("╔" + "=" * 68 + "╗")
    print("║" + " " * 68 + "║")
    print(
        "║"
        + "  TEST: voice_assistant_generic_video with agora_channel_name".center(
            68
        )
        + "║"
    )
    print("║" + " " * 68 + "║")
    print("╚" + "=" * 68 + "╝")
    print("\n")

    result = simulate_property_override(
        graph_name="voice_assistant_generic_video",
        channel_name="dynamic_test_channel_456",
        custom_properties={
            "avatar": {"agora_channel_name": "dynamic_test_channel_456"}
        },
    )

    return result


def test_without_custom_properties():
    """Test what happens WITHOUT custom properties"""
    print("\n")
    print("╔" + "=" * 68 + "╗")
    print("║" + " " * 68 + "║")
    print(
        "║"
        + "  TEST: Without custom properties (EXPECTED TO FAIL)".center(68)
        + "║"
    )
    print("║" + " " * 68 + "║")
    print("╚" + "=" * 68 + "╝")
    print("\n")

    result = simulate_property_override(
        graph_name="voice_assistant_heygen",
        channel_name="dynamic_test_channel_999",
        custom_properties={},  # No custom properties!
    )

    # This should fail because avatar won't get the dynamic channel
    return not result  # Invert - we expect this to fail


if __name__ == "__main__":
    print("\n")
    print("╔" + "=" * 68 + "╗")
    print("║" + " " * 68 + "║")
    print("║" + "  Avatar Channel Override Testing".center(68) + "║")
    print(
        "║" + "  Demonstrates /start API properties parameter".center(68) + "║"
    )
    print("║" + " " * 68 + "║")
    print("╚" + "=" * 68 + "╝")

    results = []

    # Test 1: HeyGen with properties
    results.append(
        ("voice_assistant_heygen with properties", test_heygen_properties())
    )

    # Test 2: Generic Video with properties
    results.append(
        (
            "voice_assistant_generic_video with properties",
            test_generic_video_properties(),
        )
    )

    # Test 3: Without custom properties (should show the problem)
    results.append(
        (
            "Without custom properties (shows issue)",
            test_without_custom_properties(),
        )
    )

    # Summary
    print("\n")
    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print("")

    passed = sum(1 for _, result in results if result)
    total = len(results)

    for name, result in results:
        status = "PASS" if result else "FAIL"
        symbol = "✓" if result else "✗"
        print(f"{symbol} {name}: {status}")

    print(f"\nTotal: {passed}/{total} tests passed")

    print("")
    print("=" * 70)
    print("HOW TO USE IN CLIENT CODE")
    print("=" * 70)
    print("")
    print("When calling /start API, pass the 'properties' parameter:")
    print("")
    print("For voice_assistant_heygen:")
    print("  POST /start")
    print("  {")
    print('    "channel_name": "your_channel",')
    print('    "graph_name": "voice_assistant_heygen",')
    print('    "properties": {')
    print('      "avatar": {')
    print('        "channel": "your_channel"')
    print("      }")
    print("    }")
    print("  }")
    print("")
    print("For voice_assistant_generic_video:")
    print("  POST /start")
    print("  {")
    print('    "channel_name": "your_channel",')
    print('    "graph_name": "voice_assistant_generic_video",')
    print('    "properties": {')
    print('      "avatar": {')
    print('        "agora_channel_name": "your_channel"')
    print("      }")
    print("    }")
    print("  }")
    print("")

    if passed == total:
        print("✓ All tests demonstrate correct behavior!")
        sys.exit(0)
    else:
        print("✗ Some tests failed")
        sys.exit(1)
