# main.c 文件分析

## 文件概述

`main.c` 是 LVGL Runtime 的主入口文件，负责初始化 LVGL 图形库、设置显示驱动、输入设备驱动，并提供与 JavaScript 端的交互接口。该文件支持 LVGL v8.x 和 v9.x 两个版本。

## 目录结构

```
main.c
├── 头文件和宏定义
├── 显示驱动
│   └── my_driver_flush() - 显示刷新回调
├── 输入设备驱动
│   ├── my_mouse_read() - 鼠标读取
│   ├── my_keyboard_read() - 键盘读取
│   └── my_mousewheel_read() - 滚轮读取
├── 文件系统驱动
│   ├── my_ready_cb() - 就绪回调
│   ├── my_open_cb() - 打开文件
│   ├── my_close_cb() - 关闭文件
│   ├── my_read_cb() - 读取文件
│   ├── my_seek_cb() - 文件定位
│   └── my_tell_cb() - 获取位置
├── HAL 初始化
│   └── hal_init() - 硬件抽象层初始化
├── 主循环和初始化
│   ├── init() - 初始化函数
│   └── mainLoop() - 主循环
├── JavaScript 交互接口
│   ├── getSyncedBuffer() - 获取同步缓冲区
│   ├── onPointerEvent() - 指针事件
│   ├── onMouseWheelEvent() - 滚轮事件
│   └── onKeyPressed() - 按键事件
└── 符号生成
    ├── WidgetInfo[] - 控件信息表
    ├── FlagInfo[] - 标志信息表
    └── dump_custom_styles() - 导出样式符号
```

## 1. 头文件和宏定义

### 1.1 包含的头文件

```c
#include <assert.h>      // 断言
#include <stdlib.h>      // 标准库
#include <stdio.h>       // 输入输出
#include <memory.h>      // 内存操作
#include <unistd.h>      // UNIX 标准符号
#include <math.h>        // 数学函数
#include <emscripten.h>  // Emscripten API

#include "lvgl/lvgl.h"   // LVGL 图形库
#include "src/flow.h"     // Flow 运行时
```

### 1.2 宏定义

```c
// Emscripten 端口 API 宏，防止函数被优化掉
#define EM_PORT_API(rettype) rettype EMSCRIPTEN_KEEPALIVE

// 未使用参数标记
#define EEZ_UNUSED(x) (void)(x)
```

## 2. 全局变量

### 2.1 显示相关

```c
int hor_res;              // 水平分辨率
int ver_res;              // 垂直分辨率
uint32_t *display_fb;      // 显示帧缓冲区
bool display_fb_dirty;     // 帧缓冲区脏标记
```

### 2.2 鼠标状态

```c
static int mouse_x = 0;        // 鼠标 X 坐标
static int mouse_y = 0;        // 鼠标 Y 坐标
static int mouse_pressed = 0;   // 鼠标按下状态
```

### 2.3 键盘缓冲区

```c
#define KEYBOARD_BUFFER_SIZE 1
static uint32_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];  // 键盘缓冲区
static uint32_t keyboard_buffer_index = 0;           // 缓冲区索引
static bool keyboard_pressed = false;                 // 键盘按下状态
```

### 2.4 滚轮状态

```c
static int mouse_wheel_delta = 0;      // 滚轮增量
static int mouse_wheel_pressed = 0;    // 滚轮按下状态
```

### 2.5 输入设备

```c
lv_indev_t *encoder_indev;   // 编码器输入设备（滚轮）
lv_indev_t *keyboard_indev; // 键盘输入设备
```

### 2.6 初始化状态

```c
bool initialized = false;  // LVGL 是否已初始化
```

## 3. 显示驱动

### 3.1 显示刷新回调函数

**函数签名**:
```c
#if LVGL_VERSION_MAJOR >= 9
void my_driver_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
#else
void my_driver_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
#endif
```

**功能**:
- 将 LVGL 渲染的图像数据复制到显示帧缓冲区
- 执行颜色空间转换（BGR -> RGB）
- 标记帧缓冲区为脏状态

