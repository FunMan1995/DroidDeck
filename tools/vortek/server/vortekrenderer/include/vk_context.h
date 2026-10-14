#ifndef VORTEK_CONTEXT_H
#define VORTEK_CONTEXT_H

#include "vortek.h"
#include "xwindow_swapchain.h"
#include "texture_decoder.h"
#include "async_pipeline_creator.h"
#include "timeline_semaphore.h"

typedef struct VkContext {
    int clientFd;
    int vkMaxVersion;
    short maxDeviceMemory;
    short imageCacheSize;
    ResourceMemoryType resourceMemoryType;
    ArrayList* exposedDeviceExtensions;
    ArrayList* disabledDeviceExtensions;
    uint64_t totalAllocationSize;

    bool hasExternalMemoryFd;
    bool hasExternalMemoryDMABuf;
    /* DroidDeck: instances/devices the client still owns, destroyed when it disconnects without
     * cleaning up (each leaked instance kept a driver memfd and memory alive; brief 003 run (f)). */
#define DD_MAX_LIVE_HANDLES 16
    VkInstance ddLiveInstances[DD_MAX_LIVE_HANDLES];
    VkDevice ddLiveDevices[DD_MAX_LIVE_HANDLES];
    bool opaqueFdDedicatedOnly; /* DroidDeck: driver requires dedicated allocations for exported opaque-fd memory */

#if ENABLE_VALIDATION_LAYER
    VkDebugReportCallbackEXT debugReportCallback;
#endif

    char* inputBuffer;
    int inputBufferSize;
    MemoryPool memoryPool;

    pthread_t requestHandlerThread;
    RingBuffer* clientRing;
    RingBuffer* serverRing;
    VkResult status;

    int graphicsQueueIndex;

    TextureDecoder* textureDecoder;
    ShaderInspector* shaderInspector;
    ThreadPool* threadPool;

    ArrayList extraDataRequests;
    pthread_mutex_t extraDataRequestsMutex;

    JMethods jmethods;
    char* engineName;
    uint32_t engineVersion;
    VkDriverId driverID;

    /* DroidDeck batch-1 shims (tools/vortek/server/droiddeck/droiddeck_shims.c) */
    bool emulateFillModeNonSolid;
    VkBuffer ddDummyIndexBuffer;
    VkDeviceMemory ddDummyIndexMemory;
} VkContext;

typedef struct ExtraDataRequest {
    uint16_t requestId;
    void* data;
    int size;
} ExtraDataRequest;

extern VkContext* createVkContext(JNIEnv *env, jobject obj, int clientFd, jobject options);
extern void destroyVkContext(JNIEnv *env, VkContext* context);
extern bool handleExtraDataRequest(VkContext* context, uint16_t requestId, int requestLength);

/* DroidDeck: live-handle tracking (see VkContext.ddLiveInstances). */
static inline void dd_trackHandle(void** slots, void* h) {
    for (int i = 0; i < DD_MAX_LIVE_HANDLES; i++) if (!slots[i]) { slots[i] = h; return; }
}
static inline void dd_untrackHandle(void** slots, void* h) {
    for (int i = 0; i < DD_MAX_LIVE_HANDLES; i++) if (slots[i] == h) slots[i] = NULL;
}

#endif
