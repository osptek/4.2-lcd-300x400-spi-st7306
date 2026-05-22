idf_component_register(
    SRCS 
        "mian.cc"
        "st7306/st7306_color4.cc"
        "encoder/encoder.cc"
                    
    INCLUDE_DIRS 
        "."
        "st7306"
        "encoder"
    
    REQUIRES driver esp_timer
)