**实现细节**:
1. **边界检查**: 检查渲染区域是否在屏幕范围内
2. **颜色转换**: 将 LVGL 的 BGR 格式转换为 RGB 格式
3. **内存拷贝**: 逐像素拷贝数据到帧缓冲区
4. **完成通知**: 调用 `lv_disp_flush_ready()` 通知 LVGL 刷新完成

**关键代码**:
```c
// bgr -> rgb
*dst++ = src[2];  // R
*dst++ = src[1];  // G
*dst++ = src[0];  // B
*dst++ = src[3];  // Alpha
```

## 4. 输入设备驱动

### 4.1 鼠标读取函数

**函数签名**:
```c
#if LVGL_VERSION_MAJOR >= 9
void my_mouse_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
#else
void my_mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
#endif
```

**功能**: 将全局鼠标状态传递给 LVGL

**数据结构**:
- `data->point.x` - 鼠标 X 坐标
- `data->point.y` - 鼠标 Y 坐标
- `data->state` - 按下/释放状态

### 4.2 键盘读取函数

**函数签名**:
```c
#if LVGL_VERSION_MAJOR >= 9
void my_keyboard_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
#else
void my_keyboard_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
#endif
```

**功能**: 从键盘缓冲区读取按键事件

**工作流程**:
1. 如果之前有按键按下，发送释放事件
2. 如果缓冲区有数据，发送按下事件并读取按键码

### 4.3 滚轮读取函数

**函数签名**:
```c
#if LVGL_VERSION_MAJOR >= 9
void my_mousewheel_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
#else
void my_mousewheel_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
#endif
```

**功能**: 读取滚轮增量并传递给 LVGL

**特殊处理**:
```c
if (yMouseWheel >= 100 || yMouseWheel <= -100) {
    yMouseWheel /= 100;  // 归一化大值
}
```

## 5. 文件系统驱动

### 5.1 文件结构

```c
typedef struct {
    uint8_t *ptr;    // 文件数据指针
    uint32_t pos;    // 当前读取位置
} my_file_t;
```

### 5.2 文件系统回调函数

#### 5.2.1 就绪回调

```c
bool my_ready_cb(lv_fs_drv_t * drv)
```
- 返回 `true` 表示驱动始终就绪

#### 5.2.2 打开文件

```c
void *my_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
```
- 将路径字符串转换为内存地址指针
- 初始化文件位置为 0

#### 5.2.3 关闭文件

```c
lv_fs_res_t my_close_cb(lv_fs_drv_t * drv, void * file_p)
```
- 释放文件结构内存

#### 5.2.4 读取文件

```c
lv_fs_res_t my_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
```
- 从内存位置拷贝数据到缓冲区
- 更新文件读取位置

#### 5.2.5 文件定位

```c
lv_fs_res_t my_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
```
- 支持 `LV_FS_SEEK_SET`（绝对位置）
- 支持 `LV_FS_SEEK_CUR`（相对位置）

#### 5.2.6 获取位置

```c
lv_fs_res_t my_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
```
- 返回当前文件读取位置

### 5.3 文件系统驱动初始化

```c
static void init_fs_driver()
```

**驱动配置**:
- 驱动器字母: `'M'` (Memory drive)
- 缓存大小: `my_cache_size`
- 实现的回调: ready, open, close, read, seek, tell
- 未实现的回调: write, dir_open, dir_read, dir_close

## 6. HAL 初始化

### 6.1 hal_init() 函数

**函数签名**:
```c
EM_PORT_API(void) hal_init(bool is_editor)
```

**参数**:
- `is_editor`: 是否为编辑器模式（编辑器模式不初始化输入设备）

**功能**:

#### 6.1.1 分配显示缓冲区

```c
display_fb = (uint32_t *)malloc(sizeof(uint32_t) * hor_res * ver_res);
memset(display_fb, 0x44, hor_res * ver_res * sizeof(uint32_t));
```

#### 6.1.2 LVGL v9.x 显示初始化

```c
lv_display_t *disp = lv_display_create(hor_res, ver_res);
lv_display_set_flush_cb(disp, my_driver_flush);

uint8_t *buf1 = malloc(sizeof(uint32_t) * hor_res * ver_res);
uint8_t *buf2 = NULL;
lv_display_set_buffers(disp, buf1, buf2, sizeof(uint32_t) * hor_res * ver_res, LV_DISPLAY_RENDER_MODE_PARTIAL);
```

