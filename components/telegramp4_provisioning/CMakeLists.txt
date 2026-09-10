idf_component_register(
    SRCS "telegramp4_provisioning.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_http_server
)
