EXTRA_ARGS="is_clang=false log_level=1 enable_serialized_actions=true ten_enable_serialized_rust_action=true ten_rust_enable_gen_cargo_config=false ten_enable_cargo_clean=true ten_enable_go_lint=true ten_enable_rust_incremental_build=false ten_manager_enable_frontend=false ten_enable_integration_tests_prebuilt=false ten_enable_ffmpeg_extensions=true"
export PATH=$(pwd)/core/ten_gn:/usr/local/go/bin:/root/go/bin:/root/.cargo/bin:$PATH
tgn gen linux x64 debug -- $EXTRA_ARGS
tgn build linux x64 debug
