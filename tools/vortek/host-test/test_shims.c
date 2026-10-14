/* Host unit test for tools/vortek/server/droiddeck/droiddeck_shims.c (no Vulkan driver needed).
 * Run: tools/vortek/host-test/run.sh */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "droiddeck_shims.h"

static void test_polygon_mode(void) {
    assert(dd_clampPolygonMode(VK_POLYGON_MODE_FILL) == VK_POLYGON_MODE_FILL);
    assert(dd_clampPolygonMode(VK_POLYGON_MODE_LINE) == VK_POLYGON_MODE_FILL);
    assert(dd_clampPolygonMode(VK_POLYGON_MODE_POINT) == VK_POLYGON_MODE_FILL);

    VkPipelineRasterizationStateCreateInfo line = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_LINE};
    VkPipelineRasterizationStateCreateInfo fill = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL};
    VkGraphicsPipelineCreateInfo infos[3] = {{.pRasterizationState = &line}, {.pRasterizationState = NULL}, {.pRasterizationState = &fill}};
    dd_clampGraphicsPipelinePolygonModes(infos, 3);
    assert(line.polygonMode == VK_POLYGON_MODE_FILL);
    assert(fill.polygonMode == VK_POLYGON_MODE_FILL);
    dd_clampGraphicsPipelinePolygonModes(NULL, 0);
}

int main(void) {
    test_polygon_mode();
    printf("test_shims: OK\n");
    return 0;
}
