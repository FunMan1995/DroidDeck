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

#endif
