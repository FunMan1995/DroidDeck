/*
 * DroidDeck presentation option (b) for the Vortek renderer (brief 003 Q1 ruling, brief 003b §4).
 * Part of the DroidDeck build of the Vortek renderer; LGPL-2.1 like the file it replaces
 * (brunodev85/winlator-app vortekrenderer/src/xwindow_swapchain.c).
 *
 * Swapchain images are plain offscreen images. On present the renderer copies the presented
 * image into a host-visible buffer and then into a shared memory fd (passed to the client
 * once, at swapchain creation). The client puts it into the Xlib window it was given,
 * gamescope's Xwayland, with MIT-SHM (or XPutImage). Slow but simple; option (a), dma-buf,
 * is out of scope. Only reached when the opt-in Vortek forwarder is enabled (default off).
 */
#include <sys/mman.h>
#include <unistd.h>
#include "xwindow_swapchain.h"
#include "vulkan_helper.h"
#include "sysvshared_memory.h"
#include "droiddeck_shims.h"

void getWindowExtent(JMethods* jmethods, int windowId, VkExtent2D* extent) {
    /* No Java window here: the client fills the real extent from XGetGeometry. */
    (void)jmethods; (void)windowId;
    extent->width = DD_SHM_EXTENT_FROM_CLIENT;
    extent->height = DD_SHM_EXTENT_FROM_CLIENT;
}

int getSurfaceMinImageCount() { return 2; }

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

static VkResult allocBound(VkDevice device, VkMemoryRequirements* req, VkMemoryPropertyFlags want, VkDeviceMemory* memory) {
    int32_t type = -1;
    for (uint32_t i = 0; i < 32 && type < 0; i++) {
        if ((req->memoryTypeBits & (1u << i)) && (getMemoryPropertyFlags(i) & want) == want) type = (int32_t)i;
    }
    if (type < 0) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req->size, (uint32_t)type};
    return vulkanWrapper.vkAllocateMemory(device, &alloc, NULL, memory);
}

XWindowSwapchain* XWindowSwapchain_create(VkDevice device, uint32_t graphicsQueueIndex, VkSwapchainCreateInfoKHR* info, JMethods* jmethods, int windowId) {
    XWindowSwapchain* sc = calloc(1, sizeof(XWindowSwapchain));
    sc->windowId = windowId;
    sc->jmethods = jmethods;
    sc->device = device;
    sc->imageFormat = info->imageFormat;
    sc->imageExtent = info->imageExtent;
    sc->imageUsage = info->imageUsage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    sc->imageCount = info->minImageCount < 2 ? 2 : (int)info->minImageCount;
    sc->images = calloc(sc->imageCount, sizeof(XWindowSwapchain_Image));
    sc->shmFd = -1;
    uint32_t w = info->imageExtent.width, h = info->imageExtent.height;
    if (w == 0 || h == 0 || w > 16384 || h > 16384) goto error;

    for (int i = 0; i < sc->imageCount; i++) {
        VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        ii.imageType = VK_IMAGE_TYPE_2D;
        ii.format = sc->imageFormat;
        ii.extent = (VkExtent3D){w, h, 1};
        ii.mipLevels = 1;
        ii.arrayLayers = 1;
        ii.samples = VK_SAMPLE_COUNT_1_BIT;
        ii.tiling = VK_IMAGE_TILING_OPTIMAL;
        ii.usage = sc->imageUsage;
        ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vulkanWrapper.vkCreateImage(device, &ii, NULL, &sc->images[i].image) != VK_SUCCESS) goto error;
        VkMemoryRequirements req;
        vulkanWrapper.vkGetImageMemoryRequirements(device, sc->images[i].image, &req);
        if (allocBound(device, &req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &sc->images[i].memory) != VK_SUCCESS &&
            allocBound(device, &req, 0, &sc->images[i].memory) != VK_SUCCESS) goto error;
        if (vulkanWrapper.vkBindImageMemory(device, sc->images[i].image, sc->images[i].memory, 0) != VK_SUCCESS) goto error;
    }

    sc->frameSize = (size_t)w * h * 4;
    VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = sc->frameSize;
    bi.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (vulkanWrapper.vkCreateBuffer(device, &bi, NULL, &sc->staging) != VK_SUCCESS) goto error;
    VkMemoryRequirements req;
    vulkanWrapper.vkGetBufferMemoryRequirements(device, sc->staging, &req);
    if (allocBound(device, &req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &sc->stagingMemory) != VK_SUCCESS) goto error;
    if (vulkanWrapper.vkBindBufferMemory(device, sc->staging, sc->stagingMemory, 0) != VK_SUCCESS) goto error;
    if (vulkanWrapper.vkMapMemory(device, sc->stagingMemory, 0, VK_WHOLE_SIZE, 0, &sc->stagingMap) != VK_SUCCESS) goto error;

    VkCommandPoolCreateInfo pi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, graphicsQueueIndex};
    if (vulkanWrapper.vkCreateCommandPool(device, &pi, NULL, &sc->commandPool) != VK_SUCCESS) goto error;
    VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, sc->commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    if (vulkanWrapper.vkAllocateCommandBuffers(device, &ai, &sc->commandBuffer) != VK_SUCCESS) goto error;
    VkFenceCreateInfo fi = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vulkanWrapper.vkCreateFence(device, &fi, NULL, &sc->fence) != VK_SUCCESS) goto error;
    vulkanWrapper.vkGetDeviceQueue(device, graphicsQueueIndex, 0, &sc->queue);

    sc->shmSize = sizeof(DDShmFrameHeader) + sc->frameSize;
    sc->shmFd = createMemoryFd("vortek-present", (int64_t)sc->shmSize);
    if (sc->shmFd < 0) goto error;
    sc->shm = mmap(NULL, sc->shmSize, PROT_READ | PROT_WRITE, MAP_SHARED, sc->shmFd, 0);
    if (sc->shm == MAP_FAILED) { sc->shm = NULL; goto error; }
    DDShmFrameHeader* hdr = sc->shm;
    hdr->width = w;
    hdr->height = h;
    hdr->stride = w * 4;
    hdr->rgba = sc->imageFormat == VK_FORMAT_R8G8B8A8_UNORM || sc->imageFormat == VK_FORMAT_R8G8B8A8_SRGB;
    return sc;

