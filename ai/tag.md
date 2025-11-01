# SSML Tag Chunking Fix for Cartesia TTS

## Problem

When using inline SSML tags with decimal values (like `<speed ratio="1.2"/><volume ratio="1.5"/>`), the sentence chunker was breaking the tags into multiple fragments before sending them to TTS. This caused the TTS to speak the tags literally instead of interpreting them.

### Root Cause

The `parse_sentences()` function in helper.py splits LLM streaming responses into sentences for TTS. It was incorrectly splitting on the decimal point in attribute values like `ratio="1.2"` for two reasons:

1. **Streaming Chunk Boundaries**: The LLM streams text in small chunks. When a chunk ends with `<speed ratio="1.`, the decimal point looks like sentence-ending punctuation because:
   - The `1` is in the current chunk
   - The period `.` is in the current chunk
   - The `0` comes in the NEXT chunk
   - The original code checked if the period was between two digits in the same chunk, which failed

2. **Tag Detection Across Chunks**: The `inside_tag` flag wasn't tracking state from previous chunks, so tags that started in a previous chunk weren't recognized.

### Example Failure

LLM streams: `<speed ratio="1.2"/><volume ratio="1.5"/><emotion value="excited"/>Hello!`

Chunked incorrectly as:
- Chunk 1: `<speed ratio="1.`  ← Split here! (period treated as sentence end)
- Chunk 2: `2"/><volume ratio="1.`  ← Split here too!
- Chunk 3: `5"/><emotion value="excited"/>Hello!`

Sent to TTS as THREE separate sentences, causing it to speak the broken tags literally.

## Solution

Two changes to `parse_sentences()` in both:
- `/agents/examples/voice-assistant-advanced/tenapp/ten_packages/extension/main_python/helper.py`
- `/agents/ten_packages/extension/openai_llm2_python/helper.py`

### Fix 1: Track Tag State Across Chunks

```python
# OLD: Reset flag each time
inside_tag = False

# NEW: Check if sentence fragment has unclosed tag
inside_tag = '<' in sentence_fragment and '>' not in sentence_fragment
```

This preserves tag state when chunks split mid-tag.

### Fix 2: Skip Periods After Digits

```python
# OLD: Check if period is between two digits in current chunk
if char == '.' or char == '。':
    prev_is_digit = i > 0 and content[i-1].isdigit()
    next_is_digit = i < len(content) - 1 and content[i+1].isdigit()
    if prev_is_digit and next_is_digit:
        continue

# NEW: Skip any period that follows a digit (can't check next char in streaming)
if char == '.' or char == '。':
    curr_len = len(current_sentence)
    prev_is_digit = curr_len >= 2 and current_sentence[-2].isdigit()
    if prev_is_digit:
        continue
```

**Why this works**:
- In streaming, we can't reliably check the next character (it might be in the next chunk)
- If a period follows a digit, it's almost certainly a decimal number, not a sentence end
- This handles decimals across chunk boundaries: `ratio="1.` (chunk 1) + `0"` (chunk 2)

## Files Modified

1. `agents/examples/voice-assistant-advanced/tenapp/ten_packages/extension/main_python/helper.py`
   - Main control extension uses this for sentence chunking

2. `agents/ten_packages/extension/openai_llm2_python/helper.py`
   - Shared LLM extension also has parse_sentences (may be used by other graphs)

## Testing

Test with LLM prompt that includes:
```
Start each response with control tags: <speed ratio="1.0"/><volume ratio="1.0"/><emotion value="neutral"/> your sentence.
```

Verify in logs:
- Tags should NOT appear in TTS logs as broken fragments like `0"/><emotion...`
- Complete tags should be sent together in one TTS request
- TTS should apply the effects (speed, volume, emotion) instead of speaking the tags

## Related Configuration

The LLM prompt for `dgv2_flux_cartesiatts` graph includes:
- Speed ratio: 0.7 (slow) to 1.5 (fast), default 1.0
- Volume ratio: 0.5 (quiet) to 2.0 (loud), default 1.0
- Emotion values: neutral, angry, excited, content, sad, sympathetic, scared
- Laughter: [laughter] inline text
