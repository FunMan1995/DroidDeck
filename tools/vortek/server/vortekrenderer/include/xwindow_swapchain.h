#ifndef VORTEK_XWINDOW_SWAPCHAIN_H
#define VORTEK_XWINDOW_SWAPCHAIN_H

#include <android/hardware_buffer.h>

#include "vortek.h"

typedef struct XWindowSwapchain_Image {
    VkImage image;
    VkDeviceMemory memory;
} XWindowSwapchain_Image;

typedef struct XWindowSwapchain {
    int windowId;
    XWindowSwapchain_Image* images;
    int imageCount;
    VkFormat imageFormat;
    VkExtent2D imageExtent;
    VkImageUsageFlags imageUsage;
    VkQueue queue;
    JMethods* jmethods;
#ifdef DD_PRESENT_SHM
    /* DroidDeck presentation (b), server/droiddeck/shm_swapchain.c */
    VkDevice device;
    int nextImage;
    size_t frameSize;
    VkBuffer staging;
    VkDeviceMemory stagingMemory;
    void* stagingMap;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkFence fence;
    int shmFd;
    size_t shmSize;
    void* shm;
#endif
} XWindowSwapchain;

extern void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent);
extern int getSurfaceMinImageCount();
extern VkSurfaceFormatKHR* getSurfaceFormats(uint32_t* formatCount);

extern XWindowSwapchain* XWindowSwapchain_create(VkDevice device, uint32_t graphicsQueueIndex, VkSwapchainCreateInfoKHR* swapchainInfo, JMethods* jmethods, int windowId);
extern void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* swapchain);
extern VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* swapchain, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex);
extern void XWindowSwapchain_presentImage(XWindowSwapchain* swapchain);
#ifdef DD_PRESENT_SHM
extern VkResult dd_ShmSwapchain_present(XWindowSwapchain* swapchain, uint32_t imageIndex, const VkSemaphore* waits, uint32_t waitCount);
#endif

#endif