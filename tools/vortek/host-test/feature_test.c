/*
 * Brief 003 §3 / §4.3 (e): C feature tests for the batch-1 and batch-2 shims, run through the
 * Vortek client against vortek-host-server (lavapipe + Pixel 9a profile, not komodo).
 * Validation runs on the server side (VK_LAYER_KHRONOS_validation sees the real, profile-masked
 * device); run.sh counts its errors. This program fails on any API error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include "tri.vert.inc"
#include "tri.frag.inc"

static int failures = 0;
#define CHECK(expr) do { VkResult r_ = (expr); if (r_ != VK_SUCCESS) { fprintf(stderr, "FAIL %s = %d (line %d)\n", #expr, r_, __LINE__); failures++; } } while (0)
#define EXPECT(cond, msg) do { if (!(cond)) { fprintf(stderr, "FAIL %s\n", msg); failures++; } else printf("ok   %s\n", msg); } while (0)

static int hasExt(VkPhysicalDevice pd, const char* name) {
    uint32_t n = 0;
    vkEnumerateDeviceExtensionProperties(pd, NULL, &n, NULL);
    VkExtensionProperties* p = calloc(n, sizeof(*p));
    vkEnumerateDeviceExtensionProperties(pd, NULL, &n, p);
    int found = 0;
    for (uint32_t i = 0; i < n; i++) if (!strcmp(p[i].extensionName, name)) found = 1;
    free(p);
    return found;
}

static uint32_t memType(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return 0;
}

int main(void) {
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, NULL, "vortek-feature-test", 1, NULL, 0, VK_API_VERSION_1_3};
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, NULL, 0, &app};
    VkInstance inst;
    CHECK(vkCreateInstance(&ici, NULL, &inst));
    if (failures) return 1;
    uint32_t n = 1;
    VkPhysicalDevice pd;
    vkEnumeratePhysicalDevices(inst, &n, &pd);
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(pd, &props);
    printf("device: %s\n", props.deviceName);

    EXPECT(hasExt(pd, "VK_KHR_maintenance6"), "VK_KHR_maintenance6 advertised");
    EXPECT(hasExt(pd, "VK_KHR_load_store_op_none"), "VK_KHR_load_store_op_none advertised");
    EXPECT(hasExt(pd, "VK_EXT_robustness2"), "VK_EXT_robustness2 advertised");

    VkPhysicalDeviceMaintenance6FeaturesKHR m6 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_6_FEATURES_KHR};
    VkPhysicalDeviceRobustness2FeaturesEXT r2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, &m6};
    VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &r2};
    vkGetPhysicalDeviceFeatures2(pd, &f2);
    EXPECT(f2.features.fillModeNonSolid, "fillModeNonSolid reported");
    EXPECT(r2.robustBufferAccess2, "robustBufferAccess2 reported");
    EXPECT(m6.maintenance6, "maintenance6 reported");

    /* Device with every shimmed feature enabled. */
    VkPhysicalDeviceRobustness2FeaturesEXT r2on = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT, NULL, VK_TRUE, VK_FALSE, r2.nullDescriptor};
    VkPhysicalDeviceFeatures2 f2on = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &r2on};
    f2on.features.fillModeNonSolid = VK_TRUE;
    f2on.features.robustBufferAccess = VK_TRUE;
    const char* exts[] = {"VK_KHR_maintenance6", "VK_KHR_load_store_op_none", "VK_EXT_robustness2"};
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, NULL, 0, 0, 1, &prio};
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2on, 0, 1, &qci, 0, NULL, 3, exts, NULL};
    VkDevice dev;
    CHECK(vkCreateDevice(pd, &dci, NULL, &dev));
    if (failures) return 1;
    printf("ok   vkCreateDevice with maintenance6 + KHR_load_store_op_none + robustBufferAccess2 + fillModeNonSolid\n");
    VkQueue q;
    vkGetDeviceQueue(dev, 0, 0, &q);

    PFN_vkCmdBindDescriptorSets2KHR bind2 = (PFN_vkCmdBindDescriptorSets2KHR)vkGetDeviceProcAddr(dev, "vkCmdBindDescriptorSets2KHR");
    PFN_vkCmdPushConstants2KHR push2 = (PFN_vkCmdPushConstants2KHR)vkGetDeviceProcAddr(dev, "vkCmdPushConstants2KHR");
    EXPECT(bind2 && push2, "maintenance6 commands resolve");

    /* Color target, 64x64, LOAD_OP_NONE_KHR. */
    VkImageCreateInfo ii = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, 0, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, {64, 64, 1}, 1, 1,
                            VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED};
    VkImage img; CHECK(vkCreateImage(dev, &ii, NULL, &img));
    VkMemoryRequirements req; vkGetImageMemoryRequirements(dev, img, &req);
    VkMemoryAllocateInfo mai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, memType(pd, req.memoryTypeBits, 0)};
    VkDeviceMemory imem; CHECK(vkAllocateMemory(dev, &mai, NULL, &imem)); CHECK(vkBindImageMemory(dev, img, imem, 0));
    VkImageViewCreateInfo vci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0, img, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, {0}, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkImageView view; CHECK(vkCreateImageView(dev, &vci, NULL, &view));
    VkAttachmentDescription att = {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_NONE_KHR, VK_ATTACHMENT_STORE_OP_STORE,
                                   VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &ref};
    VkRenderPassCreateInfo rpi = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, NULL, 0, 1, &att, 1, &sub};
    VkRenderPass rp; CHECK(vkCreateRenderPass(dev, &rpi, NULL, &rp));
    printf("ok   render pass with VK_ATTACHMENT_LOAD_OP_NONE_KHR\n");
    VkFramebufferCreateInfo fbi = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, NULL, 0, rp, 1, &view, 64, 64, 1};
    VkFramebuffer fb; CHECK(vkCreateFramebuffer(dev, &fbi, NULL, &fb));

    /* Uniform buffer + descriptor set used by both bind points. */
    VkBufferCreateInfo bci = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, 256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT};
    VkBuffer ubo; CHECK(vkCreateBuffer(dev, &bci, NULL, &ubo));
    vkGetBufferMemoryRequirements(dev, ubo, &req);
    mai.allocationSize = req.size; mai.memoryTypeIndex = memType(pd, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    VkDeviceMemory umem; CHECK(vkAllocateMemory(dev, &mai, NULL, &umem)); CHECK(vkBindBufferMemory(dev, ubo, umem, 0));
    float* up; CHECK(vkMapMemory(dev, umem, 0, 256, 0, (void**)&up)); up[0] = up[1] = 1.0f; vkUnmapMemory(dev, umem);
    VkDescriptorSetLayoutBinding b0 = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT};
    VkDescriptorSetLayoutCreateInfo dsl = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, NULL, 0, 1, &b0};
    VkDescriptorSetLayout setLayout; CHECK(vkCreateDescriptorSetLayout(dev, &dsl, NULL, &setLayout));
    VkPushConstantRange pcr = {VK_SHADER_STAGE_VERTEX_BIT, 0, 16};
    VkPipelineLayoutCreateInfo pli = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, NULL, 0, 1, &setLayout, 1, &pcr};
    VkPipelineLayout layout; CHECK(vkCreatePipelineLayout(dev, &pli, NULL, &layout));
    VkDescriptorPoolSize ps = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1};
    VkDescriptorPoolCreateInfo dpi = {VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, NULL, 0, 1, 1, &ps};
    VkDescriptorPool pool; CHECK(vkCreateDescriptorPool(dev, &dpi, NULL, &pool));
    VkDescriptorSetAllocateInfo dai = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, NULL, pool, 1, &setLayout};
    VkDescriptorSet set; CHECK(vkAllocateDescriptorSets(dev, &dai, &set));
    VkDescriptorBufferInfo dbi = {ubo, 0, 16};
    VkWriteDescriptorSet w = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, NULL, set, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL, &dbi};
    vkUpdateDescriptorSets(dev, 1, &w, 0, NULL);

    /* LINE polygon mode pipeline (fillModeNonSolid shim). */
    VkShaderModuleCreateInfo smv = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, sizeof(vert_spv), vert_spv};
    VkShaderModuleCreateInfo smf = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, NULL, 0, sizeof(frag_spv), frag_spv};
    VkShaderModule vs, fs; CHECK(vkCreateShaderModule(dev, &smv, NULL, &vs)); CHECK(vkCreateShaderModule(dev, &smf, NULL, &fs));
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main"},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, NULL, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main"}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, NULL, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
    VkViewport vp = {0, 0, 64, 64, 0, 1}; VkRect2D sc = {{0, 0}, {64, 64}};
    VkPipelineViewportStateCreateInfo vps = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, NULL, 0, 1, &vp, 1, &sc};
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, NULL, 0, 0, 0, VK_POLYGON_MODE_LINE, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE, 0, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, NULL, 0, VK_SAMPLE_COUNT_1_BIT};
    VkPipelineColorBlendAttachmentState cba = {0}; cba.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, NULL, 0, 0, 0, 1, &cba};
    VkGraphicsPipelineCreateInfo gpi = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, NULL, 0, 2, st, &vi, &ia, NULL, &vps, &rs, &ms, NULL, &cb, NULL, layout, rp, 0};
    VkPipeline pipe; CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpi, NULL, &pipe));
    printf("ok   VK_POLYGON_MODE_LINE pipeline created\n");

    VkCommandPoolCreateInfo cpi = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, NULL, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, 0};
    VkCommandPool cpool; CHECK(vkCreateCommandPool(dev, &cpi, NULL, &cpool));
    VkCommandBufferAllocateInfo cai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, NULL, cpool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    VkCommandBuffer cmd; CHECK(vkAllocateCommandBuffers(dev, &cai, &cmd));
    VkCommandBufferBeginInfo cbi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    CHECK(vkBeginCommandBuffer(cmd, &cbi));
    VkImageMemoryBarrier toColor = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, img, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, NULL, 0, NULL, 1, &toColor);
    VkRenderPassBeginInfo rbi = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, NULL, rp, fb, {{0, 0}, {64, 64}}};
    vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
    if (bind2 && push2) {
        VkBindDescriptorSetsInfoKHR bdi = {VK_STRUCTURE_TYPE_BIND_DESCRIPTOR_SETS_INFO_KHR, NULL, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT, layout, 0, 1, &set, 0, NULL};
        bind2(cmd, &bdi); /* graphics + compute: two core binds on the server */
        float off[4] = {0.1f, 0.0f, 0.0f, 0.0f};
        VkPushConstantsInfoKHR pci = {VK_STRUCTURE_TYPE_PUSH_CONSTANTS_INFO_KHR, NULL, layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 16, off};
        push2(cmd, &pci);
    }
    vkCmdBindIndexBuffer(cmd, VK_NULL_HANDLE, 0, VK_INDEX_TYPE_UINT32); /* maintenance6: NULL index buffer reads zeros */
    vkCmdDrawIndexed(cmd, 3, 1, 0, 0, 0);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
    CHECK(vkEndCommandBuffer(cmd));
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO, NULL, 0, NULL, NULL, 1, &cmd};
    VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence; CHECK(vkCreateFence(dev, &fci, NULL, &fence));
    CHECK(vkQueueSubmit(q, 1, &si, fence));
    CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10ull * 1000 * 1000 * 1000));
    printf("ok   draw with bindDescriptorSets2 (GRAPHICS|COMPUTE), pushConstants2, NULL index buffer\n");

    /* Data path: client-mapped writes reach the GPU, GPU results reach the client mapping, and the
     * draw above actually produced pixels (vkcube showed only the clear color through Vortek). */
    {
        const uint32_t n = 64 * 64;
        VkBuffer src, dst; VkDeviceMemory smem, dmem;
        VkBufferCreateInfo xb = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, NULL, 0, n * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        CHECK(vkCreateBuffer(dev, &xb, NULL, &src)); CHECK(vkCreateBuffer(dev, &xb, NULL, &dst));
        vkGetBufferMemoryRequirements(dev, src, &req);
        VkMemoryAllocateInfo xm = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, req.size, memType(pd, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
        CHECK(vkAllocateMemory(dev, &xm, NULL, &smem)); CHECK(vkAllocateMemory(dev, &xm, NULL, &dmem));
        CHECK(vkBindBufferMemory(dev, src, smem, 0)); CHECK(vkBindBufferMemory(dev, dst, dmem, 0));
        uint32_t* sp; CHECK(vkMapMemory(dev, smem, 0, VK_WHOLE_SIZE, 0, (void**)&sp));
        for (uint32_t i = 0; i < n; i++) sp[i] = 0xA5000000u | i;
        uint32_t* sp2 = NULL; CHECK(vkMapMemory(dev, smem, 0, VK_WHOLE_SIZE, 0, (void**)&sp2)); /* repeat map */
        EXPECT(sp2 == sp, "repeat vkMapMemory returns the same pointer");
        uint32_t* dp; CHECK(vkMapMemory(dev, dmem, 0, VK_WHOLE_SIZE, 0, (void**)&dp)); memset(dp, 0, n * 4);
        CHECK(vkResetCommandBuffer(cmd, 0)); CHECK(vkBeginCommandBuffer(cmd, &cbi));
        VkBufferCopy bc = {0, 0, n * 4}; vkCmdCopyBuffer(cmd, src, dst, 1, &bc);
        CHECK(vkEndCommandBuffer(cmd)); CHECK(vkResetFences(dev, 1, &fence));
        CHECK(vkQueueSubmit(q, 1, &si, fence)); CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10ull * 1000 * 1000 * 1000));
        uint32_t bad = 0; for (uint32_t i = 0; i < n; i++) bad += dp[i] != (0xA5000000u | i);
        EXPECT(bad == 0, "client-written buffer copied by the GPU reads back in the client");
        if (bad) fprintf(stderr, "     %u/%u words differ, dp[0]=%08x dp[1]=%08x\n", bad, n, dp[0], dp[1]);
        memset(dp, 0, n * 4);
        CHECK(vkResetCommandBuffer(cmd, 0)); CHECK(vkBeginCommandBuffer(cmd, &cbi));
        VkImageMemoryBarrier toSrc = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, NULL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, img, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &toSrc);
        VkBufferImageCopy ic = {0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {64, 64, 1}};
        vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst, 1, &ic);
        CHECK(vkEndCommandBuffer(cmd)); CHECK(vkResetFences(dev, 1, &fence));
        CHECK(vkQueueSubmit(q, 1, &si, fence)); CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10ull * 1000 * 1000 * 1000));
        uint32_t lit = 0; for (uint32_t i = 0; i < n; i++) lit += dp[i] != 0;
        EXPECT(lit > 0, "the draw produced pixels (image -> buffer readback)");
        fprintf(stderr, "     %u/%u pixels written by the draw\n", lit, n);
        vkUnmapMemory(dev, smem); vkUnmapMemory(dev, dmem);
        vkDestroyBuffer(dev, src, NULL); vkDestroyBuffer(dev, dst, NULL); vkFreeMemory(dev, smem, NULL); vkFreeMemory(dev, dmem, NULL);
    }

    vkDestroyFence(dev, fence, NULL);
    vkDestroyCommandPool(dev, cpool, NULL);
    vkDestroyPipeline(dev, pipe, NULL);
    vkDestroyShaderModule(dev, vs, NULL); vkDestroyShaderModule(dev, fs, NULL);
    vkDestroyDescriptorPool(dev, pool, NULL);
    vkDestroyPipelineLayout(dev, layout, NULL);
    vkDestroyDescriptorSetLayout(dev, setLayout, NULL);
    vkDestroyBuffer(dev, ubo, NULL); vkFreeMemory(dev, umem, NULL);
    vkDestroyFramebuffer(dev, fb, NULL);
    vkDestroyRenderPass(dev, rp, NULL);
    vkDestroyImageView(dev, view, NULL);
    vkDestroyImage(dev, img, NULL); vkFreeMemory(dev, imem, NULL);
    vkDestroyDevice(dev, NULL);
    vkDestroyInstance(inst, NULL);
    printf(failures ? "feature_test: FAIL (%d)\n" : "feature_test: OK\n", failures);
    return failures ? 1 : 0;
}
