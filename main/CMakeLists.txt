idf_component_register(
    SRCS "app_main.cpp" "freertos_static_mem.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash telegramp4_board telegramp4_wifi telegramp4_telegram telegramp4_security telegramp4_camera telegramp4_storage telegramp4_video telegramp4_audio telegramp4_stt telegramp4_ai telegramp4_motion telegramp4_gpio telegramp4_display telegramp4_ota telegramp4_provisioning json esp_timer freertos driver
)

# freertos_static_mem.c overrides FreeRTOS's IDLE/Timer task memory providers via
# linker --wrap so their TCB/stack always come from ordinary internal DRAM (.bss),
# never the small TCM region that pvPortMalloc() can fall back to once PSRAM
# fragments the internal heap - see the file's header comment for the full story.
target_link_options(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=vApplicationGetIdleTaskMemory"
    "-Wl,--wrap=vApplicationGetPassiveIdleTaskMemory"
    "-Wl,--wrap=vApplicationGetTimerTaskMemory"
)
