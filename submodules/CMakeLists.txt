# CrossWindow
add_subdirectory(crosswindow)

# Flecs
add_subdirectory(flecs)

# Diligent
add_subdirectory(DiligentCore)
add_subdirectory(DiligentTools)

function(configure_diligent APP_NAME)
    target_link_libraries(${APP_NAME} PRIVATE
        Diligent-BuildSettings
        Diligent-Common
        Diligent-TargetPlatform
        Diligent-Primitives)

    if(D3D11_SUPPORTED)
        target_link_libraries(${APP_NAME} PRIVATE Diligent-GraphicsEngineD3D11-shared)
    endif()
    if(D3D12_SUPPORTED)
        target_link_libraries(${APP_NAME} PRIVATE Diligent-GraphicsEngineD3D12-shared)
    endif()
    if(GL_SUPPORTED)
        target_link_libraries(${APP_NAME} PRIVATE Diligent-GraphicsEngineOpenGL-shared)
    endif()
    if(VULKAN_SUPPORTED)
        target_link_libraries(${APP_NAME} PRIVATE Diligent-GraphicsEngineVk-shared)
    endif()
    if(METAL_SUPPORTED)
        target_link_libraries(${APP_NAME} PRIVATE Diligent-GraphicsEngineMetal-shared)
    endif()

    if(PLATFORM_WIN32)
        copy_required_dlls(${APP_NAME})
    elseif(PLATFORM_UNIVERSAL_WINDOWS)
        package_required_dlls(${APP_NAME})
    endif()

    if(PLATFORM_MACOS AND VULKAN_LIB_PATH)
        set_target_properties(${APP_NAME} PROPERTIES
            BUILD_RPATH "${VULKAN_LIB_PATH}"
        )
    endif()

    if(PLATFORM_EMSCRIPTEN)
        target_link_options(${APP_NAME} PRIVATE "SHELL: -s SINGLE_FILE -s ALLOW_MEMORY_GROWTH=1")
    endif()
endfunction()
