/*
 * DroidDeck batch-1 feature shims for the Vortek renderer (brief 003 §3).
 * Part of the DroidDeck build of the Vortek renderer, LGPL-2.1 (tools/vortek/LICENSE).
 */
#ifndef DROIDDECK_SHIMS_H
#define DROIDDECK_SHIMS_H

#include "vortek.h"

/* 3.1 fillModeNonSolid: clamp non-FILL polygon modes when the real driver lacks the feature. */
extern void dd_clampGraphicsPipelinePolygonModes(VkGraphicsPipelineCreateInfo* createInfos, uint32_t count);
extern VkPolygonMode dd_clampPolygonMode(VkPolygonMode mode);

/* 3.2 maintenance6: advertised by the renderer, implemented by the client (vulkan_calls.c)
 * plus these server pieces. Stripped from vkCreateDevice when the driver lacks it. */
struct VkContext;
extern const char* const ddImplementedDeviceExtensions[];
extern const uint32_t ddImplementedDeviceExtensionCount;
extern bool dd_deviceHasExtension(VkPhysicalDevice physicalDevice, const char* name);
/* Removes names in `skip` from an enabled-extension list (new array from the context memory pool). */
extern void dd_stripExtensions(struct VkContext* context, char*** names, uint32_t* count, const char* const* skip, uint32_t skipCount);
/* maintenance6 allows a NULL index buffer (reads as zero); bound to a zeroed dummy instead. */
extern void dd_createDummyIndexBuffer(struct VkContext* context, VkDevice device);
extern void dd_destroyDummyIndexBuffer(struct VkContext* context, VkDevice device);
#define DD_DUMMY_INDEX_BUFFER_SIZE 65536

/* 3.3 Advertise VK_KHR_load_store_op_none when only the EXT exists; enable it as the EXT. */
extern void dd_aliasLoadStoreOpNone(struct VkContext* context, VkExtensionProperties** props, uint32_t* count);
extern void dd_rewriteLoadStoreOpNone(struct VkContext* context, VkPhysicalDevice physicalDevice, char*** names, uint32_t* count);

#endif
