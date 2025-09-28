add_library(UI STATIC
        SimpleChatMainWindow.cpp
        SimpleChatMainWindow.h
        SimpleChatMainWindow.ui)

target_link_libraries(UI PRIVATE Core Qt6::Widgets Qt6::Network)


