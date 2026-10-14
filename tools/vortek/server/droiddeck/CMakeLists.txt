# DroidDeck build of the Vortek renderer (LGPL-2.1, see tools/vortek/README.md).
# No adrenotools, no winlator lib: only the helper sources it needs. Links log/android.
set(VT_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
option(VORTEK_NO_XWINDOW "Stub Winlator's X window swapchain (brief 003 Q1 open)" ON)

set(VT_SRC
        ${VT_ROOT}/vortekrenderer/src/main.c
        ${VT_ROOT}/vortekrenderer/src/vk_object.c
        ${VT_ROOT}/vortekrenderer/src/request_handler.c
        ${VT_ROOT}/vortekrenderer/src/vulkan_helper.c
        ${VT_ROOT}/vortekrenderer/src/vk_context.c
        ${VT_ROOT}/vortekrenderer/src/resource_memory.c
        ${VT_ROOT}/vortekrenderer/src/texture_decoder.c
        ${VT_ROOT}/vortekrenderer/src/shader_inspector.c
        ${VT_ROOT}/vortekrenderer/src/async_pipeline_creator.c
        ${VT_ROOT}/vortekrenderer/src/timeline_semaphore.c
        ${VT_ROOT}/winlator/src/arrays.c
        ${VT_ROOT}/winlator/src/ring_buffer.c
        ${VT_ROOT}/winlator/src/sysvshared_memory.c
        ${CMAKE_CURRENT_LIST_DIR}/ahb_compat.c)
if(VORTEK_NO_XWINDOW)
    list(APPEND VT_SRC ${CMAKE_CURRENT_LIST_DIR}/xwindow_swapchain_stub.c)
else()
    list(APPEND VT_SRC ${VT_ROOT}/vortekrenderer/src/xwindow_swapchain.c)
endif()

add_library(vortekrenderer SHARED ${VT_SRC})
target_include_directories(vortekrenderer PRIVATE ${VT_ROOT}/vortekrenderer/include ${VT_ROOT}/winlator/include)
target_compile_options(vortekrenderer PRIVATE -O2 -Wno-unused-function -Wimplicit-function-declaration -w)
target_compile_definitions(vortekrenderer PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
target_link_libraries(vortekrenderer log android nativewindow)
