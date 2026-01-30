idf_component_register(SRCS "3d_graphics_demo.cpp" 
                            "../../../graphics/3d_matrix/3d_matrix_data/esp_logo.c"
                            "../../../graphics/3d_matrix/3d_matrix_data/esp_text.c" 
                            "../../../graphics/3d_matrix/3d_matrix_data/image_to_3d_matrix.c"
                            "../../../graphics/3d_matrix/3d_matrix_src/graphics_support.cpp" 
                       INCLUDE_DIRS "."
                                    "../../../graphics/3d_matrix/3d_matrix_data"
                                    "../../../graphics/3d_matrix/3d_matrix_src")
