#!/bin/bash

# 禁用所有 lvgl-runtime 版本中的 freetype 支持

set -e

echo "Disabling FreeType in lv_conf.h files..."

# 修复 lvgl-runtime 各个版本的 lv_conf.h
for version in v8.4.0 v9.2.2 v9.3.0 v9.4.0; do
    conf_file="lvgl-runtime/$version/lv_conf.h"
    
    if [ -f "$conf_file" ]; then
        echo "Processing $conf_file..."
        
        # 禁用 freetype
        sed -i 's/#define LV_USE_FREETYPE   1/#define LV_USE_FREETYPE   0/g' "$conf_file"
        
        echo "Fixed $conf_file"
    fi
done

echo "Done!"
