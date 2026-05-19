include(FetchContent)

if((CMAKE_SYSTEM_NAME STREQUAL "visionOS"))
    set(VISIONOS 1)
endif()

if(APPLE)
    string(FIND ${CMAKE_OSX_SYSROOT} "iPhoneSimulator" IOS_SIMULATOR)
    string(FIND ${CMAKE_OSX_SYSROOT} "XRSimulator" VISIONOS_SIMULATOR)
    if(IOS_SIMULATOR GREATER -1)
        set(IOS_SIMULATOR 1)
    else()
        set(IOS_SIMULATOR 0)
    endif()
    if(VISIONOS_SIMULATOR GREATER -1)
        set(VISIONOS_SIMULATOR 1)
    else()
        set(VISIONOS_SIMULATOR 0)
    endif()
    if(IOS AND IOS_SIMULATOR)
        set(APPLE_SDK_NAME iphonesimulator)
    elseif(IOS)
        set(APPLE_SDK_NAME iphoneos)
    elseif(VISIONOS AND VISIONOS_SIMULATOR)
        set(APPLE_SDK_NAME xrsimulator)
    elseif(VISIONOS)
        set(APPLE_SDK_NAME xros)
    else()
        set(APPLE_SDK_NAME )
    endif()
endif()

#Different apple platforms require different shader binaries...
set(RN_IOS_SHADER_TYPE ios)
if(IS_IOS_SIMULATOR GREATER -1)
    set(RN_IOS_SHADER_TYPE ios_sim)
elseif(IS_VISIONOS GREATER -1)
    set(RN_IOS_SHADER_TYPE visionos)
elseif(IS_VISIONOS_SIMULATOR GREATER -1)
    set(RN_IOS_SHADER_TYPE visionos_sim)
endif()

function(rayne_fetch_content _NAME)
    FetchContent_Declare("${_NAME}"
        ${ARGN}
        EXCLUDE_FROM_ALL
        SYSTEM)

    block(SCOPE_FOR VARIABLES)
        set(CMAKE_SKIP_INSTALL_RULES TRUE)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)
        FetchContent_MakeAvailable("${_NAME}")
    endblock()
endfunction()

macro(__rayne_link_target_libraries _TARGET_NAME _TYPE _VISIBILITY _LIBRARIES)
    foreach(LIBRARY ${_LIBRARIES})
        set(LIBRARY_NAME ${LIBRARY})

        if("${_TYPE}" STREQUAL "STATIC")
            set(LIBRARY_NAME "${LIBRARY_NAME}-static")
        endif()

        target_link_libraries("${_TARGET_NAME}" ${_VISIBILITY} "${LIBRARY_NAME}")
    endforeach()
endmacro()

function(__rayne_get_install_directories _TARGET_NAME _HEADER_DIRECTORY _LIBRARY_DIRECTORY)
    set(INSTALL_COMPONENT "${_TARGET_NAME}")
    string(REGEX REPLACE "-static$" "" INSTALL_COMPONENT "${INSTALL_COMPONENT}")

    if("${INSTALL_COMPONENT}" STREQUAL "Rayne")
        set("${_HEADER_DIRECTORY}" "include/Rayne" PARENT_SCOPE)
        set("${_LIBRARY_DIRECTORY}" "lib/Rayne" PARENT_SCOPE)
    else()
        set("${_HEADER_DIRECTORY}" "include/Modules/${INSTALL_COMPONENT}" PARENT_SCOPE)
        set("${_LIBRARY_DIRECTORY}" "lib/Modules/${INSTALL_COMPONENT}" PARENT_SCOPE)
    endif()
endfunction()

