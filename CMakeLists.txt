# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("VOIPMessenger")

set(CMAKE_CXX_STANDARD 20)
set(REQUIRED_QT_VERSION 6.9.1)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH $ENV{QTDIR})
link_directories("C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64")
link_directories("C:/Qt/6.9.1/msvc2022_64/lib")
link_directories("C:/Qt/6.9.1/msvc2022_64/include")
find_package(Qt6Core REQUIRED)
find_package(Qt6Widgets REQUIRED)

add_executable(VOIPMessenger WIN32 "VOIPMessenger/VOIPMessenger.cpp" "VOIPMessenger/VOIPMessenger.h" "VOIPMessenger/Controller.cpp" "VOIPMessenger/Controller.h" "VOIPMessenger/MainWindow.cpp" "VOIPMessenger/MainWindow.h" "VOIPMessenger/MainWindow.ui" "VOIPMessenger/NetworkerTCP.cpp" "VOIPMessenger/NetworkerTCP.h"  "VOIPMessenger/FileManager.cpp" "VOIPMessenger/FileManager.h" "VOIPMessenger/AudioInput.cpp" "VOIPMessenger/AudioInput.h" "VOIPMessenger/AudioOutput.cpp" "VOIPMessenger/AudioOutput.h" "VOIPMessenger/VoiceCallback.cpp" "VOIPMessenger/VoiceCallback.h" "VOIPMessenger/NetworkerUDP.cpp" "VOIPMessenger/NetworkerUDP.h")
target_link_libraries(VOIPMessenger Ws2_32.lib xaudio2.lib Qt6::Widgets)
include_directories("VOIPMessenger")