error:
    println("vortek: SHM swapchain creation failed (%ux%u)", w, h);
    XWindowSwapchain_destroy(device, sc);
    return NULL;
}

void XWindowSwapchain_destroy(VkDevice device, XWindowSwapchain* sc) {
    if (!sc) return;
    if (sc->queue) vulkanWrapper.vkQueueWaitIdle(sc->queue);
    for (int i = 0; i < sc->imageCount; i++) {
        if (sc->images[i].image) vulkanWrapper.vkDestroyImage(device, sc->images[i].image, NULL);
        if (sc->images[i].memory) vulkanWrapper.vkFreeMemory(device, sc->images[i].memory, NULL);
    }
    if (sc->fence) vulkanWrapper.vkDestroyFence(device, sc->fence, NULL);
    if (sc->commandPool) vulkanWrapper.vkDestroyCommandPool(device, sc->commandPool, NULL);
    if (sc->staging) vulkanWrapper.vkDestroyBuffer(device, sc->staging, NULL);
    if (sc->stagingMemory) vulkanWrapper.vkFreeMemory(device, sc->stagingMemory, NULL);
    if (sc->shm) munmap(sc->shm, sc->shmSize);
    if (sc->shmFd >= 0) close(sc->shmFd);
    MEMFREE(sc->images);
    MEMFREE(sc);
}

VkResult XWindowSwapchain_acquireNextImage(XWindowSwapchain* sc, uint64_t timeout, VkSemaphore signalSemaphore, VkFence fence, uint32_t* imageIndex) {
    (void)timeout;
    if (!sc) return VK_ERROR_SURFACE_LOST_KHR;
    if (signalSemaphore || fence) {
        VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        if (signalSemaphore) { si.signalSemaphoreCount = 1; si.pSignalSemaphores = &signalSemaphore; }
        VkResult r = vulkanWrapper.vkQueueSubmit(sc->queue, 1, &si, fence);
        if (r == VK_ERROR_DEVICE_LOST) return r;
    }
    *imageIndex = sc->nextImage;
    sc->nextImage = (sc->nextImage + 1) % sc->imageCount;
    return VK_SUCCESS;
}

static void barrier(VkCommandBuffer cb, VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags srcAccess, VkAccessFlags dstAccess) {
    VkImageMemoryBarrier b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = (VkImageSubresourceRange){VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vulkanWrapper.vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, NULL, 0, NULL, 1, &b);
}

VkResult dd_ShmSwapchain_present(XWindowSwapchain* sc, uint32_t imageIndex, const VkSemaphore* waits, uint32_t waitCount) {
    if (!sc || imageIndex >= (uint32_t)sc->imageCount) return VK_ERROR_SURFACE_LOST_KHR;
    VkImage image = sc->images[imageIndex].image;
    VkCommandBufferBeginInfo begin = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    vulkanWrapper.vkResetCommandBuffer(sc->commandBuffer, 0);
    vulkanWrapper.vkBeginCommandBuffer(sc->commandBuffer, &begin);
    barrier(sc->commandBuffer, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy region = {0};
    region.imageSubresource = (VkImageSubresourceLayers){VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = (VkExtent3D){sc->imageExtent.width, sc->imageExtent.height, 1};
    vulkanWrapper.vkCmdCopyImageToBuffer(sc->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sc->staging, 1, &region);
    barrier(sc->commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_ACCESS_TRANSFER_READ_BIT, 0);
    vulkanWrapper.vkEndCommandBuffer(sc->commandBuffer);

    VkPipelineStageFlags stages[waitCount ? waitCount : 1];
    for (uint32_t i = 0; i < waitCount; i++) stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = waitCount;
    si.pWaitSemaphores = waits;
    si.pWaitDstStageMask = stages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &sc->commandBuffer;
    vulkanWrapper.vkResetFences(sc->device, 1, &sc->fence);
    VkResult r = vulkanWrapper.vkQueueSubmit(sc->queue, 1, &si, sc->fence);
    if (r != VK_SUCCESS) return r;
    r = vulkanWrapper.vkWaitForFences(sc->device, 1, &sc->fence, VK_TRUE, UINT64_MAX);
    if (r != VK_SUCCESS) return r;

    DDShmFrameHeader* hdr = sc->shm;
    memcpy((char*)sc->shm + sizeof(DDShmFrameHeader), sc->stagingMap, sc->frameSize);
    __atomic_add_fetch(&hdr->sequence, 1, __ATOMIC_RELEASE);
    return VK_SUCCESS;
}

void XWindowSwapchain_presentImage(XWindowSwapchain* sc) { dd_ShmSwapchain_present(sc, 0, NULL, 0); }
