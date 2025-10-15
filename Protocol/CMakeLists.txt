# Protocol/CMakeLists.txt - ћ»Ќ»ћјЋ№Ќјя ¬≈–—»я
add_library(monitor_protocol STATIC
    MonitorClient.cpp
    MonitorServer.cpp
)

target_compile_features(monitor_protocol PUBLIC cxx_std_17)
target_include_directories(monitor_protocol PUBLIC ${CMAKE_CURRENT_LIST_DIR})

if(WIN32)
    find_package(Boost REQUIRED CONFIG COMPONENTS asio system)
    target_link_libraries(monitor_protocol PUBLIC Boost::asio Boost::system)
else()
    # Ќа Linux просто находим Boost и используем заголовки
    find_package(Boost REQUIRED)
    target_include_directories(monitor_protocol PUBLIC ${Boost_INCLUDE_DIRS})
endif()

if(WIN32)
    target_link_libraries(monitor_protocol PRIVATE ws2_32)
else()
    find_package(Threads REQUIRED)
    target_link_libraries(monitor_protocol PRIVATE Threads::Threads)
    set_target_properties(monitor_protocol PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()