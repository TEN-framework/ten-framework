idf_build_get_property(target IDF_TARGET)

set(srcs        "dl_fft_f32.c"
                "dl_fft_s16.c"
                "dl_rfft_f32.c"
                "dl_rfft_s16.c"
                "dl_fft.hpp"
                "base/dl_fft2r_fc32_ansi.c"
                "base/dl_fft4r_fc32_ansi.c"
                "base/dl_fft2r_sc16_ansi.c"
                "base/dl_fft_base.c"
                )

set(include_dirs    "."
                    "base"
                    "base/isa"
                    )

if(CONFIG_IDF_TARGET_ESP32)
    list(APPEND srcs        "base/isa/esp32/dl_fft2r_fc32_ae32.S"
                            "base/isa/esp32/dl_fft4r_fc32_ae32.S" )

elseif(CONFIG_IDF_TARGET_ESP32S3)
    list(APPEND srcs        "base/isa/esp32s3/dl_fft2r_fc32_aes3.S"
                            "base/isa/esp32s3/dl_fft4r_fc32_aes3.S" )

elseif(CONFIG_IDF_TARGET_ESP32P4)
    list(APPEND srcs        "base/isa/esp32p4/dl_fft2r_fc32_arp4.S"
                            "base/isa/esp32p4/dl_fft4r_fc32_arp4.S" )

endif()


idf_component_register(SRCS ${srcs}  INCLUDE_DIRS ${include_dirs})

component_compile_options(-ffast-math -O2)
