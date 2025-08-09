from typing import Callable
from pydantic import field_serializer


def encrypting_serializer(*fields: str) -> Callable:
    """
    A factory function that creates a Pydantic serializer for specified fields 
    that encrypts them when serializing to JSON.

    Args:
        *fields: Field names that need encryption applied.

    Returns:
        A configured Pydantic field_serializer object.

    Example:
        class MyModel(BaseModel):
            secret_field: str
            another_secret_field: str
            _encrypt_fields = encrypting_serializer('secret_field', 'another_secret_field')

        model = MyModel(secret_field="my_secret_value", another_secret_field="another_secret_value")
        print(model.model_dump_json())  # Outputs encrypted JSON
    """
    def _encrypt(key: object) -> str:
        if hasattr(key, "__str__"):
            key = str(key)
        else:
            key = ""

        step = int(len(key) / 5)
        if step > 5:
            step = 5
        if step == 0:
            step = 1

        prefix = key[:step]
        suffix = key[-step:]

        return f"{prefix}***{suffix}"

    # field_serializer() returns a decorator that we can call directly
    # and pass our generic encryption function as a parameter.
    # `when_used='json'` ensures it only takes effect when calling model_dump_json().
    return field_serializer(*fields, when_used='json')(_encrypt)