#### 6.1.3 LVGL v8.x 显示初始化

```c
static lv_disp_draw_buf_t disp_buf1;
lv_color_t *buf1_1 = malloc(sizeof(lv_color_t) * hor_res * ver_res);
lv_disp_draw_buf_init(&disp_buf1, buf1_1, NULL, hor_res * ver_res);

static lv_disp_drv_t disp_drv;
lv_disp_drv_init(&disp_drv);
disp_drv.draw_buf = &disp_buf1;
disp_drv.flush_cb = my_driver_flush;
disp_drv.hor_res = hor_res;
disp_drv.ver_res = ver_res;
lv_disp_drv_register(&disp_drv);
```

#### 6.1.4 输入设备初始化（非编辑器模式）

**鼠标设备**:
```c
lv_indev_t *indev1 = lv_indev_create();
lv_indev_set_type(indev1, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev1, my_mouse_read);
```

**键盘设备**:
```c
keyboard_indev = lv_indev_create();
lv_indev_set_type(keyboard_indev, LV_INDEV_TYPE_KEYPAD);
lv_indev_set_read_cb(keyboard_indev, my_keyboard_read);
```

**滚轮设备**:
```c
encoder_indev = lv_indev_create();
lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
lv_indev_set_read_cb(encoder_indev, my_mousewheel_read);
```

### 6.2 设置输入设备组

```c
EM_PORT_API(void) lvglSetEncoderGroup(lv_group_t *group)
EM_PORT_API(void) lvglSetKeyboardGroup(lv_group_t *group)
```

为编码器和键盘设备设置焦点组，用于键盘导航。

## 7. 主循环和初始化

### 7.1 init() 函数

**函数签名**:
```c
EM_PORT_API(void) init(
    uint32_t wasmModuleId,
    uint32_t debuggerMessageSubsciptionFilter,
    uint8_t *assets,
    uint32_t assetsSize,
    uint32_t displayWidth,
    uint32_t displayHeight,
    bool darkTheme,
    uint32_t timeZone,
    bool screensLifetimeSupport
)
```

**参数说明**:
- `wasmModuleId`: WASM 模块 ID
- `debuggerMessageSubsciptionFilter`: 调试器消息订阅过滤器
- `assets`: 资源数据指针
- `assetsSize`: 资源数据大小
- `displayWidth`, `displayHeight`: 显示分辨率
- `darkTheme`: 是否使用深色主题
- `timeZone`: 时区
- `screensLifetimeSupport`: 是否支持屏幕生命周期

**功能**:

1. **判断模式**: `assetsSize == 0` 表示编辑器模式
2. **设置分辨率**:
   ```c
   hor_res = displayWidth;
   ver_res = displayHeight;
   ```
3. **初始化 LVGL**: `lv_init()`
4. **初始化 HAL**: `hal_init(is_editor)`
5. **设置主题**:
   ```c
   lv_theme_t *theme = lv_theme_default_init(
       disp,
       lv_palette_main(LV_PALETTE_BLUE),
       lv_palette_main(LV_PALETTE_RED),
       darkTheme,
       LV_FONT_DEFAULT
   );
   lv_disp_set_theme(disp, theme);
   ```
6. **初始化 Flow 运行时**（非编辑器模式）:
   ```c
   flowInit(wasmModuleId, debuggerMessageSubsciptionFilter, assets, assetsSize, darkTheme, timeZone, screensLifetimeSupport);
   ```
7. **初始化时间戳**（LVGL v9.x）:
   ```c
   g_prevTick = (uint32_t)emscripten_get_now();
   ```
8. **标记已初始化**: `initialized = true`

### 7.2 mainLoop() 函数

**函数签名**:
```c
EM_PORT_API(bool) mainLoop()
```

**返回值**: `true` 表示继续运行，`false` 表示停止

**功能**:

1. **检查初始化状态**: 未初始化则直接返回
2. **更新时间戳**（LVGL v9.x）:
   ```c
   uint32_t currentTick = (uint32_t)emscripten_get_now();
   lv_tick_inc(currentTick - g_prevTick);
   g_prevTick = currentTick;
   ```
