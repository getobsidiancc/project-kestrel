idf_component_register(SRCS "main.c" "input_usb.c" "translate.c" "gip_host.c" "settings.c" "hidparse.c"
                       INCLUDE_DIRS "."
                       REQUIRES kestrel_link kestrel_gip usb usb_host_hid esp_timer driver esp_driver_gpio nvs_flash)
