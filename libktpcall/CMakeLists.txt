find_package(PkgConfig REQUIRED)
pkg_check_modules(FARSTREAM REQUIRED farstream-0.2)

include_directories(${CMAKE_CURRENT_BINARY_DIR}
        ${FARSTREAM_INCLUDE_DIRS}
)

#####

set(libktpcall_SRCS
    call-channel-handler.cpp
    call-content-handler.cpp
    volume-controller.cpp

    private/device-element-factory.cpp
    private/phonon-integration.cpp
    private/sink-controllers.cpp
    private/sink-manager.cpp
    private/tf-audio-content-handler.cpp
    private/tf-channel-handler.cpp
    private/tf-content-handler.cpp
    private/tf-video-content-handler.cpp
    private/video-sink-bin.cpp
)

kde4_add_library(ktpcall STATIC ${libktpcall_SRCS})
target_link_libraries(ktpcall
    ${KDE4_KDECORE_LIBS}
    ${KDE4_PHONON_LIBRARY}
    ${QT_QTDBUS_LIBRARY}
    ${QTGSTREAMER_LIBRARIES}
    ${TELEPATHY_QT4_LIBRARIES}
    qtf
)

#####

add_subdirectory(tests)
