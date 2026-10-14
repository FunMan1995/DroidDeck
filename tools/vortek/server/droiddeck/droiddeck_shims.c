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

/* 3.3 VK_KHR_load_store_op_none: same enum values as the EXT (VK_ATTACHMENT_LOAD_OP_NONE_KHR ==
 * VK_ATTACHMENT_LOAD_OP_NONE_EXT, STORE_OP_NONE is core 1.3), so only the name is translated. */
#define DD_LSON_KHR "VK_KHR_load_store_op_none"
#define DD_LSON_EXT "VK_EXT_load_store_op_none"

static bool dd_listHas(const VkExtensionProperties* props, uint32_t count, const char* name) {
    for (uint32_t i = 0; i < count; i++) if (strcmp(props[i].extensionName, name) == 0) return true;
    return false;
}

void dd_aliasLoadStoreOpNone(struct VkContext* context, VkExtensionProperties** props, uint32_t* count) {
    if (!*props || !dd_listHas(*props, *count, DD_LSON_EXT) || dd_listHas(*props, *count, DD_LSON_KHR)) return;
    static const char* const alias[] = {DD_LSON_KHR};
    injectExtensions2(context, props, count, alias, 1, NULL, 0);
}

void dd_rewriteLoadStoreOpNone(struct VkContext* context, VkPhysicalDevice physicalDevice, char*** names, uint32_t* count) {
    if (!*names || *count == 0) return;
    int khr = -1;
    bool ext = false;
    for (uint32_t i = 0; i < *count; i++) {
        if (strcmp((*names)[i], DD_LSON_KHR) == 0) khr = (int)i;
        else if (strcmp((*names)[i], DD_LSON_EXT) == 0) ext = true;
    }
    if (khr < 0 || (physicalDevice && dd_deviceHasExtension(physicalDevice, DD_LSON_KHR))) return;
    if (ext) {
        static const char* const skip[] = {DD_LSON_KHR};
        dd_stripExtensions(context, names, count, skip, 1);
        return;
    }
    char** out = vt_alloc(&context->memoryPool, *count * sizeof(char*));
    memcpy(out, *names, *count * sizeof(char*));
    out[khr] = (char*)DD_LSON_EXT;
    *names = out;
}

/* Batch 2, robustBufferAccess2 relaxed (brief 003 Q3 ruling): advertise it, and when the driver
 * lacks it enable v1 robustBufferAccess instead. No SPIR-V bounds-check injection, so out-of-bounds
 * reads can return driver-defined values instead of zero: a known stability risk. */
void dd_advertiseRobustBufferAccess2(void* pNext) {
    VkPhysicalDeviceRobustness2FeaturesEXT* r2 = findNextVkStructure(pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
    if (r2) r2->robustBufferAccess2 = VK_TRUE;
}

bool dd_relaxRobustBufferAccess2(VkPhysicalDevice physicalDevice, VkDeviceCreateInfo* createInfo) {
    VkPhysicalDeviceRobustness2FeaturesEXT* r2 = findNextVkStructure((void*)createInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
    if (!r2 || !r2->robustBufferAccess2) return false;

    VkPhysicalDeviceRobustness2FeaturesEXT real = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &real};
    if (physicalDevice) vulkanWrapper.vkGetPhysicalDeviceFeatures2(physicalDevice, &features);
    if (real.robustBufferAccess2) return false;

    r2->robustBufferAccess2 = VK_FALSE;
    VkPhysicalDeviceFeatures* enabled = (VkPhysicalDeviceFeatures*)createInfo->pEnabledFeatures;
    if (!enabled) {
        VkPhysicalDeviceFeatures2* f2 = findNextVkStructure((void*)createInfo->pNext, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
        if (f2) enabled = &f2->features;
    }
    if (enabled && features.features.robustBufferAccess) enabled->robustBufferAccess = VK_TRUE;
    println("vortek: robustBufferAccess2 not supported by the driver; using robustBufferAccess (v1) for this device");
    return true;
}
