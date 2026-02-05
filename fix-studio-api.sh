#!/bin/bash

# 修复 studio_api.cpp 中的 freetype 相关代码

set -e

echo "Fixing studio_api.cpp for FreeType disabled build..."

studio_api_file="lvgl-runtime/common/src/studio_api.cpp"

if [ -f "$studio_api_file" ]; then
    echo "Processing $studio_api_file..."
    
    # 创建临时文件
    temp_file=$(mktemp)
    
    # 使用 awk 处理文件，替换 lvglCreateFreeTypeFont 函数
    awk '
    /^EM_PORT_API\(void \*\) lvglCreateFreeTypeFont/ {
        in_function = 1
        print "EM_PORT_API(void *) lvglCreateFreeTypeFont(const char *filePath, int size, int renderMode, int style) {"
        print "#if LVGL_VERSION_MAJOR >= 9"
        print "    LV_LOG_ERROR(\"FreeType is not enabled in this build\");"
        print "    return 0;"
        print "#else"
        print "    LV_LOG_ERROR(\"FreeType is not enabled in this build\");"
        print "    return 0;"
        print "#endif"
        print "}"
        skip_until_closing_brace = 1
        next
    }
    
    skip_until_closing_brace && /^}$/ {
        skip_until_closing_brace = 0
        next
    }
    
    !skip_until_closing_brace {
        print
    }
    ' "$studio_api_file" > "$temp_file"
    
    # 替换原文件
    mv "$temp_file" "$studio_api_file"
    
    echo "Fixed $studio_api_file"
else
    echo "Error: $studio_api_file not found"
    exit 1
fi

echo "Done!"
