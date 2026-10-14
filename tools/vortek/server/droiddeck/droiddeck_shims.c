/*
 * DroidDeck batch-1 feature shims for the Vortek renderer (brief 003 §3).
 * Part of the DroidDeck build of the Vortek renderer, LGPL-2.1 (tools/vortek/LICENSE).
 */
#include <stdatomic.h>
#include "droiddeck_shims.h"
#include "vk_context.h"
#include "vulkan_helper.h"

static atomic_bool loggedPolygonMode = false;

VkPolygonMode dd_clampPolygonMode(VkPolygonMode mode) {
    if (mode == VK_POLYGON_MODE_FILL) return mode;
    if (!atomic_exchange(&loggedPolygonMode, true)) {
        println("vortek: fillModeNonSolid not supported by the driver; polygon mode %d drawn as FILL", (int)mode);
    }
    return VK_POLYGON_MODE_FILL;
}

void dd_clampGraphicsPipelinePolygonModes(VkGraphicsPipelineCreateInfo* createInfos, uint32_t count) {
    if (!createInfos) return;
    for (uint32_t i = 0; i < count; i++) {
        VkPipelineRasterizationStateCreateInfo* rs = (VkPipelineRasterizationStateCreateInfo*)createInfos[i].pRasterizationState;
        if (rs) rs->polygonMode = dd_clampPolygonMode(rs->polygonMode);
    }
}

const char* const ddImplementedDeviceExtensions[] = {"VK_KHR_maintenance6"};
const uint32_t ddImplementedDeviceExtensionCount = sizeof(ddImplementedDeviceExtensions) / sizeof(ddImplementedDeviceExtensions[0]);

bool dd_deviceHasExtension(VkPhysicalDevice physicalDevice, const char* name) {
    uint32_t count = 0;
    if (vulkanWrapper.vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &count, NULL) != VK_SUCCESS || count == 0) return false;
    VkExtensionProperties* props = calloc(count, sizeof(VkExtensionProperties));
    bool found = false;
    if (vulkanWrapper.vkEnumerateDeviceExtensionProperties(physicalDevice, NULL, &count, props) == VK_SUCCESS) {
        for (uint32_t i = 0; i < count && !found; i++) found = strcmp(props[i].extensionName, name) == 0;
    }
    free(props);
    return found;
}

void dd_stripExtensions(VkContext* context, char*** names, uint32_t* count, const char* const* skip, uint32_t skipCount) {
    if (!*names || *count == 0) return;
    char** out = vt_alloc(&context->memoryPool, *count * sizeof(char*));
    uint32_t n = 0;
    for (uint32_t i = 0; i < *count; i++) {
        bool drop = false;
        for (uint32_t j = 0; j < skipCount && !drop; j++) drop = strcmp((*names)[i], skip[j]) == 0;
        if (!drop) out[n++] = (*names)[i];
    }
    *names = out;
    *count = n;
}

void dd_createDummyIndexBuffer(VkContext* context, VkDevice device) {
    VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = DD_DUMMY_INDEX_BUFFER_SIZE;
    info.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vulkanWrapper.vkCreateBuffer(device, &info, NULL, &context->ddDummyIndexBuffer) != VK_SUCCESS) {
        context->ddDummyIndexBuffer = VK_NULL_HANDLE;
        return;
    }

    VkMemoryRequirements req;
    vulkanWrapper.vkGetBufferMemoryRequirements(device, context->ddDummyIndexBuffer, &req);
    const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    int32_t typeIndex = -1;
    for (uint32_t i = 0; i < 32; i++) {
        if ((req.memoryTypeBits & (1u << i)) && (getMemoryPropertyFlags(i) & want) == want) { typeIndex = (int32_t)i; break; }
    }

    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = (uint32_t)typeIndex;
    void* data = NULL;
    if (typeIndex < 0 ||
        vulkanWrapper.vkAllocateMemory(device, &alloc, NULL, &context->ddDummyIndexMemory) != VK_SUCCESS ||
        vulkanWrapper.vkBindBufferMemory(device, context->ddDummyIndexBuffer, context->ddDummyIndexMemory, 0) != VK_SUCCESS ||
        vulkanWrapper.vkMapMemory(device, context->ddDummyIndexMemory, 0, VK_WHOLE_SIZE, 0, &data) != VK_SUCCESS) {
        println("vortek: maintenance6 dummy index buffer unavailable");
        dd_destroyDummyIndexBuffer(context, device);
        return;
    }
    memset(data, 0, req.size);
    vulkanWrapper.vkUnmapMemory(device, context->ddDummyIndexMemory);
}

void dd_destroyDummyIndexBuffer(VkContext* context, VkDevice device) {
    if (context->ddDummyIndexBuffer) vulkanWrapper.vkDestroyBuffer(device, context->ddDummyIndexBuffer, NULL);
    if (context->ddDummyIndexMemory) vulkanWrapper.vkFreeMemory(device, context->ddDummyIndexMemory, NULL);
    context->ddDummyIndexBuffer = VK_NULL_HANDLE;
    context->ddDummyIndexMemory = VK_NULL_HANDLE;
}