3. **调用 LVGL 任务处理器**: `lv_task_handler()`
4. **调用 Flow Tick**: `flowTick()`

## 8. JavaScript 交互接口

### 8.1 获取同步缓冲区

**函数签名**:
```c
EM_PORT_API(uint8_t*) getSyncedBuffer()
```

**功能**: 返回显示帧缓冲区指针，如果缓冲区未修改则返回 NULL

**实现**:
```c
if (display_fb_dirty) {
    display_fb_dirty = false;
    return (uint8_t*)display_fb;
}
return NULL;
```

### 8.2 检查 RTL 方向

**函数签名**:
```c
EM_PORT_API(bool) isRTL()
```

**功能**: 返回 `false`，表示不支持从右到左布局

### 8.3 指针事件

**函数签名**:
```c
EM_PORT_API(void) onPointerEvent(int x, int y, int pressed)
```

**功能**: 更新鼠标位置和按下状态

**边界限制**:
```c
if (x < 0) x = 0;
else if (x >= hor_res) x = hor_res - 1;
```

### 8.4 滚轮事件

**函数签名**:
```c
EM_PORT_API(void) onMouseWheelEvent(double yMouseWheel, int pressed)
```

**功能**: 更新滚轮增量和按下状态

**特殊处理**:
```c
if (yMouseWheel >= 100 || yMouseWheel <= -100) {
    yMouseWheel /= 100;  // 归一化
}
mouse_wheel_delta = round(yMouseWheel);
```

### 8.5 按键事件

**函数签名**:
```c
EM_PORT_API(void) onKeyPressed(uint32_t key)
```

**功能**: 将按键码存入键盘缓冲区

**缓冲区管理**:
```c
if (keyboard_buffer_index < KEYBOARD_BUFFER_SIZE) {
    keyboard_buffer[keyboard_buffer_index++] = key;
}
```

## 9. 符号生成系统

### 9.1 符号字符串结构

```c
typedef struct SymbolsString {
    char *ptr;          // 字符串指针
    unsigned size;       // 当前大小
    unsigned allocated;   // 分配的大小
} SymbolsString;
```

**初始化**:
```c
#define SYMBOLS_STRING_INIT_ALLOCATED 1024 * 1024
static SymbolsString g_symbolsString;
```

### 9.2 符号追加函数

**函数签名**:
```c
void symbols_append(const char *format, ...)
```

**功能**: 使用可变参数格式化字符串并追加到符号字符串

**实现细节**:
1. 使用 `vsnprintf` 计算所需空间
2. 检查是否有足够空间（使用 `assert`）
3. 实际写入格式化字符串
4. 更新字符串大小

### 9.3 控件适配函数

#### 9.3.1 Spinner 适配

```c
lv_obj_t *lv_spinner_create_adapt(lv_obj_t *parentObj)
```

**版本差异**:
- LVGL v9.x: `lv_spinner_create(parentObj)`
- LVGL v8.x: `lv_spinner_create(parentObj, 1000, 60)`

#### 9.3.2 Colorwheel 适配（仅 v8.x）

```c
lv_obj_t *lv_colorwheel_create_adapt(lv_obj_t *parentObj)
```

**实现**: `lv_colorwheel_create(parentObj, false)`

#### 9.3.3 QRCode 适配

```c
lv_obj_t *lv_qrcode_create_adapt(lv_obj_t *parentObj)
```

**版本差异**:
- LVGL v9.x: `lv_qrcode_create(parentObj)`
- LVGL v8.x: 需要指定颜色参数

### 9.4 控件信息表

```c
typedef struct {
    const char *name;                      // 控件名称
    lv_obj_t *(*create)(lv_obj_t *);      // 创建函数指针
} WidgetInfo;
```

**支持的控件**:

