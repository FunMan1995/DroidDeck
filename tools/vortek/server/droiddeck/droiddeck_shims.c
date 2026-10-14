/*
 * DroidDeck batch-1 feature shims for the Vortek renderer (brief 003 §3).
 * Part of the DroidDeck build of the Vortek renderer, LGPL-2.1 (tools/vortek/LICENSE).
 */
#include <stdatomic.h>
#include "droiddeck_shims.h"

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
