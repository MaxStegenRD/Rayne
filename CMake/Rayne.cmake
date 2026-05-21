if((CMAKE_SYSTEM_NAME STREQUAL "visionOS"))
    set(VISIONOS 1)
endif()

set(DIR_OF_RAYNE_CMAKE ${CMAKE_CURRENT_LIST_DIR})

find_package(Python COMPONENTS Interpreter)

function(rayne_get_android_assets_directory _OUT_VARIABLE)
    if(NOT RAYNE_ANDROID_ASSETS_DIRECTORY)
        message(FATAL_ERROR "RAYNE_ANDROID_ASSETS_DIRECTORY must be set for Android builds.")
    endif()

    cmake_path(CONVERT "${RAYNE_ANDROID_ASSETS_DIRECTORY}" TO_CMAKE_PATH_LIST _ANDROID_ASSETS_DIRECTORY NORMALIZE)
    set("${_OUT_VARIABLE}" "${_ANDROID_ASSETS_DIRECTORY}" PARENT_SCOPE)
endfunction()

macro(rayne_link_with _TARGET)
    if(IOS OR VISIONOS)
        get_target_property(CURRENT_EMBED_FRAMEWORKS ${_TARGET} XCODE_EMBED_FRAMEWORKS)
        if(CURRENT_EMBED_FRAMEWORKS)
            set_property(TARGET ${_TARGET} PROPERTY XCODE_EMBED_FRAMEWORKS ${CURRENT_EMBED_FRAMEWORKS} Rayne ${_TARGET}Lib)
        else()
            set_property(TARGET ${_TARGET} PROPERTY XCODE_EMBED_FRAMEWORKS Rayne ${_TARGET}Lib)
        endif()
        set_target_properties(${_TARGET} PROPERTIES XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY ON)
        set_target_properties(${_TARGET} PROPERTIES XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON)

        target_link_libraries(${_TARGET}Lib PUBLIC Rayne)
    else()
        add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:Rayne>" "$<TARGET_FILE_DIR:${_TARGET}>/$<TARGET_FILE_NAME:Rayne>")
        target_link_libraries(${_TARGET} PRIVATE Rayne)
    endif()

    if(APPLE)
        set_property(TARGET ${_TARGET} PROPERTY BUILD_WITH_INSTALL_RPATH TRUE)
        set_property(TARGET ${_TARGET} APPEND PROPERTY INSTALL_RPATH "@executable_path")
        if(IOS OR VISIONOS)
            set_property(TARGET ${_TARGET} APPEND PROPERTY INSTALL_RPATH "@executable_path/Frameworks")
        endif()
    elseif(UNIX AND NOT ANDROID)
        set_property(TARGET ${_TARGET} PROPERTY BUILD_WITH_INSTALL_RPATH TRUE)
        set_property(TARGET ${_TARGET} APPEND PROPERTY INSTALL_RPATH "\$ORIGIN")
    endif()

    if(ANDROID)
        set(_ANDROID_APP_GLUE_TARGET ${_TARGET}-android-app-glue)
        add_library(${_ANDROID_APP_GLUE_TARGET} OBJECT ${DIR_OF_RAYNE_CMAKE}/../Vendor/android_native_app_glue/android_native_app_glue.c)
        set_target_properties(${_ANDROID_APP_GLUE_TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        target_sources(${_TARGET} PRIVATE $<TARGET_OBJECTS:${_ANDROID_APP_GLUE_TARGET}>)
        target_link_libraries(${_TARGET} PRIVATE android log)

        set_property(TARGET "${_TARGET}" APPEND_STRING PROPERTY LINK_FLAGS " -u ANativeActivity_onCreate")
    endif()

    if(WIN32)
        set_target_properties(${_TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/${_TARGET}"
                LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Debug/${_TARGET}"
                ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Debug/${_TARGET}")

        set_target_properties(${_TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RelWithDebInfo/${_TARGET}"
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/RelWithDebInfo/${_TARGET}"
                RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/RelWithDebInfo/${_TARGET}")

        set_target_properties(${_TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/${_TARGET}"
                LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Release/${_TARGET}"
                ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Release/${_TARGET}")
    else()
        set_target_properties(${_TARGET} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${_TARGET}"
                LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${_TARGET}"
                ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${_TARGET}")
    endif()
endmacro()

macro(INTERNAL_rayne_copy_module_resources _TARGET _MODULE _DIRECTORY)
    foreach(_RESOURCE ${${_MODULE}_MODULE_RESOURCES})
        add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E echo "from $<TARGET_FILE_DIR:${_MODULE}>/${_RESOURCE} to ${_DIRECTORY}/${_MODULE}/${_RESOURCE}")
        add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:${_MODULE}>/${_RESOURCE}" "${_DIRECTORY}/${_MODULE}/${_RESOURCE}")
    endforeach()
endmacro()

macro(rayne_use_modules _TARGET _MODULES)
    foreach(_MODULE ${_MODULES})
        set(_MODULE_TARGET "${_MODULE}")
        
        if(IOS OR VISIONOS)
            target_link_libraries(${_TARGET}Lib PUBLIC ${_MODULE_TARGET})

            set(_RAYNE_XCODE_EMBED_FRAMEWORKS ${_MODULE_TARGET})
            get_target_property(_MODULE_XCODE_EMBED_FRAMEWORKS ${_MODULE_TARGET} RAYNE_XCODE_EMBED_FRAMEWORKS)
            if(_MODULE_XCODE_EMBED_FRAMEWORKS)
                list(APPEND _RAYNE_XCODE_EMBED_FRAMEWORKS ${_MODULE_XCODE_EMBED_FRAMEWORKS})
            endif()

            get_target_property(CURRENT_EMBED_FRAMEWORKS ${_TARGET} XCODE_EMBED_FRAMEWORKS)
            if(CURRENT_EMBED_FRAMEWORKS)
                set_property(TARGET ${_TARGET} PROPERTY XCODE_EMBED_FRAMEWORKS ${CURRENT_EMBED_FRAMEWORKS} ${_RAYNE_XCODE_EMBED_FRAMEWORKS})
            else()
                set_property(TARGET ${_TARGET} PROPERTY XCODE_EMBED_FRAMEWORKS ${_RAYNE_XCODE_EMBED_FRAMEWORKS})
            endif()
            set_target_properties(${_TARGET} PROPERTIES XCODE_EMBED_FRAMEWORKS_REMOVE_HEADERS_ON_COPY ON)
            set_target_properties(${_TARGET} PROPERTIES XCODE_EMBED_FRAMEWORKS_CODE_SIGN_ON_COPY ON)
        else()
            target_link_libraries(${_TARGET} PRIVATE ${_MODULE_TARGET})
        endif()


        if(APPLE)
            if(IOS OR VISIONOS)
                #Don't copy here for iOS and visionOS, they will already be embeded in the frameworks directory
                #add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:${_MODULE_TARGET}>" "$<TARGET_BUNDLE_CONTENT_DIR:${_TARGET}>/ResourceFiles/Resources/Modules/${_MODULE}/${_MODULE}.framework")
            else()
                add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_FILE_DIR:${_MODULE_TARGET}>" "$<TARGET_BUNDLE_CONTENT_DIR:${_TARGET}>/Resources/Resources/Modules/${_MODULE}")
            endif()
        elseif(NOT ANDROID)
            INTERNAL_rayne_copy_module_resources(${_TARGET} ${_MODULE_TARGET} "$<TARGET_FILE_DIR:${_TARGET}>/Resources/Modules")
        endif()

        if(APPLE AND NOT IOS AND NOT VISIONOS)
            set_property(TARGET ${_TARGET} APPEND PROPERTY INSTALL_RPATH "@executable_path/../Resources/Resources/Modules/${_MODULE}")
        elseif(UNIX AND NOT ANDROID)
            set_property(TARGET ${_TARGET} APPEND PROPERTY INSTALL_RPATH "\$ORIGIN/Resources/Modules/${_MODULE}")
        endif()

        if(WIN32 OR (UNIX AND NOT APPLE AND NOT ANDROID))
            add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${_MODULE_TARGET}>" "$<TARGET_FILE_DIR:${_TARGET}>/$<TARGET_FILE_NAME:${_MODULE_TARGET}>")

            get_target_property(_MODULE_RUNTIME_DEPENDENCY_FILES ${_MODULE_TARGET} RAYNE_RUNTIME_DEPENDENCY_FILES)
            if(_MODULE_RUNTIME_DEPENDENCY_FILES)
                foreach(_MODULE_RUNTIME_DEPENDENCY_FILE ${_MODULE_RUNTIME_DEPENDENCY_FILES})
                    add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_MODULE_RUNTIME_DEPENDENCY_FILE}" "$<TARGET_FILE_DIR:${_TARGET}>")
                endforeach()
            endif()

            get_target_property(_MODULE_RUNTIME_DEPENDENCY_DIRECTORIES ${_MODULE_TARGET} RAYNE_RUNTIME_DEPENDENCY_DIRECTORIES)
            if(_MODULE_RUNTIME_DEPENDENCY_DIRECTORIES)
                foreach(_MODULE_RUNTIME_DEPENDENCY_DIRECTORY ${_MODULE_RUNTIME_DEPENDENCY_DIRECTORIES})
                    get_filename_component(_MODULE_RUNTIME_DEPENDENCY_DIRECTORY_NAME "${_MODULE_RUNTIME_DEPENDENCY_DIRECTORY}" NAME)
                    add_custom_command(TARGET ${_TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${_MODULE_RUNTIME_DEPENDENCY_DIRECTORY}" "$<TARGET_FILE_DIR:${_TARGET}>/${_MODULE_RUNTIME_DEPENDENCY_DIRECTORY_NAME}")
                endforeach()
            endif()
        endif()

        if(ANDROID)
            rayne_get_android_assets_directory(android-assets-dir)
            set(_MODULE_RESOURCE_COPY_TARGET "${_TARGET}-${_MODULE_TARGET}-Resources")

            if(NOT TARGET "${_MODULE_RESOURCE_COPY_TARGET}")
                set(_MODULE_RESOURCE_COPY_COMMANDS)
                get_target_property(_MODULE_RESOURCE_SOURCE_DIRECTORY ${_MODULE_TARGET} SOURCE_DIR)
                foreach(_RESOURCE ${${_MODULE}_MODULE_RESOURCES})
                    set(_MODULE_RESOURCE_SOURCE "${_MODULE_RESOURCE_SOURCE_DIRECTORY}/${_RESOURCE}")
                    set(_MODULE_RESOURCE_DESTINATION "${android-assets-dir}/Resources/Modules/${_MODULE}/${_RESOURCE}")
                    get_filename_component(_MODULE_RESOURCE_DESTINATION_DIRECTORY "${_MODULE_RESOURCE_DESTINATION}" DIRECTORY)

                    list(APPEND _MODULE_RESOURCE_COPY_COMMANDS
                        COMMAND ${CMAKE_COMMAND} -E make_directory "${_MODULE_RESOURCE_DESTINATION_DIRECTORY}")

                    if(IS_DIRECTORY "${_MODULE_RESOURCE_SOURCE}")
                        list(APPEND _MODULE_RESOURCE_COPY_COMMANDS
                            COMMAND ${CMAKE_COMMAND} -E rm -rf "${_MODULE_RESOURCE_DESTINATION}"
                            COMMAND ${CMAKE_COMMAND} -E copy_directory "${_MODULE_RESOURCE_SOURCE}" "${_MODULE_RESOURCE_DESTINATION}")
                    elseif(EXISTS "${_MODULE_RESOURCE_SOURCE}")
                        list(APPEND _MODULE_RESOURCE_COPY_COMMANDS
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_MODULE_RESOURCE_SOURCE}" "${_MODULE_RESOURCE_DESTINATION}")
                    else()
                        message(FATAL_ERROR "Android asset source does not exist: ${_MODULE_RESOURCE_SOURCE}")
                    endif()
                endforeach()

                if(_MODULE_RESOURCE_COPY_COMMANDS)
                    add_custom_target(${_MODULE_RESOURCE_COPY_TARGET}
                        ${_MODULE_RESOURCE_COPY_COMMANDS}
                        COMMENT "Copying Android resources for ${_MODULE_TARGET}"
                        VERBATIM)
                endif()
            endif()

            if(TARGET "${_MODULE_RESOURCE_COPY_TARGET}")
                add_dependencies(${_TARGET} ${_MODULE_RESOURCE_COPY_TARGET})
            endif()
        endif()
    endforeach()
endmacro()

macro(rayne_copy_resources _TARGET _RESOURCES _ADDITIONAL_PACK_PARAMS)
    set(_RESOURCE_COPY_TARGET "${_TARGET}-Resources")
    if(TARGET "${_RESOURCE_COPY_TARGET}")
        message(FATAL_ERROR "rayne_copy_resources was called more than once for target ${_TARGET}")
    endif()

    set(_RESOURCE_COPY_COMMANDS)

    if(ANDROID)
        rayne_get_android_assets_directory(android-assets-dir)
        file(MAKE_DIRECTORY "${android-assets-dir}")

        foreach(_PACK_PARAM ${_ADDITIONAL_PACK_PARAMS})
            if(_PACK_PARAM MATCHES "^--resourcespec=(.+)$")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_MATCH_1}")
            endif()
        endforeach()
    endif()

    foreach(_RESOURCE ${_RESOURCES})
        set(_RESOURCE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/${_RESOURCE}")

        if(ANDROID)
            set(_RESOURCE_DESTINATION "${android-assets-dir}/${_RESOURCE}")
            set(_RESOURCE_PLATFORM android)
            set(_REMOVE_RESOURCE_DESTINATION TRUE)
        elseif(APPLE)
            if(IOS OR VISIONOS)
                string(FIND ${CMAKE_OSX_SYSROOT} "XROS" IS_VISIONOS)
                string(FIND ${CMAKE_OSX_SYSROOT} "iPhoneSimulator" IS_IOS_SIMULATOR)
                string(FIND ${CMAKE_OSX_SYSROOT} "XRSimulator" IS_VISIONOS_SIMULATOR)

                #Different apple platforms require different shader binaries...
                set(_RESOURCE_PLATFORM ios)
                if(IS_IOS_SIMULATOR GREATER -1)
                    set(_RESOURCE_PLATFORM ios_sim)
                elseif(IS_VISIONOS GREATER -1)
                    set(_RESOURCE_PLATFORM visionos)
                elseif(IS_VISIONOS_SIMULATOR GREATER -1)
                    set(_RESOURCE_PLATFORM visionos_sim)
                endif()

                set(_RESOURCE_DESTINATION "$<TARGET_BUNDLE_CONTENT_DIR:${_TARGET}>/ResourceFiles/${_RESOURCE}")
            else()
                set(_RESOURCE_DESTINATION "$<TARGET_BUNDLE_CONTENT_DIR:${_TARGET}>/Resources/${_RESOURCE}")
                set(_RESOURCE_PLATFORM macos)
            endif()

            set(_REMOVE_RESOURCE_DESTINATION FALSE)
        elseif(WIN32)
            set(_RESOURCE_DESTINATION "$<TARGET_FILE_DIR:${_TARGET}>/${_RESOURCE}")
            set(_RESOURCE_PLATFORM windows)
            set(_REMOVE_RESOURCE_DESTINATION FALSE)
        else()
            set(_RESOURCE_DESTINATION "$<TARGET_FILE_DIR:${_TARGET}>/${_RESOURCE}")
            set(_RESOURCE_PLATFORM linux)
            set(_REMOVE_RESOURCE_DESTINATION FALSE)
        endif()

        get_filename_component(_RESOURCE_DESTINATION_DIRECTORY "${_RESOURCE_DESTINATION}" DIRECTORY)
        list(APPEND _RESOURCE_COPY_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_RESOURCE_DESTINATION_DIRECTORY}")

        if(IS_DIRECTORY "${_RESOURCE_SOURCE}")
            if(_REMOVE_RESOURCE_DESTINATION)
                list(APPEND _RESOURCE_COPY_COMMANDS
                    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_RESOURCE_DESTINATION}")
            endif()

            list(APPEND _RESOURCE_COPY_COMMANDS
                COMMAND ${Python_EXECUTABLE} "${DIR_OF_RAYNE_CMAKE}/../Tools/ResourcePacker/pack.py" "${_RESOURCE_SOURCE}" "${_RESOURCE_DESTINATION}" "${_RESOURCE_PLATFORM}" ${_ADDITIONAL_PACK_PARAMS})
        elseif(EXISTS "${_RESOURCE_SOURCE}")
            list(APPEND _RESOURCE_COPY_COMMANDS
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_RESOURCE_SOURCE}" "${_RESOURCE_DESTINATION}")
        else()
            message(FATAL_ERROR "Resource source does not exist: ${_RESOURCE_SOURCE}")
        endif()
    endforeach()

    if(_RESOURCE_COPY_COMMANDS)
        add_custom_target(${_RESOURCE_COPY_TARGET}
            ${_RESOURCE_COPY_COMMANDS}
            COMMENT "Copying resources for ${_TARGET}"
            VERBATIM)
        add_dependencies(${_TARGET} ${_RESOURCE_COPY_TARGET})
    endif()
endmacro()
