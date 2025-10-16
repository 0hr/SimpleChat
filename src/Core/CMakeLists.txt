add_library(Core STATIC
        IChatTransport.h
        UdpPeerTransport.h
        UdpPeerTransport.cpp
)

target_link_libraries(Core PRIVATE Qt6::Core Qt6::Network)