| 控件名称 | 创建函数 | 版本支持 |
|---------|---------|---------|
| Screen | lv_obj_create | 全部 |
| Label | lv_label_create | 全部 |
| Button | lv_btn_create | 全部 |
| Panel | lv_obj_create | 全部 |
| Image | lv_img_create | 全部 |
| Slider | lv_slider_create | 全部 |
| Roller | lv_roller_create | 全部 |
| Switch | lv_switch_create | 全部 |
| Bar | lv_bar_create | 全部 |
| Dropdown | lv_dropdown_create | 全部 |
| Arc | lv_arc_create | 全部 |
| Spinner | lv_spinner_create_adapt | 全部 |
| Checkbox | lv_checkbox_create | 全部 |
| Textarea | lv_textarea_create | 全部 |
| Keyboard | lv_keyboard_create | 全部 |
| Chart | lv_chart_create | 全部 |
| Calendar | lv_calendar_create | 全部 |
| Scale | lv_scale_create | v9.x |
| Colorwheel | lv_colorwheel_create_adapt | v8.x |
| ImageButton | lv_imgbtn_create | v8.x |
| Meter | lv_meter_create | v8.x |
| QRCode | lv_qrcode_create_adapt | 全部 |
| Spinbox | lv_spinbox_create | 全部 |

### 9.5 标志信息表

```c
typedef struct {
    const char *name;  // 标志名称
    int code;         // 标志代码
} FlagInfo;
```

**支持的对象标志**:

| 标志名称 | 代码 | 说明 |
|---------|------|------|
| HIDDEN | LV_OBJ_FLAG_HIDDEN | 隐藏 |
| CLICKABLE | LV_OBJ_FLAG_CLICKABLE | 可点击 |
| CLICK_FOCUSABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE | 可点击聚焦 |
| CHECKABLE | LV_OBJ_FLAG_CHECKABLE | 可选中 |
| SCROLLABLE | LV_OBJ_FLAG_SCROLLABLE | 可滚动 |
| SCROLL_ELASTIC | LV_OBJ_FLAG_SCROLL_ELASTIC | 弹性滚动 |
| SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_MOMENTUM | 惯性滚动 |
| SCROLL_ONE | LV_OBJ_FLAG_SCROLL_ONE | 单次滚动 |
| SCROLL_CHAIN_HOR | LV_OBJ_FLAG_SCROLL_CHAIN_HOR | 水平链式滚动 |
| SCROLL_CHAIN_VER | LV_OBJ_FLAG_SCROLL_CHAIN_VER | 垂直链式滚动 |
| SCROLL_CHAIN | LV_OBJ_FLAG_SCROLL_CHAIN | 链式滚动 |
| SCROLL_ON_FOCUS | LV_OBJ_FLAG_SCROLL_ON_FOCUS | 聚焦时滚动 |
| SCROLL_WITH_ARROW | LV_OBJ_FLAG_SCROLL_WITH_ARROW | 箭头滚动 |
| SNAPPABLE | LV_OBJ_FLAG_SNAPPABLE | 可吸附 |
| PRESS_LOCK | LV_OBJ_FLAG_PRESS_LOCK | 按下锁定 |
| EVENT_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE | 事件冒泡 |
| GESTURE_BUBBLE | LV_OBJ_FLAG_GESTURE_BUBBLE | 手势冒泡 |
| ADV_HITTEST | LV_OBJ_FLAG_ADV_HITTEST | 高级命中测试 |
| IGNORE_LAYOUT | LV_OBJ_FLAG_IGNORE_LAYOUT | 忽略布局 |
| FLOATING | LV_OBJ_FLAG_FLOATING | 浮动 |
| OVERFLOW_VISIBLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE | 溢出可见 |

### 9.6 导出控件标志信息

**函数签名**:
```c
void dump_widget_flags_info(WidgetInfo *info, lv_obj_t *obj, bool last)
```

**功能**: 将控件的标志信息导出为 JSON 格式

**输出格式**:
```json
"控件名": "标志1 | 标志2 | ..."
```

**函数签名**:
```c
void dump_widgets_flags_info()
```

**功能**: 遍历所有控件并导出其标志信息

### 9.7 导出样式信息

#### 9.7.1 样式追加函数

```c
void style_append(const char *name, int code)
```
输出: `{"name": "样式名", "code": 代码},`

```c
void style_append_with_new_name(const char *name, int code, const char *new_name)
```
输出: `{"name": "样式名", "code": 代码, "new_name": "新名称"},`

