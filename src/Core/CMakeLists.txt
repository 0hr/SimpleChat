add_library(Core STATIC
        IChatTransport.h
        MessageSerializer.h
        MessageSerializer.cpp
        RingTcpTransport.cpp
        RingTcpTransport.h
)

target_link_libraries(Core PRIVATE Qt6::Core Qt6::Network)