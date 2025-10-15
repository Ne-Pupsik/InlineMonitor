# Monitor/CMakeLists.txt

add_library(Monitor STATIC
    Monitor.cpp
    Monitor.h
    injector.h
)

if(WIN32)
    target_sources(Monitor PRIVATE winInjector.cpp winInjector.h)
    target_compile_definitions(Monitor PRIVATE WINDOWS_BUILD)
elseif(UNIX AND NOT APPLE)
    target_sources(Monitor PRIVATE gnuInjector.cpp gnuInjector.h)
    target_compile_definitions(Monitor PRIVATE LINUX_BUILD)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# инклюды самой папки Monitor
target_include_directories(Monitor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# ВАЖНО: библиотека Monitor использует MonitorServer.h из Protocol,
# поэтому она должна ссылаться на monitor_protocol (PUBLIC, чтобы
# потребителям Monitor не пришлось добавлять Protocol вручную).
target_link_libraries(Monitor PUBLIC monitor_protocol)

# Исполняемый HMonitor (если он есть в этом каталоге)
add_executable(HMonitor HMonitor.cpp)

# Ему достаточно линковаться на Monitor (Protocol подтянется транзитивно)
target_link_libraries(HMonitor PRIVATE Monitor)