macro(__rayne_create_target _NAME _TYPE _SOURCES _HEADERS _PRIVATE_HEADERS _PUBLIC_LIBRARIES _PRIVATE_LIBRARIES _VERSION _ABI)
    # Input check
    if(NOT (("${_TYPE}" STREQUAL "STATIC") OR ("${_TYPE}" STREQUAL "SHARED")))
        message(FATAL_ERROR "Type must be either \"STATIC\" or \"SHARED\", is \"${_TYPE}\"")
    endif()

    # Create the target name
    set(TARGET_NAME "${_NAME}")

    if("${_TYPE}" STREQUAL "STATIC")
        set(TARGET_NAME "${TARGET_NAME}-static")
    endif()

    # Create the target
    if(APPLE)
        if(POLICY CMP0042)
            cmake_policy(SET CMP0042 NEW) # Set MACOSX_RPATH=YES by default
        endif()

    elseif(UNIX)
    elseif(WIN32)
    endif()

    add_library("${TARGET_NAME}" ${_TYPE})
    target_sources("${TARGET_NAME}" PRIVATE ${_SOURCES})
    if(NOT ("${_PRIVATE_HEADERS}" STREQUAL ""))
        target_sources("${TARGET_NAME}" PRIVATE ${_PRIVATE_HEADERS})
    endif()
    #set_target_properties("${TARGET_NAME}" PROPERTIES VERSION ${_VERSION} SOVERSION ${_ABI})

    if(IOS OR VISIONOS)
        set_target_properties("${TARGET_NAME}" PROPERTIES FRAMEWORK TRUE)
        set_target_properties("${TARGET_NAME}" PROPERTIES MACOSX_FRAMEWORK_IDENTIFIER com.uberpixel.${TARGET_NAME})
    endif()

    if(UNIX AND NOT APPLE)
        target_compile_options(${TARGET_NAME} PRIVATE -m64)
    endif()

    if(MINGW)
        target_compile_options(${TARGET_NAME} PRIVATE -m64)
    endif()

    __rayne_get_install_directories("${TARGET_NAME}" PUBLIC_HEADER_INSTALL_ROOT RAYNE_TARGET_INSTALL_DIRECTORY)

    if(NOT ("${_HEADERS}" STREQUAL ""))
        if(DEFINED RAYNE_PUBLIC_HEADER_INSTALL_DIRECTORY)
            set(PUBLIC_HEADER_INSTALL_ROOT "${RAYNE_PUBLIC_HEADER_INSTALL_DIRECTORY}")
        endif()

        set(PUBLIC_HEADER_BUILD_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
        set(PUBLIC_HEADER_INSTALL_DIRECTORIES "${PUBLIC_HEADER_INSTALL_ROOT}")
        foreach(HEADER ${_HEADERS})
            get_filename_component(HEADER_DIRECTORY "${HEADER}" DIRECTORY)
            if(NOT ("${HEADER_DIRECTORY}" STREQUAL ""))
                list(APPEND PUBLIC_HEADER_BUILD_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/${HEADER_DIRECTORY}")
                list(APPEND PUBLIC_HEADER_INSTALL_DIRECTORIES "${PUBLIC_HEADER_INSTALL_ROOT}/${HEADER_DIRECTORY}")
            endif()
        endforeach()
        list(REMOVE_DUPLICATES PUBLIC_HEADER_BUILD_DIRECTORIES)
        list(REMOVE_DUPLICATES PUBLIC_HEADER_INSTALL_DIRECTORIES)

        if(IOS OR VISIONOS)
            target_sources("${TARGET_NAME}" PRIVATE ${_HEADERS})
            set_target_properties("${TARGET_NAME}" PROPERTIES PUBLIC_HEADER "${_HEADERS}")
        else()
            target_sources("${TARGET_NAME}" PUBLIC
                FILE_SET public_headers
                TYPE HEADERS
                BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
                FILES ${_HEADERS})
        endif()

        foreach(PUBLIC_HEADER_BUILD_DIRECTORY ${PUBLIC_HEADER_BUILD_DIRECTORIES})
            target_include_directories("${TARGET_NAME}" PUBLIC "$<BUILD_INTERFACE:${PUBLIC_HEADER_BUILD_DIRECTORY}>")
        endforeach()
        foreach(PUBLIC_HEADER_INSTALL_DIRECTORY ${PUBLIC_HEADER_INSTALL_DIRECTORIES})
            target_include_directories("${TARGET_NAME}" PUBLIC "$<INSTALL_INTERFACE:${PUBLIC_HEADER_INSTALL_DIRECTORY}>")
        endforeach()

    endif()

    __rayne_link_target_libraries("${TARGET_NAME}" "${_TYPE}" PUBLIC "${_PUBLIC_LIBRARIES}")
    __rayne_link_target_libraries("${TARGET_NAME}" "${_TYPE}" PRIVATE "${_PRIVATE_LIBRARIES}")

    set(RAYNE_TARGET_INSTALL_ARGS
        EXPORT RayneTargets
        RUNTIME DESTINATION "${RAYNE_TARGET_INSTALL_DIRECTORY}"
        LIBRARY DESTINATION "${RAYNE_TARGET_INSTALL_DIRECTORY}"
        ARCHIVE DESTINATION "${RAYNE_TARGET_INSTALL_DIRECTORY}"
        FRAMEWORK DESTINATION "${RAYNE_TARGET_INSTALL_DIRECTORY}"
        BUNDLE DESTINATION "${RAYNE_TARGET_INSTALL_DIRECTORY}")

    if((NOT ("${_HEADERS}" STREQUAL "")) AND (NOT IOS) AND (NOT VISIONOS))
        list(APPEND RAYNE_TARGET_INSTALL_ARGS
            FILE_SET public_headers DESTINATION "${PUBLIC_HEADER_INSTALL_ROOT}")
    endif()

    install(TARGETS "${TARGET_NAME}" ${RAYNE_TARGET_INSTALL_ARGS})
    if(NOT ("${TARGET_NAME}" STREQUAL "Rayne"))
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../../Build/${TARGET_NAME}/."
            DESTINATION "${RAYNE_TARGET_INSTALL_DIRECTORY}")
    endif()

endmacro()


macro(rayne_add_library _NAME _SOURCES _HEADERS _VERSION _ABI)

    cmake_parse_arguments(RAYNE_ADD_LIBRARY "" "" "PRIVATE_HEADERS;PUBLIC_LIBRARIES;PRIVATE_LIBRARIES" ${ARGN})
    if(RAYNE_ADD_LIBRARY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown rayne_add_library arguments: ${RAYNE_ADD_LIBRARY_UNPARSED_ARGUMENTS}")
    endif()

    __rayne_create_target(${_NAME} SHARED "${_SOURCES}" "${_HEADERS}" "${RAYNE_ADD_LIBRARY_PRIVATE_HEADERS}" "${RAYNE_ADD_LIBRARY_PUBLIC_LIBRARIES}" "${RAYNE_ADD_LIBRARY_PRIVATE_LIBRARIES}" "${_VERSION}" "${_ABI}")

endmacro()


macro(rayne_set_module_resources _TARGET _RESOURCES)

    SET(${_TARGET}_MODULE_RESOURCES "" CACHE INTERNAL "")

    foreach(_RESOURCE ${_RESOURCES})

        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${_RESOURCE}")
            if(IOS OR VISIONOS)
                add_custom_command(TARGET ${_TARGET} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${_RESOURCE}" "$<TARGET_FILE_DIR:${_TARGET}>/ResourceFiles/${_RESOURCE}")
            else()
                add_custom_command(TARGET ${_TARGET} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/${_RESOURCE}" "$<TARGET_FILE_DIR:${_TARGET}>/${_RESOURCE}")
            endif()
        else()
            if(IOS OR VISIONOS)
                add_custom_command(TARGET ${_TARGET} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${_RESOURCE}" "$<TARGET_FILE_DIR:${_TARGET}>/ResourceFiles/${_RESOURCE}")
            else()
                add_custom_command(TARGET ${_TARGET} PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/${_RESOURCE}" "$<TARGET_FILE_DIR:${_TARGET}>/${_RESOURCE}")
            endif()
        endif()

        SET(${_TARGET}_MODULE_RESOURCES  ${${_TARGET}_MODULE_RESOURCES} ${_RESOURCE} CACHE INTERNAL "")

    endforeach()

endmacro()


function(rayne_add_public_header_directory _TARGET)
    cmake_parse_arguments(RAYNE_PUBLIC_HEADER_DIRECTORY "" "BUILD_DIRECTORY;INSTALL_DIRECTORY;INSTALL_SOURCE" "INSTALL_OPTIONS" ${ARGN})
    if(RAYNE_PUBLIC_HEADER_DIRECTORY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown rayne_add_public_header_directory arguments: ${RAYNE_PUBLIC_HEADER_DIRECTORY_UNPARSED_ARGUMENTS}")
    endif()

    target_include_directories("${_TARGET}" SYSTEM PUBLIC
        "$<BUILD_INTERFACE:${RAYNE_PUBLIC_HEADER_DIRECTORY_BUILD_DIRECTORY}>"
        "$<INSTALL_INTERFACE:${RAYNE_PUBLIC_HEADER_DIRECTORY_INSTALL_DIRECTORY}>")

    if(RAYNE_PUBLIC_HEADER_DIRECTORY_INSTALL_SOURCE)
        set(_INSTALL_SOURCE "${RAYNE_PUBLIC_HEADER_DIRECTORY_INSTALL_SOURCE}")
    else()
        set(_INSTALL_SOURCE "${RAYNE_PUBLIC_HEADER_DIRECTORY_BUILD_DIRECTORY}/")
    endif()

    install(DIRECTORY "${_INSTALL_SOURCE}"
        DESTINATION "${RAYNE_PUBLIC_HEADER_DIRECTORY_INSTALL_DIRECTORY}"
        ${RAYNE_PUBLIC_HEADER_DIRECTORY_INSTALL_OPTIONS})
endfunction()


function(rayne_add_runtime_dependency _TARGET _DEPENDENCY)
    cmake_parse_arguments(RAYNE_RUNTIME_DEPENDENCY "" "OUTPUT_NAME" "" ${ARGN})
    if(RAYNE_RUNTIME_DEPENDENCY_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown rayne_add_runtime_dependency arguments: ${RAYNE_RUNTIME_DEPENDENCY_UNPARSED_ARGUMENTS}")
    endif()

    __rayne_get_install_directories("${_TARGET}" RAYNE_RUNTIME_HEADER_DIRECTORY RAYNE_RUNTIME_INSTALL_DIRECTORY)

    if(TARGET "${_DEPENDENCY}")
        get_target_property(RAYNE_RUNTIME_DEPENDENCY_TYPE "${_DEPENDENCY}" TYPE)
        if(RAYNE_RUNTIME_DEPENDENCY_TYPE STREQUAL "INTERFACE_LIBRARY" OR RAYNE_RUNTIME_DEPENDENCY_TYPE STREQUAL "OBJECT_LIBRARY")
            message(FATAL_ERROR "Runtime dependency target must produce a linkable library: ${_DEPENDENCY}")
        endif()

        get_target_property(RAYNE_RUNTIME_DEPENDENCY_IMPORTED "${_DEPENDENCY}" IMPORTED)
        if(NOT RAYNE_RUNTIME_DEPENDENCY_IMPORTED)
            add_dependencies("${_TARGET}" "${_DEPENDENCY}")
        endif()
        target_link_libraries("${_TARGET}" PRIVATE "$<TARGET_LINKER_FILE:${_DEPENDENCY}>")

        if(RAYNE_RUNTIME_DEPENDENCY_TYPE STREQUAL "STATIC_LIBRARY")
            return()
        endif()

        if(RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME)
            set(RAYNE_RUNTIME_OUTPUT_FILE "$<TARGET_FILE_DIR:${_TARGET}>/${RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME}")
            set(RAYNE_RUNTIME_OUTPUT_NAME "${RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME}")
            add_custom_command(TARGET "${_TARGET}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_DEPENDENCY}>" "${RAYNE_RUNTIME_OUTPUT_FILE}")
            install(FILES "$<TARGET_FILE:${_DEPENDENCY}>"
                DESTINATION "${RAYNE_RUNTIME_INSTALL_DIRECTORY}"
                RENAME "${RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME}")
        else()
            if(RAYNE_RUNTIME_DEPENDENCY_IMPORTED)
                get_target_property(RAYNE_RUNTIME_DEPENDENCY_LOCATION "${_DEPENDENCY}" IMPORTED_LOCATION)
                if(NOT RAYNE_RUNTIME_DEPENDENCY_LOCATION)
                    message(FATAL_ERROR "Imported runtime dependency target has no IMPORTED_LOCATION: ${_DEPENDENCY}")
                endif()

                get_filename_component(RAYNE_RUNTIME_OUTPUT_NAME "${RAYNE_RUNTIME_DEPENDENCY_LOCATION}" NAME)
            else()
                set(RAYNE_RUNTIME_OUTPUT_NAME "$<TARGET_FILE_NAME:${_DEPENDENCY}>")
            endif()

            set(RAYNE_RUNTIME_OUTPUT_FILE "$<TARGET_FILE_DIR:${_TARGET}>/${RAYNE_RUNTIME_OUTPUT_NAME}")
            add_custom_command(TARGET "${_TARGET}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_DEPENDENCY}>" "${RAYNE_RUNTIME_OUTPUT_FILE}")
            install(FILES "$<TARGET_FILE:${_DEPENDENCY}>"
                DESTINATION "${RAYNE_RUNTIME_INSTALL_DIRECTORY}")
        endif()

        set_property(TARGET "${_TARGET}" APPEND PROPERTY RAYNE_RUNTIME_DEPENDENCY_FILES "${RAYNE_RUNTIME_OUTPUT_FILE}")
    elseif(IS_ABSOLUTE "${_DEPENDENCY}")
        if(RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME)
            set(RAYNE_RUNTIME_OUTPUT_NAME "${RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME}")
        else()
            get_filename_component(RAYNE_RUNTIME_OUTPUT_NAME "${_DEPENDENCY}" NAME)
        endif()

        if(IS_DIRECTORY "${_DEPENDENCY}")
            set(RAYNE_RUNTIME_OUTPUT_DIRECTORY "$<TARGET_FILE_DIR:${_TARGET}>/${RAYNE_RUNTIME_OUTPUT_NAME}")
            add_custom_command(TARGET "${_TARGET}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${_DEPENDENCY}" "${RAYNE_RUNTIME_OUTPUT_DIRECTORY}")
            install(DIRECTORY "${_DEPENDENCY}/"
                DESTINATION "${RAYNE_RUNTIME_INSTALL_DIRECTORY}/${RAYNE_RUNTIME_OUTPUT_NAME}")
            set_property(TARGET "${_TARGET}" APPEND PROPERTY RAYNE_RUNTIME_DEPENDENCY_DIRECTORIES "${RAYNE_RUNTIME_OUTPUT_DIRECTORY}")
        else()
            set(RAYNE_RUNTIME_OUTPUT_FILE "$<TARGET_FILE_DIR:${_TARGET}>/${RAYNE_RUNTIME_OUTPUT_NAME}")
            add_custom_command(TARGET "${_TARGET}" POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_DEPENDENCY}" "${RAYNE_RUNTIME_OUTPUT_FILE}")

            if(RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME)
                install(FILES "${_DEPENDENCY}"
                    DESTINATION "${RAYNE_RUNTIME_INSTALL_DIRECTORY}"
                    RENAME "${RAYNE_RUNTIME_DEPENDENCY_OUTPUT_NAME}")
            else()
                install(FILES "${_DEPENDENCY}"
                    DESTINATION "${RAYNE_RUNTIME_INSTALL_DIRECTORY}")
            endif()

            if(ANDROID)
                target_link_libraries("${_TARGET}" PRIVATE "${_DEPENDENCY}")
            endif()

            set_property(TARGET "${_TARGET}" APPEND PROPERTY RAYNE_RUNTIME_DEPENDENCY_FILES "${RAYNE_RUNTIME_OUTPUT_FILE}")
        endif()
    else()
        message(FATAL_ERROR "Runtime dependency must be a target or absolute path: ${_DEPENDENCY}")
    endif()
endfunction()


macro(rayne_set_module_output_directory _TARGET)

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


macro(rayne_set_test_output_directory _TARGET)

    if(WIN32)
        set_target_properties(${_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/Tests/${_TARGET}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Debug/Tests/${_TARGET}"
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Debug/Tests/${_TARGET}")

        set_target_properties(${_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/RelWithDebInfo/Tests/${_TARGET}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/RelWithDebInfo/Tests/${_TARGET}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/RelWithDebInfo/Tests/${_TARGET}")

        set_target_properties(${_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/Tests/${_TARGET}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Release/Tests/${_TARGET}"
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Release/Tests/${_TARGET}")
    else()
        set_target_properties(${_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tests/${_TARGET}"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Tests/${_TARGET}"
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/Tests/${_TARGET}")
    endif()

endmacro()
