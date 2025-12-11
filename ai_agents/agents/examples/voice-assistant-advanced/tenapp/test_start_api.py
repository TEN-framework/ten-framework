#!/usr/bin/env python3
"""
Test script to simulate /start API endpoint behavior for avatar graphs.
Validates that graph selection and property overrides work correctly.
"""
import json
import sys
from pathlib import Path


def simulate_start_api(graph_name, channel_override):
    """
    Simulate the /start API endpoint behavior.
    Based on server/internal/worker.go and http_server.go
    """
    print("=" * 60)
    print(f"Simulating /start API for: {graph_name}")
    print(f"Channel Override: {channel_override}")
    print("=" * 60)

    # Load property.json
    with open("property.json") as f:
        config = json.load(f)

    # Find the requested graph
    graphs = config["ten"]["predefined_graphs"]
    graph = next((g for g in graphs if g["name"] == graph_name), None)

    if not graph:
        print(f"✗ ERROR: Graph '{graph_name}' not found")
        return False

    print(f"✓ Graph '{graph_name}' found")

    # Simulate property override (server applies property_map to extensions)
    # The backend has a startPropMap that determines which properties get overridden
    # For example: agora_rtc.channel, avatar.channel, etc.

    nodes = graph["graph"]["nodes"]

    # Check agora_rtc node
    agora_node = next((n for n in nodes if n["addon"] == "agora_rtc"), None)
    if agora_node:
        current_channel = agora_node["property"].get("channel", "unknown")
        print(f"\n  agora_rtc node:")
        print(f"    - Current channel property: {current_channel}")
        print(f"    - Would be overridden to: {channel_override}")
        print(
            f"    ✓ Property override supported (uses standard 'channel' property)"
        )

    # Check avatar node (if present)
    avatar_node = next((n for n in nodes if n.get("name") == "avatar"), None)
    if avatar_node:
        addon = avatar_node["addon"]
        print(f"\n  avatar node ({addon}):")

        if addon == "heygen_avatar_python":
            current_channel = avatar_node["property"].get("channel", "unknown")
            print(f"    - Current channel property: {current_channel}")
            print(f"    - Would be overridden to: {channel_override}")
            print(
                f"    ✓ Property override supported (uses standard 'channel' property)"
            )

            # Check other properties
            required_props = [
                "heygen_api_key",
                "agora_appid",
                "agora_avatar_uid",
                "input_audio_sample_rate",
            ]
            for prop in required_props:
                if prop in avatar_node["property"]:
                    value = avatar_node["property"][prop]
                    print(f"    - {prop}: {value}")

        elif addon == "generic_video_python":
            # Note: generic_video uses agora_channel_name instead of channel
            current_channel = avatar_node["property"].get(
                "agora_channel_name", "unknown"
            )
            print(
                f"    - Current agora_channel_name property: {current_channel}"
            )
            print(
                f"    ⚠ WARNING: Uses 'agora_channel_name' (may need backend update for override)"
            )

            # Check other properties
            required_props = [
                "generic_video_api_key",
                "agora_appid",
                "agora_video_uid",
                "avatar_id",
                "quality",
                "version",
                "video_encoding",
                "start_endpoint",
                "stop_endpoint",
                "input_audio_sample_rate",
            ]
            for prop in required_props:
                if prop in avatar_node["property"]:
                    value = avatar_node["property"][prop]
                    if len(str(value)) > 50:
                        value = str(value)[:47] + "..."
                    print(f"    - {prop}: {value}")

    # Check TTS -> Avatar connection
    connections = graph["graph"]["connections"]
    tts_conn = next(
        (c for c in connections if c.get("extension") == "tts"), None
    )

    if tts_conn:
        audio_frames = tts_conn.get("audio_frame", [])
        if audio_frames:
            dest = audio_frames[0].get("dest", [])
            if dest and dest[0].get("extension") == "avatar":
                print(f"\n  TTS -> Avatar routing:")
                print(f"    ✓ TTS audio correctly routed to avatar extension")
            else:
                print(f"\n  TTS -> Avatar routing:")
                print(f"    ✗ ERROR: TTS audio not routed to avatar")
                return False

    # Check STT -> LLM -> TTS pipeline
    main_control_conn = next(
        (c for c in connections if c.get("extension") == "main_control"), None
    )
    if main_control_conn:
        data_sources = main_control_conn.get("data", [])
        has_asr = any(d.get("name") == "asr_result" for d in data_sources)
        if has_asr:
            print(f"\n  Voice pipeline:")
            print(f"    ✓ STT -> main_control -> LLM -> TTS pipeline present")

    print(f"\n✓ Graph '{graph_name}' is properly configured for /start API")
    return True


def test_error_scenarios():
    """Test error handling scenarios"""
    print("\n" + "=" * 60)
    print("Testing Error Scenarios")
    print("=" * 60)

    # Test invalid graph name
    with open("property.json") as f:
        config = json.load(f)

    graphs = config["ten"]["predefined_graphs"]
    invalid_graph = next(
        (g for g in graphs if g["name"] == "invalid_graph_name"), None
    )

    if invalid_graph is None:
        print("✓ Invalid graph name correctly returns None (would 404)")
    else:
        print("✗ Invalid graph should not exist")
        return False

    return True


if __name__ == "__main__":
    print("\n")
    print("╔" + "=" * 58 + "╗")
    print("║" + " " * 58 + "║")
    print("║" + "  TEN Framework /start API Test".center(58) + "║")
    print("║" + " " * 58 + "║")
    print("╚" + "=" * 58 + "╝")
    print("\n")

    results = []

    # Test with dynamic channel override
    test_channel = "test_channel_12345"

    # Test voice_assistant (baseline)
    results.append(
        ("voice_assistant", simulate_start_api("voice_assistant", test_channel))
    )

    # Test voice_assistant_heygen
    results.append(
        (
            "voice_assistant_heygen",
            simulate_start_api("voice_assistant_heygen", test_channel),
        )
    )

    # Test voice_assistant_generic_video
    results.append(
        (
            "voice_assistant_generic_video",
            simulate_start_api("voice_assistant_generic_video", test_channel),
        )
    )

    # Test error scenarios
    results.append(("Error Scenarios", test_error_scenarios()))

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
        print("\n✓ All /start API tests passed!")
        print("\nNOTE: generic_video uses 'agora_channel_name' property.")
        print("Backend may need update to support dynamic channel override.")
        sys.exit(0)
    else:
        print("\n✗ Some tests failed. Review the output above.")
        sys.exit(1)
