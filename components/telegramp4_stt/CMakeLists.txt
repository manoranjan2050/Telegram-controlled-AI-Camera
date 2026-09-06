idf_component_register(
    SRCS "telegramp4_stt.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json mbedtls
)
