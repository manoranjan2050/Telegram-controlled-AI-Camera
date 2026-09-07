idf_component_register(
    SRCS "telegramp4_storage.c"
    INCLUDE_DIRS "include"
    REQUIRES fatfs esp_driver_sdmmc driver
)
