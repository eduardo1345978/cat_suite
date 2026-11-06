# Code shared by all three plug-ins. Compiled into each plug-in target (INTERFACE sources),
# so every plug-in stays a self-contained binary.
if(NOT TARGET cat_shared)
    add_library(cat_shared INTERFACE)

    target_sources(cat_shared INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/Source/CatLookAndFeel.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/CatTheme.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/CatWidgets.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/ScaledEditor.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/CatVisualizer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/Surprise.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/MidiLearn.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/MidiScheduler.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/MidiExport.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/PresetBar.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/CatLink.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/ComputerKeyboard.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/LinkBus.cpp
        ${CMAKE_CURRENT_LIST_DIR}/Source/LinkClient.cpp)

    target_include_directories(cat_shared INTERFACE ${CMAKE_CURRENT_LIST_DIR}/Source)

    # each Cat's own logo font (SIL Open Font License: Fonts/OFL.txt)
    file(GLOB _cat_fonts ${CMAKE_CURRENT_LIST_DIR}/Fonts/*.ttf)
    juce_add_binary_data(cat_fonts HEADER_NAME CatFonts.h NAMESPACE CatFonts SOURCES ${_cat_fonts})
    set_target_properties(cat_fonts PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(cat_shared INTERFACE cat_fonts)
endif()
