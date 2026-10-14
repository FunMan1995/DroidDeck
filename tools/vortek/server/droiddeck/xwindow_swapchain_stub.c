/*
 * DroidDeck stub for Vortek's X window swapchain (VORTEK_NO_XWINDOW).
 * Part of the DroidDeck build of the Vortek renderer; LGPL-2.1 like the file it
 * replaces (brunodev85/winlator-app vortekrenderer/src/xwindow_swapchain.c).
 * Presentation design is open (brief 003 Q1); batch-1 runs are headless, so
 * swapchain creation fails cleanly instead of reaching Winlator's X server.
 */
#include "xwindow_swapchain.h"

void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent) {
    (void)jmethods; (void)windowId;
    extent->width = 0;
    extent->height = 0;
}

int getSurfaceMinImageCount() { return 1; }

/* Caller frees the result (request_handler.c MEMFREE). */
VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount) {
    static const VkFormat supported[] = {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB};
    uint32_t n = sizeof(supported) / sizeof(supported[0]);
    VkSurfaceFormatKHR* formats = calloc(n, sizeof(VkSurfaceFormatKHR));
    if (formatCount) *formatCount = n;
    for (uint32_t i = 0; i < n; i++) {
        formats[i].format = supported[i];
        formats[i].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    return formats;
}

XWindowSwapchain* XWindowSwapchain_create(VkDevice device, uint32_t graphicsQueueIndex, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId) {
    (void)device; (void)graphicsQueueIndex; (void)swapchainInfo; (void)jmethods; (void)windowId;
    println("vortek: presentation is not wired in DroidDeck yet (VORTEK_NO_XWINDOW)");
    return NULL;
}

void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain) { (void)device; (void)swapchain; }

VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex) {
    (void)swapchain; (void)timeout; (void)signalSemaphore; (void)fence; (void)imageIndex;
    return VK_ERROR_SURFACE_LOST_KHR;
}

void XWindowSwapchain_presentImage(XWindowSwapchain* swapchain) { (void)swapchain; }
