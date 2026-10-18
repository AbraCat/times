find_path(Asio_INCLUDE_DIR
    NAMES asio.hpp
    PATHS /usr/include /usr/local/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio
    REQUIRED_VARS Asio_INCLUDE_DIR
)

if(Asio_FOUND AND NOT TARGET asio::asio)
    add_library(asio::asio INTERFACE IMPORTED)
    
    # Задаем пути к заголовочным файлам
    set_target_properties(asio::asio PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${Asio_INCLUDE_DIR}"
    )

    # В Debian libasio-dev требует макрос standalone, чтобы не искать Boost
    set_target_properties(asio::asio PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS "ASIO_STANDALONE"
    )

    # Asio критически зависит от системных потоков (pthread на Linux)
    find_package(Threads REQUIRED)
    target_link_libraries(asio::asio INTERFACE Threads::Threads)
endif()
