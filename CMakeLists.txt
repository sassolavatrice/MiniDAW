cmake_minimum_required(VERSION 3.22)
project(MiniDAW VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # utile per clangd / VS Code

# ------------------------------------------------------------------
# JUCE
# Se hai già JUCE sul disco:  cmake -B build -DJUCE_DIR=/percorso/JUCE
# Altrimenti viene scaricato automaticamente da GitHub (serve internet
# solo la prima volta).
# ------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Percorso a una copia locale di JUCE (opzionale)")

if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.6
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Sorgenti del motore audio: NON dipendono dalla GUI, così li usiamo
# sia nell'app che nel renderer da riga di comando.
set(ENGINE_SOURCES
    Source/Engine/AudioEngine.cpp)

set(COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# ------------------------------------------------------------------
# 1) L'applicazione con interfaccia grafica
# ------------------------------------------------------------------
juce_add_gui_app(MiniDAW
    PRODUCT_NAME "MiniDAW"
    COMPANY_NAME "Progetto Rielaborazione dei Suoni")

target_sources(MiniDAW PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    ${ENGINE_SOURCES})

target_include_directories(MiniDAW PRIVATE Source)
target_compile_definitions(MiniDAW PRIVATE
    ${COMMON_DEFINITIONS}
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MiniDAW,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MiniDAW,JUCE_VERSION>")

target_link_libraries(MiniDAW
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------
# 2) Renderer offline da riga di comando (senza scheda audio né GUI)
#    Serve per provare gli effetti su file e ascoltare/analizzare il
#    risultato (es. in Audacity o con Python).
# ------------------------------------------------------------------
juce_add_console_app(MiniDAWRender PRODUCT_NAME "MiniDAWRender")

target_sources(MiniDAWRender PRIVATE
    Source/RenderMain.cpp
    ${ENGINE_SOURCES})

target_include_directories(MiniDAWRender PRIVATE Source)
target_compile_definitions(MiniDAWRender PRIVATE ${COMMON_DEFINITIONS})

target_link_libraries(MiniDAWRender
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
