target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _CONSOLE
        _UNICODE
        UNICODE
        _WIN32_WINNT=0x0A00 # Windows 10
    )
    # suppress errors
    # target_compile_options(${PROJECT_NAME} PRIVATE
    #     /wd4100 # unreferenced formal parameter
    #     /wd4244 # conversion from 'int' to 'char'
    #     /wd4701 # potentially uninitialized local variable 'name' used
    #     /wd4189 # local variable is initialized but not referenced
    # )
    target_compile_options(${PROJECT_NAME} PRIVATE
        "/sdl"
        "/utf-8"
        "/Zi"
        "/permissive-"
        "/Zc:externC-"
        "/Zc:preprocessor"
        "/Zc:__cplusplus"
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        "$<$<CONFIG:DEBUG>:/INCREMENTAL;/DEBUG:FASTLINK;/OPT:NOICF;/OPT:NOREF;/LTCG:off;/TIME;>"
        "$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/LTCG:incremental;/OPT:ICF;/OPT:REF;/TIME;>"
    )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:DEBUG>:DEBUG>
        $<$<CONFIG:RELEASE>:NDEBUG>
    )
    # compiler flags as list
    set(DEBUG_OPTIONS -Og -g)
    set(RELEASE_OPTIONS -O3 -s)
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wno-unknown-pragmas
        -fPIE
        $<$<CONFIG:DEBUG>:${DEBUG_OPTIONS}>
        $<$<CONFIG:RELEASE>:${RELEASE_OPTIONS}>
    )
endif()