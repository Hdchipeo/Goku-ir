idf_component_register(SRCS "src/goku_wifi_portal.c"
                       INCLUDE_DIRS "include"
                       EMBED_FILES "www/index.html"
                       PRIV_REQUIRES esp_wifi esp_netif nvs_flash esp_http_server lwip json)
