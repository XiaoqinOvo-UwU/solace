cmake_minimum_required(VERSION 3.21)

project(Solace VERSION 5.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 required modules
find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Widgets Network Svg QuickControls2 Concurrent)

# ---- core app sources ----
set(APP_SOURCES
    src/main.cpp
    src/Core/AppCore.h
    src/Core/AppCore.cpp
    src/Services/ProxyService.h
    src/Services/ProxyService.cpp
    src/Services/NetworkService.h
    src/Services/NetworkService.cpp
    src/Services/SystemService.h
    src/Services/SystemService.cpp
    src/Services/MoodService.h
    src/Services/MoodService.cpp
    src/Services/ConfigService.h
    src/Services/ConfigService.cpp
    src/Services/UpdateService.h
    src/Services/UpdateService.cpp
    src/Services/SyncService.h
    src/Services/SyncService.cpp
    src/Services/PluginManager.h
    src/Services/PluginManager.cpp
    src/Services/AiService.h
    src/Services/AiService.cpp
    src/Services/FactModel.h
    src/Services/MemoryRetriever.h
    src/Services/MemoryRetriever.cpp
    src/Services/MemoryStore.h
    src/Services/MemoryStore.cpp
    src/Services/MemoryImportanceEvaluator.h
    src/Services/MemoryImportanceEvaluator.cpp
    src/Services/MemoryConflictManager.h
    src/Services/MemoryConflictManager.cpp
    src/Services/ConversationState.h
    src/Services/ConversationStateManager.h
    src/Services/ConversationStateManager.cpp
    src/Services/ContextManager.h
    src/Services/ContextManager.cpp
    src/Services/FactFilter.h
    src/Services/FactFilter.cpp
    src/Services/ResponseValidator.h
    src/Services/ResponseValidator.cpp
    src/Services/ResponseRepair.h
    src/Services/ResponseRepair.cpp
    src/Services/StatsService.h
    src/Services/StatsService.cpp
    src/Services/ContactService.h
    src/Services/ContactService.cpp
    src/Services/AppAnalyzer.h
    src/Services/AppAnalyzer.cpp
    src/Services/ActivityMemory.h
    src/Services/ActivityMemory.cpp
    src/Services/DoNotDisturbManager.h
    src/Services/DoNotDisturbManager.cpp
    src/Services/ProactiveScore.h
    src/Services/ProactiveScore.cpp
    src/Services/MoodTrend.h
    src/Services/MoodTrend.cpp
    src/Services/RelationshipState.h
    src/Services/RelationshipState.cpp
    src/Services/TopicGenerator.h
    src/Services/TopicGenerator.cpp
)

qt_add_executable(Solace WIN32
    ${APP_SOURCES}
    qml/Main.qml
    qml/Components/
    qml/Pages/
    resources/resources.qrc
)

# per-compiler warnings/optimisation (no global CMAKE_CXX_FLAGS pollution;
# -Werror intentionally NOT enabled while legacy warnings remain)
target_compile_options(Solace PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-O2>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
)

# embed the app icon (cute icon) into the exe
if(WIN32)
    target_sources(Solace PRIVATE resources/app.rc)
endif()

target_compile_definitions(Solace PRIVATE
    QT_DEPRECATED_WARNINGS
    "NIGHTLOG_VERSION=\"${PROJECT_VERSION}\""
)

target_link_libraries(Solace PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::QuickControls2
    Qt6::Concurrent
    winmm
    dwmapi
    crypt32
)

# QML deployment
qt_finalize_executable(Solace)


















