idf_component_register(SRCS "3d_kalman_demo.cpp" 
                              "bmi270_context.c"
                            "../../../../azure_board_apps/graphics/3d_matrix/3d_matrix_data/esp_logo.c"
                            "../../../../azure_board_apps/graphics/3d_matrix/3d_matrix_data/esp_text.c" 
                            "../../../../azure_board_apps/graphics/3d_matrix/3d_matrix_data/image_to_3d_matrix.c"
                            "../../../../azure_board_apps/graphics/3d_matrix/3d_matrix_src/graphics_support.cpp" 
                       INCLUDE_DIRS "."
                                    "../../../../azure_board_apps/graphics/3d_matrix/3d_matrix_data"
                                    "../../../../azure_board_apps/graphics/3d_matrix/3d_matrix_src")
                                    
add_compile_definitions(CONFIG_3D_OBJECT_CUBE=y)