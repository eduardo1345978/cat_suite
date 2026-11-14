# Cat Suite Setup: installs, updates and removes the Cats (VST3 plug-ins and apps) from the GitHub release.
juce_add_gui_app(CatSetup
    PRODUCT_NAME "Cat-Suite-Setup"
    COMPANY_NAME "Cat Suite"
    BUNDLE_ID    com.catsuite.setup
    ICON_BIG     "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL   "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon_small.png"
    VERSION      1.0.0)

target_sources(CatSetup PRIVATE Source/Main.cpp Source/Setup.cpp)
target_include_directories(CatSetup PRIVATE Source)
target_compile_features(CatSetup PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(CatSetup PRIVATE /utf-8)
endif()

# the commit this Setup was built from (CI passes it), so it knows when a newer Setup is out
set(CAT_COMMIT "" CACHE STRING "The git commit of this build")
target_compile_definitions(CatSetup PRIVATE
    CAT_COMMIT="${CAT_COMMIT}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CatSetup,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CatSetup,JUCE_VERSION>")

target_link_libraries(CatSetup
    PRIVATE
        cat_fonts
        juce::juce_gui_extra
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
