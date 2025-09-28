add_library(Controllers STATIC ChatController.cpp ChatController.h)

target_link_libraries(Controllers PRIVATE Core Qt6::Core Qt6::Network)