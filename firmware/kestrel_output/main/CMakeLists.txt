idf_component_register(SRCS "main.c" "xinput.c" "gip_device.c" "cfg_bridge.c"
                       INCLUDE_DIRS "."
                       REQUIRES kestrel_link kestrel_gip esp_timer driver esp_driver_gpio esp_driver_uart)

# Let gip_device.c answer string descriptors 4 and 0xEE (owned by esp_tinyusb)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=tud_descriptor_string_cb")
