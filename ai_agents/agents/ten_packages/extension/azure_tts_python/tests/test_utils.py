import unittest
from pydantic import BaseModel
from ..utils import encrypting_serializer


class TestEncryptingSerializer(unittest.TestCase):
    """Test the encrypting_serializer utility function"""

    def test_encrypting_serializer_with_string(self):
        """Test encrypting serializer with string values"""

        class TestModel(BaseModel):
            secret_field: str
            public_field: str
            _encrypt_fields = encrypting_serializer("secret_field")

        model = TestModel(
            secret_field="my_secret_value", public_field="public_value"
        )

        json_output = model.model_dump_json()
        json_dict = model.model_dump()

        # Check that the encrypted field is encrypted in JSON
        self.assertIn("***", json_output)

        # Check that the original value is preserved in model_dump
        self.assertEqual(json_dict["secret_field"], "my_secret_value")
        self.assertEqual(json_dict["public_field"], "public_value")

    def test_encrypting_serializer_with_multiple_fields(self):
        """Test encrypting serializer with multiple fields"""

        class TestModel(BaseModel):
            secret_field1: str
            secret_field2: str
            public_field: str
            _encrypt_fields = encrypting_serializer(
                "secret_field1", "secret_field2"
            )

        model = TestModel(
            secret_field1="first_secret",
            secret_field2="second_secret",
            public_field="public_value",
        )

        json_output = model.model_dump_json()

        # Check that both secret fields are encrypted
        self.assertIn("fi***et", json_output)
        self.assertIn("se***et", json_output)
        self.assertNotIn("first_secret", json_output)
        self.assertNotIn("second_secret", json_output)

    def test_encrypting_serializer_with_none_value(self):
        """Test encrypting serializer with None values"""

        class TestModel(BaseModel):
            secret_field: str | None
            _encrypt_fields = encrypting_serializer("secret_field")

        model = TestModel(secret_field=None)

        json_output = model.model_dump_json()

        # Check that None is handled correctly
        self.assertIn("null", json_output)

    def test_encrypting_serializer_with_empty_string(self):
        """Test encrypting serializer with empty string"""

        class TestModel(BaseModel):
            secret_field: str
            _encrypt_fields = encrypting_serializer("secret_field")

        model = TestModel(secret_field="")

        json_output = model.model_dump_json()

        # Check that empty string is handled correctly
        self.assertIn("***", json_output)

    def test_encrypting_serializer_with_short_string(self):
        """Test encrypting serializer with short string (less than 10 characters)"""

        class TestModel(BaseModel):
            secret_field: str
            _encrypt_fields = encrypting_serializer("secret_field")

        model = TestModel(secret_field="short")

        json_output = model.model_dump_json()

        # For short strings, step should be 1
        self.assertIn("s***t", json_output)

    def test_encrypting_serializer_with_long_string(self):
        """Test encrypting serializer with long string (more than 25 characters)"""

        class TestModel(BaseModel):
            secret_field: str
            _encrypt_fields = encrypting_serializer("secret_field")

        model = TestModel(
            secret_field="very_long_secret_value_that_exceeds_normal_length"
        )

        json_output = model.model_dump_json()

        # For long strings, step should be capped at 5
        self.assertIn("very_***ength", json_output)

    def test_encrypting_serializer_encryption_pattern(self):
        """Test the encryption pattern follows the expected format"""

        class TestModel(BaseModel):
            secret_field: str
            _encrypt_fields = encrypting_serializer("secret_field")

        test_cases = [
            "hello",
            "helloword",
            "helloworldtest",
            "verylongstringvalue",
            "extremelylongstringvalue",
        ]

        for input_str in test_cases:
            with self.subTest(input_str=input_str):
                model = TestModel(secret_field=input_str)
                json_output = model.model_dump_json()
                self.assertNotIn(input_str, json_output)

    def test_encrypting_serializer_preserves_original_data(self):
        """Test that encrypting serializer preserves original data in model_dump"""

        class TestModel(BaseModel):
            secret_field: str
            public_field: str
            _encrypt_fields = encrypting_serializer("secret_field")

        original_secret = "my_secret_value"
        model = TestModel(
            secret_field=original_secret, public_field="public_value"
        )

        # Check that model_dump preserves original values
        dumped_data = model.model_dump()
        self.assertEqual(dumped_data["secret_field"], original_secret)
        self.assertEqual(dumped_data["public_field"], "public_value")
