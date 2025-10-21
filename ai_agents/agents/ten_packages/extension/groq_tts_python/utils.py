from typing import Callable, AsyncIterator, TypeVar
from pydantic import field_serializer
from functools import wraps
import logging
import asyncio


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
        if key is None:
            return "null"
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
    return field_serializer(*fields, when_used="json")(_encrypt)


T = TypeVar("T")


class RetryError(Exception):
    pass


def with_retry_context(
    max_retries: int = 3,
    retry_delay: float = 0.1,
    backoff_factor: float = 2.0,
    exceptions: tuple = (Exception,),
):
    """
    Decorator: Add retry mechanism to the function that returns AsyncIterator

    Args:
        max_retries: maximum number of retries
        retry_delay: initial retry delay (seconds)
        backoff_factor: backoff factor, the retry delay will be multiplied by this value
        exceptions: the types of exceptions to retry
    """

    def decorator(
        func: Callable[..., AsyncIterator[T]],
    ) -> Callable[..., AsyncIterator[T]]:
        @wraps(func)
        async def wrapper(*args, **kwargs) -> AsyncIterator[T]:
            current_retry_delay = retry_delay
            last_exception = None

            for attempt in range(max_retries + 1):
                try:
                    # create a generator to wrap the original AsyncIterator
                    async def retry_generator():
                        try:
                            async for item in func(*args, **kwargs):
                                yield item
                        except exceptions as e:
                            # if an exception occurs during iteration, re-raise it
                            raise e

                    # return the generator
                    async for item in retry_generator():
                        yield item

                    # if successful, exit the retry loop
                    return

                except exceptions as e:
                    last_exception = e
                    logging.warning(
                        f"Attempt {attempt + 1}/{max_retries + 1} failed for {func.__name__}: {e}"
                    )

                    if attempt < max_retries:
                        logging.info(
                            f"Retrying in {current_retry_delay} seconds..."
                        )
                        await asyncio.sleep(current_retry_delay)
                        current_retry_delay *= backoff_factor
                    else:
                        logging.error(
                            f"All {max_retries + 1} attempts failed for {func.__name__}"
                        )
                        raise RetryError(
                            f"Failed to execute {func.__name__} after {max_retries + 1} attempts. "
                            f"Last error: {last_exception} type: {type(last_exception)}"
                        ) from last_exception

        return wrapper

    return decorator
