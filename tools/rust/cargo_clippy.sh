#!/bin/bash

cd core/src/ten_rust || exit 1
cargo clippy --tests -- -D warnings -W clippy::all
cargo clippy --release --tests -- -D warnings -W clippy::all

cd ../../..

cd core/src/ten_manager || exit 1
cargo clippy --tests -- -D warnings -W clippy::all
cargo clippy --release --tests -- -D warnings -W clippy::all
