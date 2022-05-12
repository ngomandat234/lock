# idf_component_register(SRCS "main.c"
#                     INCLUDE_DIRS ".")
idf_component_register(SRCS "main.c" "DisplayUI.c" "rc522.c" "ssd1306.c" "ssd1306_i2c.c" "ssd1306_spi.c" "SNTP_local.c"
                    INCLUDE_DIRS "Inc")