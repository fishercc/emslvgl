### 1. 对象管理函数 (约 10 个函数)
- lvglCreateScreen - 创建 LVGL 屏幕对象
- lvglCreateUserWidget - 创建用户控件对象
- lvglScreenLoad - 加载屏幕（带动画）
- lvglDeleteObject - 删除 LVGL 对象
- lvglDeleteObjectIndex - 通过索引删除对象
- lvglDeletePageFlowState - 删除页面 Flow State
### 2. 样式属性操作 (约 9 个函数)
- lvglObjGetStylePropColor - 获取样式颜色属性
- lvglObjGetStylePropNum - 获取样式数值属性
- lvglObjSetLocalStylePropColor - 设置本地样式颜色
- lvglObjSetLocalStylePropNum - 设置本地样式数值
- lvglObjSetLocalStylePropPtr - 设置本地样式指针
### 3. 字体管理 (约 8 个函数)
- BUILT_IN_FONTS - 内置字体数组（Montserrat 8-48）
- lvglGetBuiltinFontPtr - 根据名称获取内置字体
- lvglObjGetStylePropBuiltInFont - 获取内置字体索引
- lvglLoadFont - 加载字体文件
- lvglFreeFont - 释放字体
- lvglCreateFreeTypeFont - 创建 FreeType 字体
### 4. 样式对象管理 (约 8 个函数)
- lvglStyleCreate - 创建样式对象
- lvglStyleSetPropColor - 设置样式颜色
- lvglStyleSetPropNum - 设置样式数值
- lvglStyleDelete - 删除样式对象
- lvglObjAddStyle - 为对象添加样式
- lvglObjRemoveStyle - 从对象移除样式
### 5. 对象属性查询 (约 6 个函数)
- lvglGetObjRelX - 获取相对 X 坐标
- lvglGetObjRelY - 获取相对 Y 坐标
- lvglGetObjWidth - 获取对象宽度
- lvglGetObjHeight - 获取对象高度
### 6. 仪表控件 (约 8 个函数，仅 v8.x）
- onMeterTickLabelEventCallback - 仪表刻度标签事件回调
- lvglMeterIndicatorNeedleLineSetColor - 设置指针线颜色
- lvglMeterIndicatorScaleLinesSetColorStart - 设置刻度线起始颜色
- lvglMeterScaleSetMinorTickColor - 设置次刻度颜色
- lvglMeterScaleSetMajorTickColor - 设置主刻度颜色
### 7. 时间线动画 (约 3 个函数)
- lvglAddTimelineKeyframe - 添加时间线关键帧（支持 30 种缓动函数）
- lvglSetTimelinePosition - 设置时间线位置
- lvglClearTimeline - 清除时间线
### 8. 其他控件操作 (约 10 个函数)
- lvglLineSetPoints - 设置 Line 控件点
- lvglScrollTo - 滚动对象
- lvglGetScrollX/Y - 获取滚动位置
- lvglObjInvalidate - 使对象无效
- lvglGetTabName - 获取 Tab 标签名称
- lvglLedGetColor - 获取 LED 颜色
### 9. v8.x Slider 包装函数 (约 8 个函数)
- v8_lv_slider_get_min_value - 获取最小值
- v8_lv_slider_get_max_value - 获取最大值
- v8_lv_slider_set_range - 设置范围
- v8_lv_slider_set_value - 设置值
- v8_lv_slider_set_left_value - 设置左侧值（双滑块）
### 10. 辅助函数 (约 2 个函数)
- to_lvgl_color - 转换为 LVGL 颜色格式
- lvglCreateAnim - 创建动画对象