CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

SET(HEADERS
        util.h
        gui.h
        ini.h
        hash.h
        find.h
        video.h
        audio.h
        image.h
        cache.h
        )

SET(SOURCES
        util.c
        gui.c
        ini.c
        hash.c
        phash.c
        find.c
        video.c
        audio.c
        image.c
        cache.c
        main.c
        )

IF (WIN32)
    SET(HEADERS
            image-win.h
            ${HEADERS}
            )
    SET(SOURCES
            image-win.c
            main.rc
            ${SOURCES}
            )
    ADD_EXECUTABLE(fdupves WIN32 ${HEADERS} ${SOURCES} main.c)
ELSE (WIN32)
    ADD_EXECUTABLE(fdupves ${HEADERS} ${SOURCES} main.c)
ENDIF (WIN32)

TARGET_LINK_LIBRARIES(fdupves
        ${REQ_LIBRARIES}
        ${GTK_LIBRARIES}
        ${FFMPEG_LIBRARIES}
        )

INSTALL(TARGETS fdupves DESTINATION bin)
IF (WIN32)
    FIND_FILE(LIBGTK pkg-config.exe)
    GET_FILENAME_COMPONENT(GTK_BIN ${LIBGTK} DIRECTORY)
    GET_FILENAME_COMPONENT(GTK_ROOT ${GTK_BIN} DIRECTORY)

    INSTALL(CODE "set(MY_DEPENDENCY_PATHS ${GTK_BIN} ${FFMPEG_DIR}/bin)")

    INSTALL(CODE [[
    LIST(APPEND pre_exclude_regexes "api-ms-.*") # windows API
    LIST(APPEND pre_exclude_regexes "ext-ms-.*") # windows API
    LIST(APPEND pre_exclude_regexes "ieshims.dll") # windows API
    LIST(APPEND pre_exclude_regexes "emclient.dll") # windows API
    LIST(APPEND pre_exclude_regexes "devicelockhelpers.dll") # windows API

    LIST(APPEND post_exclude_regexes ".*WINDOWS[\\/]system32.*")

    FILE(GET_RUNTIME_DEPENDENCIES
      EXECUTABLES ${CMAKE_CURRENT_BINARY_DIR}/fdupves.exe
      RESOLVED_DEPENDENCIES_VAR _r_deps
      UNRESOLVED_DEPENDENCIES_VAR _u_deps
      PRE_EXCLUDE_REGEXES ${pre_exclude_regexes}
      POST_EXCLUDE_REGEXES ${post_exclude_regexes}
      DIRECTORIES ${MY_DEPENDENCY_PATHS}
      )
    LIST(LENGTH _u_deps _u_length)
    IF("${_u_length}" GREATER 0)
      MESSAGE(WARNING "Unresolved dependencies detected!")
      FOREACH(_no ${_u_deps})
        MESSAGE(INFO ${_no})
      ENDFOREACH()
      MESSAGE (WARNING ${MY_DEPENDENCY_PATHS})
    ENDIF()
    FOREACH(_file ${_r_deps})
      MESSAGE("install " ${_file})
      FILE(INSTALL FILES "${_file}"
        DESTINATION "${CMAKE_INSTALL_PREFIX}"
        )
    ENDFOREACH()
    ]])

    INSTALL(CODE "MESSAGE(\"install additional gtk3 files.\")")
    INSTALL(DIRECTORY ${GTK_ROOT}/share/glib-2.0/schemas
            DESTINATION share/glib-2.0)

    INSTALL(FILES ${GTK_ROOT}/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache
            DESTINATION lib/gdk-pixbuf-2.0/2.10.0)
    INSTALL(FILES ${GTK_ROOT}/share/icons/Adwaita/index.theme
            DESTINATION share/icons/Adwaita)
    INSTALL(DIRECTORY ${GTK_ROOT}/share/icons/Adwaita/16x16
            DESTINATION share/icons/Adwaita)

    INSTALL(FILES ${GTK_ROOT}/share/locale/zh_CN/LC_MESSAGES/gtk30.mo
            DESTINATION share/locale/zh_CN/LC_MESSAGES)
ENDIF (WIN32)

