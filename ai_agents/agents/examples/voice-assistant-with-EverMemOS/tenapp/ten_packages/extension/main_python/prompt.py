"""
Prompt templates for the voice assistant with PowerMem.
Centralized location for all prompt strings used in the extension.
"""

# Template for context message with related memories
CONTEXT_MESSAGE_WITH_MEMORY_TEMPLATE = """Here’s what I remember from our past conversations that might be relevant:

{related_memory}

Now the user is asking: {user_query}

Please respond naturally, as if you’re continuing our conversation. Reference the memories when relevant, but keep it conversational and helpful."""

# Template for personalized greeting generation
PERSONALIZED_GREETING_TEMPLATE = """You are a friendly and helpful voice assistant. Based on the following memory summary of previous conversations with this user, generate a warm, personalized greeting (2-3 sentences maximum). Reference specific details from the memories naturally, but keep it concise and friendly.

If the memory summary contains information about the user’s location/region, please respond in the most commonly used language of that region.

Memory Summary:
{memory_summary}

Generate a personalized greeting:"""
