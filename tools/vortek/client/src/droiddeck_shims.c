/*
 * DroidDeck batch-1 client shims for Vortek (brief 003 §3.2, VK_KHR_maintenance6).
 * Part of the DroidDeck build of the Vortek client, LGPL-2.1 (tools/vortek/LICENSE).
 */
#include "droiddeck_shims.h"

#define DD_GRAPHICS_STAGES (VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_MESH_BIT_EXT)

void dd_fillMaintenance6Features(void* pNext) {
    for (VkBaseOutStructure* s = pNext; s; s = s->pNext) {
        if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR) {
            ((VkPhysicalDeviceMaintenance6FeaturesKHR*)s)->maintenance6 = VK_TRUE;
        }
    }
}

void dd_fillMaintenance6Properties(void* pNext) {
    for (VkBaseOutStructure* s = pNext; s; s = s->pNext) {
        if (s->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_PROPERTIES_KHR) {
            VkPhysicalDeviceMaintenance6PropertiesKHR* p = (VkPhysicalDeviceMaintenance6PropertiesKHR*)s;
            p->blockTexelViewCompatibleMultipleLayers = VK_FALSE;
            /* Largest descriptor count any Y'CbCr format needs (3 planes). */
            p->maxCombinedImageSamplerDescriptorCount = 3;
            p->fragmentShadingRateClampCombinerInputs = VK_FALSE;
        }
    }
}

void dd_writeBindMemoryStatus(const void* pNext, VkResult result) {
    for (const VkBaseInStructure* s = pNext; s; s = s->pNext) {
        if (s->sType == VK_STRUCTURE_TYPE_BIND_MEMORY_STATUS_KHR) {
            const VkBindMemoryStatusKHR* status = (const VkBindMemoryStatusKHR*)s;
            if (status->pResult) *status->pResult = result;
        }
    }
}

uint32_t dd_bindPointsForStages(VkShaderStageFlags stageFlags, VkPipelineBindPoint out[2]) {
    uint32_t n = 0;
    if (stageFlags & DD_GRAPHICS_STAGES) out[n++] = VK_PIPELINE_BIND_POINT_GRAPHICS;
    if (stageFlags & VK_SHADER_STAGE_COMPUTE_BIT) out[n++] = VK_PIPELINE_BIND_POINT_COMPUTE;
    return n;
}

void vt_call_vkCmdBindDescriptorSets2KHR(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfoKHR* pInfo) {
    VkPipelineBindPoint points[2];
    uint32_t n = dd_bindPointsForStages(pInfo->stageFlags, points);
    for (uint32_t i = 0; i < n; i++) {
        vt_call_vkCmdBindDescriptorSets(commandBuffer, points[i], pInfo->layout, pInfo->firstSet, pInfo->descriptorSetCount,
                                        pInfo->pDescriptorSets, pInfo->dynamicOffsetCount, pInfo->pDynamicOffsets);
    }
}

void vt_call_vkCmdPushConstants2KHR(VkCommandBuffer commandBuffer, const VkPushConstantsInfoKHR* pInfo) {
    vt_call_vkCmdPushConstants(commandBuffer, pInfo->layout, pInfo->stageFlags, pInfo->offset, pInfo->size, pInfo->pValues);
}
