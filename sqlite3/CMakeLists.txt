# CMakeLists.txt for the sqlite3 component

# Register the component sources
idf_component_register(
    SRCS
        "sqlite3.c"
        "esp32.cpp"
        "shox96_0_2.cpp"
        "unishox1.c"
    INCLUDE_DIRS
        "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-missing-field-initializers
    -Wno-implicit-fallthrough
    -Wno-type-limits
)
