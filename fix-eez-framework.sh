#!/bin/bash

# 修复 eez-framework 的诊断宏定义问题
# Emscripten 的 Clang 不总是定义 __clang__ 宏

set -e

echo "Fixing eez-framework conf-internal.h for Emscripten..."

conf_file="eez-framework/src/eez/conf-internal.h"

if [ -f "$conf_file" ]; then
    # 备份原文件
    cp "$conf_file" "$conf_file.bak"
    
    # 修改检测逻辑，添加 Emscripten 检测
    # Emscripten 使用 Clang，所以应该使用 Clang 风格的诊断 pragma
    sed -i 's/\/\* Detect Clang \*\//\/* Detect Clang or Emscripten (Clang-based) *\//' "$conf_file"
    sed -i 's/#if defined(__clang__)/#if defined(__clang__) || defined(__EMSCRIPTEN__)/' "$conf_file"
    
    echo "Fixed $conf_file"
else
    echo "Error: $conf_file not found"
    exit 1
fi

echo "Done!"
