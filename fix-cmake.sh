#!/bin/bash

# 修复 CMakeLists.txt 中的 freetype 路径问题
# 这个脚本会移除硬编码的 freetype 路径，因为我们在编译时禁用了 freetype

set -e

echo "Fixing CMakeLists.txt files..."

# 修复 lvgl-runtime 各个版本的 CMakeLists.txt
for version in v8.4.0 v9.2.2 v9.3.0 v9.4.0; do
    cmake_file="lvgl-runtime/$version/CMakeLists.txt"
    
    if [ -f "$cmake_file" ]; then
        echo "Processing $cmake_file..."
        
        # 移除 freetype 链接选项
        sed -i 's/-L\/home\/mvladic\/freetype-2.14.1\/build -lfreetype//g' "$cmake_file"
        
        # 移除 freetype include 路径
        sed -i '/include_directories(\/home\/mvladic\/freetype-2.14.1\/include)/d' "$cmake_file"
        
        echo "Fixed $cmake_file"
    fi
done

echo "Done!"
