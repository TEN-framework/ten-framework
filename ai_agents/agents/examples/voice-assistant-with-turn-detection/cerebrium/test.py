import asyncio
import os
from openai import AsyncOpenAI

# Read configuration from environment variables
TTD_BASE_URL = os.getenv("TTD_BASE_URL")
TTD_API_KEY = os.getenv("TTD_API_KEY")

if not TTD_BASE_URL or not TTD_API_KEY:
    raise ValueError(
        "Missing required environment variables. Please set:\n"
        "  TTD_BASE_URL - Cerebrium endpoint URL (must end with /run)\n"
        "  TTD_API_KEY - Your Cerebrium API key"
    )

# Initialize AsyncOpenAI client with Cerebrium endpoint
# Note: base_url must end with /run according to Cerebrium docs
client = AsyncOpenAI(base_url=TTD_BASE_URL, api_key=TTD_API_KEY)


async def test_turn_detection():
    """Test the OpenAI-compatible Turn Detection API"""

    print("=" * 60)
    print("Testing Turn Detection API with AsyncOpenAI")
    print("=" * 60)

    # Test Case 1: Incomplete sentence
    print("\n[Test 1] Incomplete sentence:")
    response1 = await client.chat.completions.create(
        model="ten-turn-detection",
        messages=[{"role": "user", "content": "Hello I have a question about"}],
    )

    # Cerebrium wraps the response in a 'result' field
    turn_state1 = response1.result["choices"][0]["message"]["content"]
    print("User: 'Hello I have a question about'")
    print(f"Turn Detection Result: {turn_state1}")
    print(f"Response ID: {response1.result['id']}")
    print(f"Tokens Used: {response1.result['usage']['total_tokens']}")
    print(f"Run Time: {response1.run_time_ms:.2f}ms")

    # Test Case 2: Complete question
    print("\n[Test 2] Complete question:")
    response2 = await client.chat.completions.create(
        model="ten-turn-detection",
        messages=[
            {"role": "user", "content": "Can you help me with my order?"}
        ],
    )

    turn_state2 = response2.result["choices"][0]["message"]["content"]
    print("User: 'Can you help me with my order?'")
    print(f"Turn Detection Result: {turn_state2}")
    print(f"Response ID: {response2.result['id']}")
    print(f"Tokens Used: {response2.result['usage']['total_tokens']}")
    print(f"Run Time: {response2.run_time_ms:.2f}ms")

    # Test Case 3: With system prompt
    print("\n[Test 3] With system prompt:")
    response3 = await client.chat.completions.create(
        model="ten-turn-detection",
        messages=[
            {
                "role": "system",
                "content": "You are analyzing conversation turns.",
            },
            {"role": "user", "content": "Hey there I was wondering"},
        ],
    )

    turn_state3 = response3.result["choices"][0]["message"]["content"]
    print("User: 'Hey there I was wondering'")
    print(f"Turn Detection Result: {turn_state3}")
    print(f"Response ID: {response3.result['id']}")
    print(f"Tokens Used: {response3.result['usage']['total_tokens']}")
    print(f"Run Time: {response3.run_time_ms:.2f}ms")

    # Test Case 4: Multi-turn conversation
    print("\n[Test 4] Multi-turn conversation:")
    response4 = await client.chat.completions.create(
        model="ten-turn-detection",
        messages=[
            {"role": "user", "content": "What is a mistral?"},
            {
                "role": "assistant",
                "content": "A mistral is a type of cold, dry wind.",
            },
            {"role": "user", "content": "How does the mistral wind form?"},
        ],
    )

    turn_state4 = response4.result["choices"][0]["message"]["content"]
    print("User: 'How does the mistral wind form?'")
    print(f"Turn Detection Result: {turn_state4}")
    print(f"Response ID: {response4.result['id']}")
    print(f"Tokens Used: {response4.result['usage']['total_tokens']}")
    print(f"Run Time: {response4.run_time_ms:.2f}ms")

    # Test Case 5: Batch requests (running concurrently)
    print("\n[Test 5] Concurrent batch requests:")
    prompts = [
        "I need help with",
        "What is the weather like?",
        "Thank you very much!",
    ]

    tasks = [
        client.chat.completions.create(
            model="ten-turn-detection",
            messages=[{"role": "user", "content": prompt}],
        )
        for prompt in prompts
    ]

    responses = await asyncio.gather(*tasks)

    for i, (prompt, response) in enumerate(zip(prompts, responses), 1):
        turn_state = response.result["choices"][0]["message"]["content"]
        print(f"  {i}. '{prompt}' → {turn_state}")

    print("\n" + "=" * 60)
    print("All tests completed successfully!")
    print("=" * 60)


# Run the async test
if __name__ == "__main__":
    asyncio.run(test_turn_detection())
