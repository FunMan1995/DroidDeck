/*
 * DroidDeck: AHardwareBuffer_getFd() for the Vortek renderer, which declares it extern
 * (resource_memory.c). Upstream defines it in Winlator's gpu_image.c (brunodev85/winlator-app
 * @ 3981d86e, app/src/main/cpp/winlator/src/gpu_image.c, LGPL-2.1); carried here alone so the
 * EGL/GLES half of that file is not linked.
 */
#include <android/hardware_buffer.h>
#include "native_handle.h"

extern const native_handle_t* _Nullable AHardwareBuffer_getNativeHandle(const AHardwareBuffer* _Nonnull buffer);

int AHardwareBuffer_getFd(AHardwareBuffer* hardwareBuffer) {
    const native_handle_t* nativeHandle = AHardwareBuffer_getNativeHandle(hardwareBuffer);
    return nativeHandle && nativeHandle->numFds > 0 ? nativeHandle->data[0] : -1;
}
