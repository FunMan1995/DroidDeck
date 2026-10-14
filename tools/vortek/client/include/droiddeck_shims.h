/*
 * DroidDeck batch-1 client shims for Vortek (brief 003 §3.2, VK_KHR_maintenance6).
 * Part of the DroidDeck build of the Vortek client, LGPL-2.1 (tools/vortek/LICENSE).
 * The renderer advertises VK_KHR_maintenance6 and strips it at vkCreateDevice; the client
 * reports the feature and implements the commands on top of the existing core calls.
 */
#ifndef DROIDDECK_CLIENT_SHIMS_H
#define DROIDDECK_CLIENT_SHIMS_H

#include "vortek.h"

extern void dd_fillMaintenance6Features(void* pNext);
extern void dd_fillMaintenance6Properties(void* pNext);
extern void dd_writeBindMemoryStatus(const void* pNext, VkResult result);

/* Bind points implied by stage flags: graphics if any graphics stage, compute if COMPUTE. */
extern uint32_t dd_bindPointsForStages(VkShaderStageFlags stageFlags, VkPipelineBindPoint out[2]);

extern void vt_call_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets);
extern void vt_call_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues);
extern void vt_call_vkCmdBindDescriptorSets2KHR(VkCommandBuffer commandBuffer, const VkBindDescriptorSetsInfoKHR* pInfo);
extern void vt_call_vkCmdPushConstants2KHR(VkCommandBuffer commandBuffer, const VkPushConstantsInfoKHR* pInfo);

#endif
