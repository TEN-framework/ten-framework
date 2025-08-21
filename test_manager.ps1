$env:PATH += ";$PWD/core/ten_gn"
tgn gen win x64 debug -- vs_version=2022 log_level=1 enable_serialized_actions=true ten_enable_serialized_rust_action=true ten_rust_enable_gen_cargo_config=false ten_enable_cargo_clean=true ten_enable_python_binding=false ten_enable_go_binding=false ten_enable_nodejs_binding=false ten_enable_rust_incremental_build=false ten_manager_enable_frontend=false ten_enable_integration_tests_prebuilt=false
tgn build win x64 debug
Set-Location "out/win/x64/tests/standalone/ten_manager"
./integration_test
