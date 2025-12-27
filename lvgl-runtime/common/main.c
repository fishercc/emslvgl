#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <unistd.h>
#include <math.h>
#include <emscripten.h>

#include "lvgl/lvgl.h"

#include "src/flow.h"

#define EM_PORT_API(rettype) rettype EMSCRIPTEN_KEEPALIVE

#define EEZ_UNUSED(x) (void)(x)

static void hal_init();

int hor_res;
int ver_res;

uint32_t *display_fb;
bool display_fb_dirty;

#if LVGL_VERSION_MAJOR >= 9
void my_driver_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map) {
#else
void my_driver_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p) {
#endif
    /*Return if the area is out the screen */
    if (area->x2 < 0 || area->y2 < 0 || area->x1 > hor_res - 1 || area->y1 > ver_res - 1) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    uint8_t *dst = (uint8_t *)&display_fb[area->y1 * hor_res + area->x1];
    uint32_t s = 4 * (hor_res - lv_area_get_width(area));
    for (int y = area->y1; y <= area->y2 && y < ver_res; y++) {
        for (int x = area->x1; x <= area->x2; x++) {
#if LVGL_VERSION_MAJOR >= 9
            uint8_t *src = px_map;
            px_map += 4;
#else
            uint8_t *src = (uint8_t *)color_p++;
#endif

            // bgr -> rgb
            *dst++ = src[2];
            *dst++ = src[1];
            *dst++ = src[0];
            *dst++ = src[3];
        }

        dst += s;
    }

    lv_disp_flush_ready(disp_drv);

    display_fb_dirty = true;
}

static int mouse_x = 0;
static int mouse_y = 0;
static int mouse_pressed = 0;

#define KEYBOARD_BUFFER_SIZE 1
static uint32_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t keyboard_buffer_index = 0;
static bool keyboard_pressed = false;

