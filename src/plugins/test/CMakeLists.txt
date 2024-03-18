set(test_plugin_SRCS
    kglobalaccel_test.cpp
    kglobalaccel_test.h
    ../../logging.cpp
)

add_library(KGlobalAccelDTest STATIC ${test_plugin_SRCS})
target_link_libraries(KGlobalAccelDTest
    KGlobalAccelD
    KF6::WindowSystem
)

install(
    TARGETS
        KGlobalAccelDTest
    DESTINATION
        ${KDE_INSTALL_PLUGINDIR}/org.kde.kglobalacceld.platforms/
)
