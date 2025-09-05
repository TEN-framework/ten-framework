out/linux/x64/tests/standalone/ten_utils_unit_test

out/linux/x64/tests/standalone/ten_runtime_unit_test

cd out/linux/x64/tests/standalone/ten_rust
./unit_test --nocapture --test-threads=1
./integration_test --nocapture --test-threads=1

cd ../ten_manager
./unit_test --nocapture --test-threads=1
./integration_test --nocapture --test-threads=1

cd ../../..
pytest -s tests/ten_runtime/integration/nodejs/

pytest -s tests/ten_runtime/integration/python/

pytest -s tests/ten_runtime/integration/go/

pytest -s tests/ten_runtime/integration/cpp/

pytest -s tests/ten_manager/