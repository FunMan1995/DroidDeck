/* Host-test stub (tools/vortek/host-test): AHardwareBuffer is unavailable on the host;
 * allocation always fails so the renderer takes its non-AHB paths. */
#pragma once
#include <stdint.h>
typedef struct AHardwareBuffer AHardwareBuffer;
typedef struct AHardwareBuffer_Desc { uint32_t width, height, layers, format; uint64_t usage; uint32_t stride, rfu0; uint64_t rfu1; } AHardwareBuffer_Desc;
enum { AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM = 1, AHARDWAREBUFFER_FORMAT_BLOB = 0x21 };
enum { AHARDWAREBUFFER_USAGE_CPU_READ_OFTEN = 3, AHARDWAREBUFFER_USAGE_CPU_WRITE_OFTEN = 3 << 4,
       AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE = 1 << 8, AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT = 1 << 9,
       AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER = 1 << 24 };
static inline int AHardwareBuffer_allocate(const AHardwareBuffer_Desc* d, AHardwareBuffer** out) { (void)d; *out = 0; return -1; }
static inline void AHardwareBuffer_describe(const AHardwareBuffer* b, AHardwareBuffer_Desc* d) { (void)b; *d = (AHardwareBuffer_Desc){0}; }
static inline void AHardwareBuffer_release(AHardwareBuffer* b) { (void)b; }
