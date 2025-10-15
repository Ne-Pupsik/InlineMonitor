# Hook/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# === библиотека хука ===
add_library(Observer SHARED
    observer.cpp

    # заголовки для IDE (реализация в заголовках подключается из observer.cpp)
    IFileMonitor.h
    inlineWinHook.h
    WinFileMonitor.h
    LinFileMonitor.h
)

target_compile_features(Observer PUBLIC cxx_std_17)
target_include_directories(Observer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Если у вас есть таргет из Protocol — линкуем его (он должен быть STATIC)
# Переименуйте при необходимости или закомментируйте.
target_link_libraries(Observer PUBLIC monitor_protocol)

# Для *nix включаем PIC на всякий случай (если подтянутся стат. зависимости)
set_target_properties(Observer PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- опции проекта ---
option(HOOK_USE_CAPSTONE_LINUX "Link capstone on Linux too" OFF)
option(HOOK_COPY_RUNTIME_DLLS  "Copy runtime DLLs next to Observer on Windows" OFF)

# --- платформенные настройки ---
if (WIN32)
  # узкий windows.h и мин. версия
  target_compile_definitions(Observer PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0601   # поднимите при необходимости
  )

  # просим vcpkg отдавать статические либы (должно стоять ДО find_package)
  set(VCPKG_PREFER_STATIC_LIBS ON)

  # capstone из vcpkg
  find_package(capstone CONFIG REQUIRED)
  target_link_libraries(Observer PRIVATE capstone::capstone)

  # статический CRT, чтобы DLL была самодостаточной
  set_property(TARGET Observer PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  # если нет своих __declspec(dllexport)
  set_target_properties(Observer PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

  # (необязательно) — скопировать runtime DLL'ки рядом с Observer.dll, если всё же используете динамику
  if (HOOK_COPY_RUNTIME_DLLS)
    add_custom_command(TARGET Observer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              $<TARGET_RUNTIME_DLLS:Observer>
              $<TARGET_FILE_DIR:Observer>
      COMMAND_EXPAND_LISTS)
  endif()

else() # Linux / Unix
  # dlopen/dlsym/RTLD_NEXT
  target_link_libraries(Observer PRIVATE dl)
  
endif()

# единое имя артефакта
set_target_properties(Observer PROPERTIES OUTPUT_NAME "Observer")