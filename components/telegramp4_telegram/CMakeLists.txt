idf_component_register(
    SRCS "telegramp4_telegram.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client json esp_wifi mbedtls telegramp4_wifi telegramp4_security
    PRIV_REQUIRES esp_timer
)