```c
void style_append_last(const char *name, int code)
```
输出: `{"name": "样式名", "code": 代码}`（最后一个元素）

```c
void style_append_undefined(const char *name)
```
输出: `{"name": "样式名", "code": null}`

#### 9.7.2 导出样式函数

**函数签名**:
```c
void dump_custom_styles()
```

**导出的样式属性**:

**Group 0 - 尺寸和位置**:
- LV_STYLE_WIDTH
- LV_STYLE_HEIGHT
- LV_STYLE_LENGTH (仅 v9.x)
- LV_STYLE_MIN_WIDTH
- LV_STYLE_MAX_WIDTH
- LV_STYLE_MIN_HEIGHT
- LV_STYLE_MAX_HEIGHT
- LV_STYLE_X
- LV_STYLE_Y
- LV_STYLE_ALIGN

**Group 1 - 内边距**:
- LV_STYLE_PAD_TOP
- LV_STYLE_PAD_BOTTOM
- LV_STYLE_PAD_LEFT
- LV_STYLE_PAD_RIGHT
- LV_STYLE_PAD_ROW
- LV_STYLE_PAD_COLUMN

**Group 2 - 背景和边框**:
- LV_STYLE_BG_COLOR
- LV_STYLE_BG_OPA
- LV_STYLE_BG_GRAD_COLOR
- LV_STYLE_BG_GRAD_DIR
- LV_STYLE_BG_MAIN_STOP
- LV_STYLE_BG_GRAD_STOP
- LV_STYLE_BG_MAIN_OPA
- LV_STYLE_BG_GRAD_OPA
- LV_STYLE_BG_GRAD
- LV_STYLE_BG_IMG_SRC
- LV_STYLE_BG_IMG_OPA
- LV_STYLE_BG_IMG_RECOLOR
- LV_STYLE_BG_IMG_RECOLOR_OPA
- LV_STYLE_BG_IMG_TILED
- LV_STYLE_BORDER_COLOR
- LV_STYLE_BORDER_OPA
- LV_STYLE_BORDER_WIDTH
- LV_STYLE_BORDER_SIDE
- LV_STYLE_BORDER_POST
- LV_STYLE_OUTLINE_WIDTH
- LV_STYLE_OUTLINE_COLOR
- LV_STYLE_OUTLINE_OPA
- LV_STYLE_OUTLINE_PAD
- LV_STYLE_SHADOW_WIDTH
- LV_STYLE_SHADOW_OFS_X
- LV_STYLE_SHADOW_OFS_Y
- LV_STYLE_SHADOW_SPREAD
- LV_STYLE_SHADOW_COLOR
- LV_STYLE_SHADOW_OPA

**Group 3 - 文本**:
- LV_STYLE_TEXT_COLOR
- LV_STYLE_TEXT_OPA
- LV_STYLE_TEXT_FONT
- LV_STYLE_TEXT_LETTER_SPACE
- LV_STYLE_TEXT_LINE_SPACE
- LV_STYLE_TEXT_DECOR
- LV_STYLE_TEXT_ALIGN

**Group 4 - 其他**:
- LV_STYLE_OPA
- LV_STYLE_COLOR_FILTER_DSC
- LV_STYLE_COLOR_FILTER_OPA
- LV_STYLE_ANIM
- LV_STYLE_ANIM_DURATION
- LV_STYLE_TRANSITION
- LV_STYLE_BLEND_MODE
- LV_STYLE_RADIUS
- LV_STYLE_CLIP_CORNER

## 10. 版本兼容性

### 10.1 LVGL v8.x vs v9.x 差异

| 功能 | v8.x | v9.x |
|-----|------|------|
| 显示驱动 | lv_disp_drv_t | lv_display_t |
| 输入设备 | lv_indev_drv_t | lv_indev_t |
| 刷新回调 | my_driver_flush(lv_disp_drv_t *, ...) | my_driver_flush(lv_display_t *, ...) |
| 显示缓冲区 | lv_disp_draw_buf_t | 直接传递缓冲区指针 |
| Spinner 创建 | lv_spinner_create(parent, 1000, 60) | lv_spinner_create(parent) |
| QRCode 创建 | 需要颜色参数 | 不需要颜色参数 |
| Colorwheel | 支持 | 不支持 |
| Meter | 支持 | 不支持 |
| Scale | 不支持 | 支持 |
| ImageButton | 支持 | 不支持 |

