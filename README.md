This folder contains C++ projects that are build using Emscripten to create WASM libraries for:

- flow runtime engine for the Dashboard and EEZ-GUI projects (eez-runtime)
- flow runtime engine for the LVGL (lvgl-runtime, for varius versions)
- LZ4 compression used by the Studio

How to add submodules?

- EEZ-Framework
    - We always use master commit.
    - git submodule add https://github.com/eez-open/eez-framework eez-framework

- LVGL
    - git submodule add https://github.com/lvgl/lvgl lvgl-runtime/v8.4.0/lvgl
    - Then checkout desired commit
