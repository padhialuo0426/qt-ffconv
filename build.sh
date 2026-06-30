#!/usr/bin/env bash
# 一键构建脚本
set -e
QT_DIR="$HOME/Qt/6.11.1/gcc_64"
export PATH="$QT_DIR/bin:$PATH"

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$QT_DIR"
cmake --build build
echo "✅ 构建完成： $(pwd)/build/ffmpeg-qt6"
