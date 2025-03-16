idf_component_register(SRCS "hello_world_main.c" "lxwk.c" "lxwk_common_20.c"
                    PRIV_REQUIRES spi_flash esp_wifi nvs_flash esp_driver_gpio esp_driver_i2s esp_driver_i2c esp_lcd tcp_transport esp_netif log
                    INCLUDE_DIRS "")