#if LVGL_VERSION_MAJOR >= 9
void my_mouse_read(lv_indev_t * indev_drv, lv_indev_data_t * data) {
#else
void my_mouse_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
#endif
    EEZ_UNUSED(indev_drv);

    /*Store the collected data*/
    data->point.x = (lv_coord_t)mouse_x;
    data->point.y = (lv_coord_t)mouse_y;
    data->state = mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#if LVGL_VERSION_MAJOR >= 9
void my_keyboard_read(lv_indev_t * indev_drv, lv_indev_data_t * data) {
#else
void my_keyboard_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
#endif
    EEZ_UNUSED(indev_drv);

    if (keyboard_pressed) {
        /*Send a release manually*/
        keyboard_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
    } else if (keyboard_buffer_index > 0) {
        /*Send the pressed character*/
        keyboard_pressed = true;
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = keyboard_buffer[--keyboard_buffer_index];
    }


}

static int mouse_wheel_delta = 0;
static int mouse_wheel_pressed = 0;

#if LVGL_VERSION_MAJOR >= 9
void my_mousewheel_read(lv_indev_t * indev_drv, lv_indev_data_t * data) {
#else
void my_mousewheel_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
#endif
    (void) indev_drv;      /*Unused*/

    data->state = mouse_wheel_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->enc_diff = (int16_t)mouse_wheel_delta;

    mouse_wheel_delta = 0;
}

////////////////////////////////////////////////////////////////////////////////
// memory based file system

uint16_t my_cache_size = 0;

#if LV_USE_USER_DATA
void *my_user_data = 0;
#endif

typedef struct {
    uint8_t *ptr;
    uint32_t pos;
} my_file_t;

#if LVGL_VERSION_MAJOR >= 9
#if LVGL_VERSION_MINOR >= 3
bool my_ready_cb(lv_fs_drv_t * drv) {
#else
bool my_ready_cb(struct lv_fs_drv_t * drv) {
#endif
#else
bool my_ready_cb(struct _lv_fs_drv_t * drv) {
#endif
    EEZ_UNUSED(drv);
    return true;
}

#if LVGL_VERSION_MAJOR >= 9
#if LVGL_VERSION_MINOR >= 3
void *my_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    my_file_t *file = (my_file_t *)lv_malloc(sizeof(my_file_t));
#else
void *my_open_cb(struct lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    my_file_t *file = (my_file_t *)lv_malloc(sizeof(my_file_t));
#endif
#else
void *my_open_cb(struct _lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode) {
    my_file_t *file = (my_file_t *)lv_mem_alloc(sizeof(my_file_t));
#endif
    EEZ_UNUSED(drv);
    EEZ_UNUSED(mode);
    file->ptr = (void *)atoi(path);
    file->pos = 0;
    return file;
}

#if LVGL_VERSION_MAJOR >= 9
#if LVGL_VERSION_MINOR >= 3
lv_fs_res_t my_close_cb(lv_fs_drv_t * drv, void * file_p) {
    lv_free(file_p);
#else
lv_fs_res_t my_close_cb(struct lv_fs_drv_t * drv, void * file_p) {
    lv_free(file_p);
#endif
#else
lv_fs_res_t my_close_cb(struct _lv_fs_drv_t * drv, void * file_p) {
    lv_mem_free(file_p);
#endif
    EEZ_UNUSED(drv);
    return LV_FS_RES_OK;
}

#if LVGL_VERSION_MAJOR >= 9
#if LVGL_VERSION_MINOR >= 3
lv_fs_res_t my_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
#else
lv_fs_res_t my_read_cb(struct lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
#endif
#else
lv_fs_res_t my_read_cb(struct _lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br) {
#endif
    EEZ_UNUSED(drv);
    my_file_t *file = (my_file_t *)file_p;
    memcpy(buf, file->ptr + file->pos, btr);
    file->pos += btr;
    if (br != 0)
        *br = btr;
    return LV_FS_RES_OK;
}

#if LVGL_VERSION_MAJOR >= 9
#if LVGL_VERSION_MINOR >= 3
lv_fs_res_t my_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
#else
lv_fs_res_t my_seek_cb(struct lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
#endif
#else
lv_fs_res_t my_seek_cb(struct _lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence) {
#endif
    EEZ_UNUSED(drv);
    my_file_t *file = (my_file_t *)file_p;
    if (whence == LV_FS_SEEK_SET) {
        file->pos = pos;
        return LV_FS_RES_OK;
    }
    if (whence == LV_FS_SEEK_CUR) {
        file->pos += pos;
        return LV_FS_RES_OK;
    }
    return LV_FS_RES_NOT_IMP;
}

#if LVGL_VERSION_MAJOR >= 9
#if LVGL_VERSION_MINOR >= 3
lv_fs_res_t my_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
#else
lv_fs_res_t my_tell_cb(struct lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
#endif
#else
lv_fs_res_t my_tell_cb(struct _lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p) {
#endif
    EEZ_UNUSED(drv);
    my_file_t *file = (my_file_t *)file_p;
    *pos_p = file->pos;
    return LV_FS_RES_OK;
}

static void init_fs_driver() {
    static lv_fs_drv_t drv;                   /*Needs to be static or global*/
    lv_fs_drv_init(&drv);                     /*Basic initialization*/

    drv.letter = 'M';                         /*An uppercase letter to identify the drive */
    drv.cache_size = my_cache_size;           /*Cache size for reading in bytes. 0 to not cache.*/

    drv.ready_cb = my_ready_cb;               /*Callback to tell if the drive is ready to use */
    drv.open_cb = my_open_cb;                 /*Callback to open a file */
    drv.close_cb = my_close_cb;               /*Callback to close a file */
    drv.read_cb = my_read_cb;                 /*Callback to read a file */
    drv.write_cb = 0;               /*Callback to write a file */
    drv.seek_cb = my_seek_cb;                 /*Callback to seek in a file (Move cursor) */
    drv.tell_cb = my_tell_cb;                 /*Callback to tell the cursor position  */

    drv.dir_open_cb = 0;         /*Callback to open directory to read its content */
    drv.dir_read_cb = 0;         /*Callback to read a directory's content */
    drv.dir_close_cb = 0;       /*Callback to close a directory */

#if LV_USE_USER_DATA
    drv.user_data = my_user_data;             /*Any custom data if required*/
#endif

    lv_fs_drv_register(&drv);                 /*Finally register the drive*/
}

////////////////////////////////////////////////////////////////////////////////

lv_indev_t *encoder_indev;
lv_indev_t *keyboard_indev;

EM_PORT_API(void) lvglSetEncoderGroup(lv_group_t *group) {
    lv_indev_set_group(encoder_indev, group);
}

EM_PORT_API(void) lvglSetKeyboardGroup(lv_group_t *group) {
    lv_indev_set_group(keyboard_indev, group);
}

static void hal_init() {
    // alloc memory for the display front buffer
    display_fb = (uint32_t *)malloc(sizeof(uint32_t) * hor_res * ver_res);
    memset(display_fb, 0x44, hor_res * ver_res * sizeof(uint32_t));

#if LVGL_VERSION_MAJOR >= 9
    lv_display_t * disp = lv_display_create(hor_res, ver_res);
    lv_display_set_flush_cb(disp, my_driver_flush);

    uint8_t *buf1 = malloc(sizeof(uint32_t) * hor_res * ver_res);
    uint8_t *buf2 = NULL;
    lv_display_set_buffers(disp, buf1, buf2, sizeof(uint32_t) * hor_res * ver_res, LV_DISPLAY_RENDER_MODE_PARTIAL);
#else
    /*Create a display buffer*/
    static lv_disp_draw_buf_t disp_buf1;
    lv_color_t * buf1_1 = malloc(sizeof(lv_color_t) * hor_res * ver_res);
    lv_disp_draw_buf_init(&disp_buf1, buf1_1, NULL, hor_res * ver_res);

    /*Create a display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);            /*Basic initialization*/
    disp_drv.draw_buf = &disp_buf1;
    disp_drv.flush_cb = my_driver_flush;    /*Used when `LV_VDB_SIZE != 0` in lv_conf.h (buffered drawing)*/
    disp_drv.hor_res = hor_res;
    disp_drv.ver_res = ver_res;
    lv_disp_drv_register(&disp_drv);
#endif

    if (!is_editor) {
        // mouse init
#if LVGL_VERSION_MAJOR >= 9
        lv_indev_t * indev1 = lv_indev_create();
        lv_indev_set_type(indev1, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev1, my_mouse_read);
        //lv_indev_set_mode(indev1, LV_INDEV_MODE_EVENT);
#else
        static lv_indev_drv_t indev_drv_1;
        lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
        indev_drv_1.type = LV_INDEV_TYPE_POINTER;
        indev_drv_1.read_cb = my_mouse_read;
        lv_indev_drv_register(&indev_drv_1);
#endif

        // keyboard init
#if LVGL_VERSION_MAJOR >= 9
        keyboard_indev = lv_indev_create();
        lv_indev_set_type(keyboard_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(keyboard_indev, my_keyboard_read);
        //lv_indev_set_mode(keyboard_indev, LV_INDEV_MODE_EVENT);
#else
        static lv_indev_drv_t indev_drv_2;
        lv_indev_drv_init(&indev_drv_2);
        indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
        indev_drv_2.read_cb = my_keyboard_read;
        keyboard_indev = lv_indev_drv_register(&indev_drv_2);
#endif

        // mousewheel init
#if LVGL_VERSION_MAJOR >= 9
        encoder_indev = lv_indev_create();
        lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
        lv_indev_set_read_cb(encoder_indev, my_mousewheel_read);
        //lv_indev_set_mode(encoder_indev, LV_INDEV_MODE_EVENT);
#else
        static lv_indev_drv_t indev_drv_3;
        lv_indev_drv_init(&indev_drv_3);
        indev_drv_3.type = LV_INDEV_TYPE_ENCODER;
        indev_drv_3.read_cb = my_mousewheel_read;
        encoder_indev = lv_indev_drv_register(&indev_drv_3);
#endif
    }

    init_fs_driver();
}

bool initialized = false;

#if LVGL_VERSION_MAJOR >= 9
static uint32_t g_prevTick;
#endif

EM_PORT_API(void) init(uint32_t wasmModuleId, uint32_t debuggerMessageSubsciptionFilter, uint8_t *assets, uint32_t assetsSize, uint32_t displayWidth, uint32_t displayHeight, bool darkTheme, uint32_t timeZone, bool screensLifetimeSupport) {
    is_editor = assetsSize == 0;

    hor_res = displayWidth;
    ver_res = displayHeight;

    /*Initialize LittlevGL*/
    lv_init();

    /*Initialize the HAL (display, input devices, tick) for LittlevGL*/
    hal_init();

    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), darkTheme, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    if (!is_editor) {
        flowInit(wasmModuleId, debuggerMessageSubsciptionFilter, assets, assetsSize, darkTheme, timeZone, screensLifetimeSupport);
    }

#if LVGL_VERSION_MAJOR >= 9
    g_prevTick = (uint32_t)emscripten_get_now();
#endif

    initialized = true;
}

EM_PORT_API(bool) mainLoop() {
    if (!initialized) {
        return true;
    }

#if LVGL_VERSION_MAJOR >= 9
    uint32_t currentTick = (uint32_t)emscripten_get_now();
    lv_tick_inc(currentTick - g_prevTick);
    g_prevTick = currentTick;
#endif

    /* Periodically call the lv_task handler */
    lv_task_handler();

    return flowTick();
}

EM_PORT_API(uint8_t*) getSyncedBuffer() {
    if (display_fb_dirty) {
        display_fb_dirty = false;
        return (uint8_t*)display_fb;
    }
	return NULL;
}

EM_PORT_API(bool) isRTL() {
    return false;
}

EM_PORT_API(void) onPointerEvent(int x, int y, int pressed) {
    if (x < 0) x = 0;
    else if (x >= hor_res) x = hor_res - 1;
    mouse_x = x;

    if (y < 0) y = 0;
    else if (y >= ver_res) y = ver_res - 1;
    mouse_y = y;

    mouse_pressed = pressed;
}

EM_PORT_API(void) onMouseWheelEvent(double yMouseWheel, int pressed) {
    if (yMouseWheel >= 100 || yMouseWheel <= -100) {
        yMouseWheel /= 100;
    }
    mouse_wheel_delta = round(yMouseWheel);
    mouse_wheel_pressed = pressed;
}

EM_PORT_API(void) onKeyPressed(uint32_t key) {
    if (keyboard_buffer_index < KEYBOARD_BUFFER_SIZE) {
        keyboard_buffer[keyboard_buffer_index++] = key;
    }
}

////////////////////////////////////////////////////////////////////////////////

#define SYMBOLS_STRING_INIT_ALLOCATED 1024 * 1024

typedef struct SymbolsString {
    char *ptr;
    unsigned size;
    unsigned allocated;
} SymbolsString;

static SymbolsString g_symbolsString;

void symbols_append(const char *format, ...) {
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);

    if (needed < 0) {
        va_end(args);
        return; // encoding error
    }

    unsigned total_needed = g_symbolsString.size + needed + 1;

    // Ensure we have enough space
    assert(total_needed <= g_symbolsString.allocated);

    // Now actually write the formatted string at the end of the buffer
    int written = vsnprintf(g_symbolsString.ptr + g_symbolsString.size, 
                            g_symbolsString.allocated - g_symbolsString.size, 
                            format, args);

    va_end(args);

    if (written < 0 || written > needed) {
        // Something went wrong (shouldn't happen with proper vsnprintf)
        return;
    }

    g_symbolsString.size += written;
    // symstr->ptr is always null-terminated by vsnprintf
}

lv_obj_t *lv_spinner_create_adapt(lv_obj_t *parentObj) {
#if LVGL_VERSION_MAJOR >= 9
    return lv_spinner_create(parentObj);
#else
    return lv_spinner_create(parentObj, 1000, 60);
#endif
}

#if LVGL_VERSION_MAJOR >= 9
#else
lv_obj_t *lv_colorwheel_create_adapt(lv_obj_t *parentObj) {
    return lv_colorwheel_create(parentObj, false);
}
#endif

lv_obj_t *lv_qrcode_create_adapt(lv_obj_t *parentObj) {
#if LVGL_VERSION_MAJOR >= 9
    return lv_qrcode_create(parentObj);
#else
    lv_color_t dark_color;
    dark_color.ch.blue = 255;
    dark_color.ch.green = 0;
    dark_color.ch.red = 0;
    lv_color_t light_color;
    light_color.ch.blue = 0;
    light_color.ch.green = 0;
    light_color.ch.red = 255;
    return lv_qrcode_create(parentObj, 120, dark_color, light_color);
#endif
}

    
typedef struct {
    const char *name;
    lv_obj_t *(*create)(lv_obj_t *);
} WidgetInfo;

WidgetInfo widgets[] = {
    { "Screen", lv_obj_create },
    { "Label", lv_label_create },
    { "Button", lv_btn_create },
    { "Panel", lv_obj_create },
    { "Image", lv_img_create },
    { "Slider", lv_slider_create },
    { "Roller", lv_roller_create },
    { "Switch", lv_switch_create },
    { "Bar", lv_bar_create },
    { "Dropdown", lv_dropdown_create },
    { "Arc", lv_arc_create },
    { "Spinner", lv_spinner_create_adapt },
    { "Checkbox", lv_checkbox_create },
    { "Textarea", lv_textarea_create },
    { "Keyboard", lv_keyboard_create },
    { "Chart", lv_chart_create },
    { "Calendar", lv_calendar_create },
#if LVGL_VERSION_MAJOR >= 9
    { "Scale", lv_scale_create },
#else
    { "Colorwheel", lv_colorwheel_create_adapt },
    { "ImageButton", lv_imgbtn_create },
    { "Meter", lv_meter_create },
#endif
    { "QRCode", lv_qrcode_create_adapt },
    { "Spinbox", lv_spinbox_create },
};

typedef struct {
    const char *name;
    int code;
} FlagInfo;

FlagInfo flags[] = {
    { "HIDDEN", LV_OBJ_FLAG_HIDDEN },
    { "CLICKABLE", LV_OBJ_FLAG_CLICKABLE },
    { "CLICK_FOCUSABLE", LV_OBJ_FLAG_CLICK_FOCUSABLE },
    { "CHECKABLE", LV_OBJ_FLAG_CHECKABLE },
    { "SCROLLABLE", LV_OBJ_FLAG_SCROLLABLE },
    { "SCROLL_ELASTIC", LV_OBJ_FLAG_SCROLL_ELASTIC },
    { "SCROLL_MOMENTUM", LV_OBJ_FLAG_SCROLL_MOMENTUM },
    { "SCROLL_ONE", LV_OBJ_FLAG_SCROLL_ONE },
    { "SCROLL_CHAIN_HOR", LV_OBJ_FLAG_SCROLL_CHAIN_HOR },
    { "SCROLL_CHAIN_VER", LV_OBJ_FLAG_SCROLL_CHAIN_VER },
    { "SCROLL_CHAIN", LV_OBJ_FLAG_SCROLL_CHAIN },
    { "SCROLL_ON_FOCUS", LV_OBJ_FLAG_SCROLL_ON_FOCUS },
    { "SCROLL_WITH_ARROW", LV_OBJ_FLAG_SCROLL_WITH_ARROW },
    { "SNAPPABLE", LV_OBJ_FLAG_SNAPPABLE },
    { "PRESS_LOCK", LV_OBJ_FLAG_PRESS_LOCK },
    { "EVENT_BUBBLE", LV_OBJ_FLAG_EVENT_BUBBLE },
    { "GESTURE_BUBBLE", LV_OBJ_FLAG_GESTURE_BUBBLE },
    { "ADV_HITTEST", LV_OBJ_FLAG_ADV_HITTEST },
    { "IGNORE_LAYOUT", LV_OBJ_FLAG_IGNORE_LAYOUT },
    { "FLOATING", LV_OBJ_FLAG_FLOATING },
    { "OVERFLOW_VISIBLE", LV_OBJ_FLAG_OVERFLOW_VISIBLE }
};


void dump_widget_flags_info(WidgetInfo *info, lv_obj_t *obj, bool last) {
    symbols_append("\"");
    symbols_append(info->name);
    symbols_append("\": \"");

    bool first = true;

    for (size_t i = 0; i < sizeof(flags) / sizeof(FlagInfo); i++) {
        if (lv_obj_has_flag(obj, flags[i].code)) {
            if (first) {
                first = false;
            } else {
                symbols_append(" | ");
            }
            symbols_append(flags[i].name);
        }
    }

    symbols_append("\"");
    if (!last) {
        symbols_append(",");
    }
}

void dump_widgets_flags_info() {
    lv_obj_t *parent_obj = 0;

    for (size_t i = 0; i < sizeof(widgets) / sizeof(WidgetInfo); i++) {
        lv_obj_t *obj = widgets[i].create(parent_obj);
        if (!parent_obj) {
            parent_obj = obj;
        }
        dump_widget_flags_info(widgets + i, obj, i == sizeof(widgets) / sizeof(WidgetInfo) - 1);
    }

    lv_obj_del(parent_obj);
}

void style_append(const char *name, int code) {
    symbols_append("{\"name\": \"%s\", \"code\": %d},", name, code);
}

void style_append_with_new_name(const char *name, int code, const char *new_name) {
    symbols_append("{\"name\": \"%s\", \"code\": %d, \"new_name\": \"%s\"},", name, code, new_name);
}

void style_append_last(const char *name, int code) {
    symbols_append("{\"name\": \"%s\", \"code\": %d}", name, code);
}

void style_append_undefined(const char *name) {
    symbols_append("{\"name\": \"%s\", \"code\": null},", name);
}

void dump_custom_styles() {
    /*Group 0*/
    style_append("LV_STYLE_WIDTH", LV_STYLE_WIDTH); // { "8.3": 1, "9.0": 1 },
    style_append("LV_STYLE_HEIGHT", LV_STYLE_HEIGHT); // "8.3": 4, "9.0": 2 },

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_LENGTH", LV_STYLE_LENGTH); // "8.3": undefined, "9.0": 3 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_LENGTH");
#endif

    style_append("LV_STYLE_MIN_WIDTH", LV_STYLE_MIN_WIDTH); // "8.3": 2, "9.0": 4 },
    style_append("LV_STYLE_MAX_WIDTH", LV_STYLE_MAX_WIDTH); // "8.3": 3, "9.0": 5 },
    style_append("LV_STYLE_MIN_HEIGHT", LV_STYLE_MIN_HEIGHT); // "8.3": 5, "9.0": 6 },
    style_append("LV_STYLE_MAX_HEIGHT", LV_STYLE_MAX_HEIGHT); // "8.3": 6, "9.0": 7 },

    style_append("LV_STYLE_X", LV_STYLE_X); // "8.3": 7, "9.0": 8 },
    style_append("LV_STYLE_Y", LV_STYLE_Y); // "8.3": 8, "9.0": 9 },
    style_append("LV_STYLE_ALIGN", LV_STYLE_ALIGN); // "8.3": 9, "9.0": 10 },

    style_append("LV_STYLE_RADIUS", LV_STYLE_RADIUS); // "8.3": 11, "9.0": 12 },

    /*Group 1*/
    style_append("LV_STYLE_PAD_TOP", LV_STYLE_PAD_TOP); // "8.3": 16, "9.0": 16 },
    style_append("LV_STYLE_PAD_BOTTOM", LV_STYLE_PAD_BOTTOM); // "8.3": 17, "9.0": 17 },
    style_append("LV_STYLE_PAD_LEFT", LV_STYLE_PAD_LEFT); // "8.3": 18, "9.0": 18 },
    style_append("LV_STYLE_PAD_RIGHT", LV_STYLE_PAD_RIGHT); // "8.3": 19, "9.0": 19 },

    style_append("LV_STYLE_PAD_ROW", LV_STYLE_PAD_ROW); // "8.3": 20, "9.0": 20 },
    style_append("LV_STYLE_PAD_COLUMN", LV_STYLE_PAD_COLUMN); // "8.3": 21, "9.0": 21 },
    style_append("LV_STYLE_LAYOUT", LV_STYLE_LAYOUT); // "8.3": 10, "9.0": 22 },

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_MARGIN_TOP", LV_STYLE_MARGIN_TOP); // "8.3": undefined, "9.0": 24 }, // ONLY 9.0
    style_append("LV_STYLE_MARGIN_BOTTOM", LV_STYLE_MARGIN_BOTTOM); // "8.3": undefined, "9.0": 25 }, // ONLY 9.0
    style_append("LV_STYLE_MARGIN_LEFT", LV_STYLE_MARGIN_LEFT); // "8.3": undefined, "9.0": 26 }, // ONLY 9.0
    style_append("LV_STYLE_MARGIN_RIGHT", LV_STYLE_MARGIN_RIGHT); // "8.3": undefined, "9.0": 27 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_MARGIN_TOP");
    style_append_undefined("LV_STYLE_MARGIN_BOTTOM");
    style_append_undefined("LV_STYLE_MARGIN_LEFT");
    style_append_undefined("LV_STYLE_MARGIN_RIGHT");
#endif

    /*Group 2*/
    style_append("LV_STYLE_BG_COLOR", LV_STYLE_BG_COLOR); // "8.3": 32, "9.0": 28 },
    style_append("LV_STYLE_BG_OPA", LV_STYLE_BG_OPA); // "8.3": 33, "9.0": 29 },

    style_append("LV_STYLE_BG_GRAD_DIR", LV_STYLE_BG_GRAD_DIR); // "8.3": 35, "9.0": 32 },
    style_append("LV_STYLE_BG_MAIN_STOP", LV_STYLE_BG_MAIN_STOP); // "8.3": 36, "9.0": 33 },
    style_append("LV_STYLE_BG_GRAD_STOP", LV_STYLE_BG_GRAD_STOP); // "8.3": 37, "9.0": 34 },
    style_append("LV_STYLE_BG_GRAD_COLOR", LV_STYLE_BG_GRAD_COLOR); // "8.3": 34, "9.0": 35 },

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_BG_MAIN_OPA", LV_STYLE_BG_MAIN_OPA); // "8.3": undefined, "9.0": 36 }, // ONLY 9.0
    style_append("LV_STYLE_BG_GRAD_OPA", LV_STYLE_BG_GRAD_OPA); // "8.3": undefined, "9.0": 37 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_BG_MAIN_OPA");
    style_append_undefined("LV_STYLE_BG_GRAD_OPA");
#endif

    style_append("LV_STYLE_BG_GRAD", LV_STYLE_BG_GRAD); // "8.3": 38, "9.0": 38 },
    style_append("LV_STYLE_BASE_DIR", LV_STYLE_BASE_DIR); // "8.3": 22, "9.0": 39 },

#if LVGL_VERSION_MAJOR >= 9
    style_append_undefined("LV_STYLE_BG_DITHER_MODE");
#else
    style_append("LV_STYLE_BG_DITHER_MODE", LV_STYLE_BG_DITHER_MODE); // "8.3": 39, "9.0": undefined }, // ONLY 8.3
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append_with_new_name("LV_STYLE_BG_IMG_SRC", LV_STYLE_BG_IMAGE_SRC, "LV_STYLE_BG_IMAGE_SRC"); // "8.3": 40, "9.0": 40 },
    style_append_with_new_name("LV_STYLE_BG_IMG_OPA", LV_STYLE_BG_IMAGE_OPA, "LV_STYLE_BG_IMAGE_OPA"); // "8.3": 41, "9.0": 41 },
    style_append_with_new_name("LV_STYLE_BG_IMG_RECOLOR", LV_STYLE_BG_IMAGE_RECOLOR, "LV_STYLE_BG_IMAGE_RECOLOR"); // "8.3": 42, "9.0": 42 },
    style_append_with_new_name("LV_STYLE_BG_IMG_RECOLOR_OPA", LV_STYLE_BG_IMAGE_RECOLOR_OPA, "LV_STYLE_BG_IMAGE_RECOLOR_OPA"); // "8.3": 43, "9.0": 43 },
#else
    style_append("LV_STYLE_BG_IMG_SRC", LV_STYLE_BG_IMG_SRC); // "8.3": 40, "9.0": 40 },
    style_append("LV_STYLE_BG_IMG_OPA", LV_STYLE_BG_IMG_OPA); // "8.3": 41, "9.0": 41 },
    style_append("LV_STYLE_BG_IMG_RECOLOR", LV_STYLE_BG_IMG_RECOLOR); // "8.3": 42, "9.0": 42 },
    style_append("LV_STYLE_BG_IMG_RECOLOR_OPA", LV_STYLE_BG_IMG_RECOLOR_OPA); // "8.3": 43, "9.0": 43 },
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append_with_new_name("LV_STYLE_BG_IMG_TILED", LV_STYLE_BG_IMAGE_TILED, "LV_STYLE_BG_IMAGE_TILED"); // "8.3": 44, "9.0": 44 },
#else
    style_append("LV_STYLE_BG_IMG_TILED", LV_STYLE_BG_IMG_TILED); // "8.3": 44, "9.0": 44 },
#endif

    style_append("LV_STYLE_CLIP_CORNER", LV_STYLE_CLIP_CORNER); // "8.3": 23, "9.0": 45 },

    /*Group 3*/
    style_append("LV_STYLE_BORDER_WIDTH", LV_STYLE_BORDER_WIDTH); // "8.3": 50, "9.0": 48 },
    style_append("LV_STYLE_BORDER_COLOR", LV_STYLE_BORDER_COLOR); // "8.3": 48, "9.0": 49 },
    style_append("LV_STYLE_BORDER_OPA", LV_STYLE_BORDER_OPA); // "8.3": 49, "9.0": 50 },

    style_append("LV_STYLE_BORDER_SIDE", LV_STYLE_BORDER_SIDE); // "8.3": 51, "9.0": 52 },
    style_append("LV_STYLE_BORDER_POST", LV_STYLE_BORDER_POST); // "8.3": 52, "9.0": 53 },

    style_append("LV_STYLE_OUTLINE_WIDTH", LV_STYLE_OUTLINE_WIDTH); // "8.3": 53, "9.0": 56 },
    style_append("LV_STYLE_OUTLINE_COLOR", LV_STYLE_OUTLINE_COLOR); // "8.3": 54, "9.0": 57 },
    style_append("LV_STYLE_OUTLINE_OPA", LV_STYLE_OUTLINE_OPA); // "8.3": 55, "9.0": 58 },
    style_append("LV_STYLE_OUTLINE_PAD", LV_STYLE_OUTLINE_PAD); // "8.3": 56, "9.0": 59 },

    /*Group 4*/
    style_append("LV_STYLE_SHADOW_WIDTH", LV_STYLE_SHADOW_WIDTH); // "8.3": 64, "9.0": 60 },
    style_append("LV_STYLE_SHADOW_COLOR", LV_STYLE_SHADOW_COLOR); // "8.3": 68, "9.0": 61 },
    style_append("LV_STYLE_SHADOW_OPA", LV_STYLE_SHADOW_OPA); // "8.3": 69, "9.0": 62 },

    style_append("LV_STYLE_SHADOW_OFS_X", LV_STYLE_SHADOW_OFS_X); // "8.3": 65, "9.0": 64 },
    style_append("LV_STYLE_SHADOW_OFS_Y", LV_STYLE_SHADOW_OFS_Y); // "8.3": 66, "9.0": 65 },
    style_append("LV_STYLE_SHADOW_SPREAD", LV_STYLE_SHADOW_SPREAD); // "8.3": 67, "9.0": 66 },

#if LVGL_VERSION_MAJOR >= 9
    style_append_with_new_name("LV_STYLE_IMG_OPA", LV_STYLE_IMAGE_OPA, "LV_STYLE_IMAGE_OPA"); // "8.3": 70, "9.0": 68 },
    style_append_with_new_name("LV_STYLE_IMG_RECOLOR", LV_STYLE_IMAGE_RECOLOR, "LV_STYLE_IMAGE_RECOLOR"); // "8.3": 71, "9.0": 69 },
    style_append_with_new_name("LV_STYLE_IMG_RECOLOR_OPA", LV_STYLE_IMAGE_RECOLOR_OPA, "LV_STYLE_IMAGE_RECOLOR_OPA"); // "8.3": 72, "9.0": 70 },
#else
    style_append("LV_STYLE_IMG_OPA", LV_STYLE_IMG_OPA); // "8.3": 70, "9.0": 68 },
    style_append("LV_STYLE_IMG_RECOLOR", LV_STYLE_IMG_RECOLOR); // "8.3": 71, "9.0": 69 },
    style_append("LV_STYLE_IMG_RECOLOR_OPA", LV_STYLE_IMG_RECOLOR_OPA); // "8.3": 72, "9.0": 70 },
#endif

    style_append("LV_STYLE_LINE_WIDTH", LV_STYLE_LINE_WIDTH); // "8.3": 73, "9.0": 72 },
    style_append("LV_STYLE_LINE_DASH_WIDTH", LV_STYLE_LINE_DASH_WIDTH); // "8.3": 74, "9.0": 73 },
    style_append("LV_STYLE_LINE_DASH_GAP", LV_STYLE_LINE_DASH_GAP); // "8.3": 75, "9.0": 74 },
    style_append("LV_STYLE_LINE_ROUNDED", LV_STYLE_LINE_ROUNDED); // "8.3": 76, "9.0": 75 },
    style_append("LV_STYLE_LINE_COLOR", LV_STYLE_LINE_COLOR); // "8.3": 77, "9.0": 76 },
    style_append("LV_STYLE_LINE_OPA", LV_STYLE_LINE_OPA); // "8.3": 78, "9.0": 77 },

    /*Group 5*/
    style_append("LV_STYLE_ARC_WIDTH", LV_STYLE_ARC_WIDTH); // "8.3": 80, "9.0": 80 },
    style_append("LV_STYLE_ARC_ROUNDED", LV_STYLE_ARC_ROUNDED); // "8.3": 81, "9.0": 81 },
    style_append("LV_STYLE_ARC_COLOR", LV_STYLE_ARC_COLOR); // "8.3": 82, "9.0": 82 },
    style_append("LV_STYLE_ARC_OPA", LV_STYLE_ARC_OPA); // "8.3": 83, "9.0": 83 },

#if LVGL_VERSION_MAJOR >= 9
    style_append_with_new_name("LV_STYLE_ARC_IMG_SRC", LV_STYLE_ARC_IMAGE_SRC, "LV_STYLE_ARC_IMAGE_SRC"); // "8.3": 84, "9.0": 84 },
#else
    style_append("LV_STYLE_ARC_IMG_SRC", LV_STYLE_ARC_IMG_SRC); // "8.3": 84, "9.0": 84 },
#endif

    style_append("LV_STYLE_TEXT_COLOR", LV_STYLE_TEXT_COLOR); // "8.3": 85, "9.0": 88 },
    style_append("LV_STYLE_TEXT_OPA", LV_STYLE_TEXT_OPA); // "8.3": 86, "9.0": 89 },
    style_append("LV_STYLE_TEXT_FONT", LV_STYLE_TEXT_FONT); // "8.3": 87, "9.0": 90 },

    style_append("LV_STYLE_TEXT_LETTER_SPACE", LV_STYLE_TEXT_LETTER_SPACE); // "8.3": 88, "9.0": 91 },
    style_append("LV_STYLE_TEXT_LINE_SPACE", LV_STYLE_TEXT_LINE_SPACE); // "8.3": 89, "9.0": 92 },
    style_append("LV_STYLE_TEXT_DECOR", LV_STYLE_TEXT_DECOR); // "8.3": 90, "9.0": 93 },
    style_append("LV_STYLE_TEXT_ALIGN", LV_STYLE_TEXT_ALIGN); // "8.3": 91, "9.0": 94 },

    style_append("LV_STYLE_OPA", LV_STYLE_OPA); // "8.3": 96, "9.0": 95 },
    style_append("LV_STYLE_OPA_LAYERED", LV_STYLE_OPA_LAYERED); // "8.3": 97, "9.0": 96 },
    style_append("LV_STYLE_COLOR_FILTER_DSC", LV_STYLE_COLOR_FILTER_DSC); // "8.3": 98, "9.0": 97 },
    style_append("LV_STYLE_COLOR_FILTER_OPA", LV_STYLE_COLOR_FILTER_OPA); // "8.3": 99, "9.0": 98 },

    style_append("LV_STYLE_ANIM", LV_STYLE_ANIM); // "8.3": 100, "9.0": 99 },

#if LVGL_VERSION_MAJOR >= 9
    style_append_undefined("LV_STYLE_ANIM_TIME");
#else
    style_append("LV_STYLE_ANIM_TIME", LV_STYLE_ANIM_TIME); // "8.3": 101, "9.0": undefined }, // ONLY 8.3
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_ANIM_DURATION", LV_STYLE_ANIM_DURATION); // "8.3": undefined, "9.0": 100 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_ANIM_DURATION");
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append_undefined("LV_STYLE_ANIM_SPEED");
#else
    style_append("LV_STYLE_ANIM_SPEED", LV_STYLE_ANIM_SPEED); // "8.3": 102, "9.0": undefined }, // ONLY 8.3
#endif

    style_append("LV_STYLE_TRANSITION", LV_STYLE_TRANSITION); // "8.3": 103, "9.0": 102 },

    style_append("LV_STYLE_BLEND_MODE", LV_STYLE_BLEND_MODE); // "8.3": 104, "9.0": 103 },
    style_append("LV_STYLE_TRANSFORM_WIDTH", LV_STYLE_TRANSFORM_WIDTH); // "8.3": 105, "9.0": 104 },
    style_append("LV_STYLE_TRANSFORM_HEIGHT", LV_STYLE_TRANSFORM_HEIGHT); // "8.3": 106, "9.0": 105 },
    style_append("LV_STYLE_TRANSLATE_X", LV_STYLE_TRANSLATE_X); // "8.3": 107, "9.0": 106 },
    style_append("LV_STYLE_TRANSLATE_Y", LV_STYLE_TRANSLATE_Y); // "8.3": 108, "9.0": 107 },

#if LVGL_VERSION_MAJOR >= 9
    style_append_undefined("LV_STYLE_TRANSFORM_ZOOM");
#else
    style_append("LV_STYLE_TRANSFORM_ZOOM", LV_STYLE_TRANSFORM_ZOOM); // "8.3": 109, "9.0": undefined }, // ONLY 8.3
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_TRANSFORM_SCALE_X", LV_STYLE_TRANSFORM_SCALE_X); // "8.3": undefined, "9.0": 108 }, // ONLY 9.0
    style_append("LV_STYLE_TRANSFORM_SCALE_Y", LV_STYLE_TRANSFORM_SCALE_Y); // "8.3": undefined, "9.0": 109 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_TRANSFORM_SCALE_X");
    style_append_undefined("LV_STYLE_TRANSFORM_SCALE_Y");
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append_undefined("LV_STYLE_TRANSFORM_ANGLE");
#else
    style_append("LV_STYLE_TRANSFORM_ANGLE", LV_STYLE_TRANSFORM_ANGLE); // "8.3": 110, "9.0": undefined }, // ONLY 8.3
#endif

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_TRANSFORM_ROTATION", LV_STYLE_TRANSFORM_ROTATION); // "8.3": undefined, "9.0": 110 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_TRANSFORM_ROTATION");
#endif

    style_append("LV_STYLE_TRANSFORM_PIVOT_X", LV_STYLE_TRANSFORM_PIVOT_X); // "8.3": 111, "9.0": 111 },
    style_append("LV_STYLE_TRANSFORM_PIVOT_Y", LV_STYLE_TRANSFORM_PIVOT_Y); // "8.3": 112, "9.0": 112 },

#if LVGL_VERSION_MAJOR >= 9
    style_append("LV_STYLE_TRANSFORM_SKEW_X", LV_STYLE_TRANSFORM_SKEW_X); // "8.3": undefined, "9.0": 113 }, // ONLY 9.0
    style_append("LV_STYLE_TRANSFORM_SKEW_Y", LV_STYLE_TRANSFORM_SKEW_Y); // "8.3": undefined, "9.0": 114 }, // ONLY 9.0
#else
    style_append_undefined("LV_STYLE_TRANSFORM_SKEW_X");
    style_append_undefined("LV_STYLE_TRANSFORM_SKEW_Y");
#endif

    style_append("LV_STYLE_FLEX_FLOW", LV_STYLE_FLEX_FLOW);
    style_append("LV_STYLE_FLEX_MAIN_PLACE", LV_STYLE_FLEX_MAIN_PLACE);
    style_append("LV_STYLE_FLEX_CROSS_PLACE", LV_STYLE_FLEX_CROSS_PLACE);
    style_append("LV_STYLE_FLEX_TRACK_PLACE", LV_STYLE_FLEX_TRACK_PLACE);
    style_append("LV_STYLE_FLEX_GROW", LV_STYLE_FLEX_GROW);

    style_append("LV_STYLE_GRID_COLUMN_ALIGN", LV_STYLE_GRID_COLUMN_ALIGN);
    style_append("LV_STYLE_GRID_ROW_ALIGN", LV_STYLE_GRID_ROW_ALIGN);
    style_append("LV_STYLE_GRID_ROW_DSC_ARRAY", LV_STYLE_GRID_ROW_DSC_ARRAY);
    style_append("LV_STYLE_GRID_COLUMN_DSC_ARRAY", LV_STYLE_GRID_COLUMN_DSC_ARRAY);
    style_append("LV_STYLE_GRID_CELL_COLUMN_POS", LV_STYLE_GRID_CELL_COLUMN_POS);
    style_append("LV_STYLE_GRID_CELL_COLUMN_SPAN", LV_STYLE_GRID_CELL_COLUMN_SPAN);
    style_append("LV_STYLE_GRID_CELL_X_ALIGN", LV_STYLE_GRID_CELL_X_ALIGN);
    style_append("LV_STYLE_GRID_CELL_ROW_POS", LV_STYLE_GRID_CELL_ROW_POS);
    style_append("LV_STYLE_GRID_CELL_ROW_SPAN", LV_STYLE_GRID_CELL_ROW_SPAN);
    style_append_last("LV_STYLE_GRID_CELL_Y_ALIGN", LV_STYLE_GRID_CELL_Y_ALIGN);
}

EM_PORT_API(const char *) getStudioSymbols() {
    hor_res = 400;
    ver_res = 400;
    lv_init();
    hal_init();

    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    g_symbolsString.ptr = (char *)malloc(SYMBOLS_STRING_INIT_ALLOCATED);
    if (!g_symbolsString.ptr) {
        return "memory allocation error";
    }
 
    g_symbolsString.allocated = SYMBOLS_STRING_INIT_ALLOCATED;
    g_symbolsString.size = 0;
 
    g_symbolsString.ptr[0] = 0;

    symbols_append("{\"styles\":[");
    dump_custom_styles();
    symbols_append("],\"widget_flags\":{");
    dump_widgets_flags_info();
    symbols_append("}}");

    return g_symbolsString.ptr;
}