### 10.2 内存管理

| 操作 | v8.x | v9.x |
|-----|------|------|
| 分配 | lv_mem_alloc() | lv_malloc() |
| 释放 | lv_mem_free() | lv_free() |
| 颜色转换 | lv_color_to32() | lv_color_to_u32() |

### 10.3 文件系统

| 功能 | v8.x | v9.x |
|-----|------|------|
| 驱动结构 | struct _lv_fs_drv_t | lv_fs_drv_t |
| v9.3+ | lv_fs_drv_t | lv_fs_drv_t |

## 11. 数据流图

```
JavaScript 端
    ↓
init() 初始化
    ↓
hal_init() 初始化 HAL
    ├─→ 创建显示驱动
    ├─→ 创建输入设备（非编辑器）
    └─→ 初始化文件系统
    ↓
mainLoop() 主循环
    ├─→ lv_tick_inc() 更新时间戳
    ├─→ lv_task_handler() LVGL 任务处理
    └─→ flowTick() Flow 运行时 Tick
    ↓
getSyncedBuffer() 获取帧缓冲区
    ↓
JavaScript 渲染
    ↓
onPointerEvent() / onMouseWheelEvent() / onKeyPressed()
    ↓
更新输入设备状态
```

## 12. 关键特性总结

### 12.1 显示系统
- ✅ 双缓冲支持（v9.x）
- ✅ 部分渲染模式
- ✅ BGR 到 RGB 颜色转换
- ✅ 脏标记优化

### 12.2 输入系统
- ✅ 鼠标指针输入
- ✅ 键盘输入（带缓冲区）
- ✅ 滚轮/编码器输入
- ✅ 输入设备组管理

### 12.3 文件系统
- ✅ 内存文件系统（M: 盘）
- ✅ 支持读取、定位、查询
- ✅ 不支持写入和目录操作

### 12.4 版本兼容
- ✅ 同时支持 LVGL v8.x 和 v9.x
- ✅ 编译时版本检测
- ✅ API 适配层

### 12.5 JavaScript 交互
- ✅ EM_PORT_API 导出接口
- ✅ 事件回调机制
- ✅ 帧缓冲区同步

### 12.6 符号生成
- ✅ 控件信息导出
- ✅ 样式属性导出
- ✅ 对象标志导出
- ✅ JSON 格式输出

## 13. 使用场景

### 13.1 编辑器模式
- 不初始化输入设备
- 用于可视化编辑器预览

### 13.2 运行时模式
- 初始化所有输入设备
- 加载 Flow 资源
- 支持用户交互

### 13.3 调试模式
- 支持调试器消息订阅
- 支持断点和变量监视

## 14. 性能优化

### 14.1 显示优化
- 脏标记避免不必要的传输
- 部分渲染减少内存占用
- 双缓冲减少闪烁

### 14.2 输入优化
- 键盘缓冲区减少事件丢失
- 滚轮归一化处理

### 14.3 内存优化
- 单缓冲区分配（v8.x）
- 可选双缓冲区（v9.x）
- 文件系统零拷贝

## 15. 安全性考虑

### 15.1 边界检查
- 鼠标坐标限制在屏幕范围内
- 渲染区域边界检查

### 15.2 断言检查
- 符号字符串空间检查
- 缓冲区溢出保护

### 15.3 空指针检查
- 文件指针有效性检查
- 控件创建成功检查

## 16. 总结

`main.c` 是 LVGL Runtime 的核心文件，实现了：

1. **完整的显示系统**: 支持双缓冲、颜色转换、脏标记
2. **多输入设备支持**: 鼠标、键盘、滚轮
3. **内存文件系统**: 用于资源加载
4. **版本兼容性**: 同时支持 LVGL v8.x 和 v9.x
5. **JavaScript 交互**: 完整的事件和同步接口
6. **符号生成系统**: 为 Studio 提供控件和样式信息

该文件是 EEZ Studio Web 端 GUI 模拟器的基础，提供了高性能、低延迟的图形渲染和用户交互能力。
