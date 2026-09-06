idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES nvs_flash telegramp4_board telegramp4_wifi telegramp4_telegram telegramp4_security telegramp4_camera telegramp4_storage telegramp4_video telegramp4_audio telegramp4_stt telegramp4_ai telegramp4_motion telegramp4_gpio telegramp4_display json esp_timer
)
