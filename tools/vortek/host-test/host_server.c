/*
 * vortek-host-server: the DroidDeck Vortek renderer built for x86_64 Linux (brief 003 §4.2).
 * Part of the DroidDeck build of the Vortek renderer, LGPL-2.1 (tools/vortek/LICENSE).
 *
 * Replaces VortekServerComponent.kt for host testing: listens on $VORTEK_SERVER_PATH, runs the
 * same create-context / extra-data protocol, and hands each client to createVkContext(). The
 * renderer's vulkanWrapper dlopens the host libvulkan.so.1, so with the brief's environment it
 * reaches lavapipe through the Khronos profiles layer (Pixel 9a profile, not komodo).
 * JNI is replaced by a tiny fake JNIEnv that serves the Options fields and nothing else.
 */
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <jni.h>
#include "vk_context.h"
#include "vulkan_helper.h"

extern VulkanWrapper vulkanWrapper;
extern bool handleExtraDataRequest(VkContext* context, uint16_t requestId, int requestLength);

#define REQUEST_CODE_CREATE_CONTEXT 1
#define REQUEST_CODE_SEND_EXTRA_DATA 2

/* ---- fake JNI ------------------------------------------------------------------------------ */
enum { F_VK_MAX_VERSION = 1, F_MAX_DEVICE_MEMORY, F_IMAGE_CACHE_SIZE, F_RESOURCE_MEMORY_TYPE, F_EXPOSED_EXTENSIONS };
static struct { jint vkMaxVersion; jshort maxDeviceMemory, imageCacheSize; jbyte resourceMemoryType; } options = {
    (1 << 22) | (3 << 12) | 128, 0, 256, 0};

static jclass JNICALL fGetObjectClass(JNIEnv* e, jobject o) { (void)e; return (jclass)o; }
static void JNICALL fDeleteLocalRef(JNIEnv* e, jobject o) { (void)e; (void)o; }
static jobject JNICALL fNewGlobalRef(JNIEnv* e, jobject o) { (void)e; return o ? o : (jobject)1; }
static void JNICALL fDeleteGlobalRef(JNIEnv* e, jobject o) { (void)e; (void)o; }
static jmethodID JNICALL fGetMethodID(JNIEnv* e, jclass c, const char* n, const char* s) { (void)e; (void)c; (void)n; (void)s; return (jmethodID)1; }
static jfieldID JNICALL fGetFieldID(JNIEnv* e, jclass c, const char* name, const char* sig) {
    (void)e; (void)c; (void)sig;
    if (!strcmp(name, "vkMaxVersion")) return (jfieldID)F_VK_MAX_VERSION;
    if (!strcmp(name, "maxDeviceMemory")) return (jfieldID)F_MAX_DEVICE_MEMORY;
    if (!strcmp(name, "imageCacheSize")) return (jfieldID)F_IMAGE_CACHE_SIZE;
    if (!strcmp(name, "resourceMemoryType")) return (jfieldID)F_RESOURCE_MEMORY_TYPE;
    if (!strcmp(name, "exposedDeviceExtensions")) return (jfieldID)F_EXPOSED_EXTENSIONS;
    return 0;
}
static jobject JNICALL fGetObjectField(JNIEnv* e, jobject o, jfieldID f) { (void)e; (void)o; (void)f; return NULL; }
static jint JNICALL fGetIntField(JNIEnv* e, jobject o, jfieldID f) { (void)e; (void)o; return (intptr_t)f == F_VK_MAX_VERSION ? options.vkMaxVersion : 0; }
static jshort JNICALL fGetShortField(JNIEnv* e, jobject o, jfieldID f) {
    (void)e; (void)o;
    return (intptr_t)f == F_MAX_DEVICE_MEMORY ? options.maxDeviceMemory : (intptr_t)f == F_IMAGE_CACHE_SIZE ? options.imageCacheSize : 0;
}
static jbyte JNICALL fGetByteField(JNIEnv* e, jobject o, jfieldID f) { (void)e; (void)o; return (intptr_t)f == F_RESOURCE_MEMORY_TYPE ? options.resourceMemoryType : 0; }
static jboolean JNICALL fGetBooleanField(JNIEnv* e, jobject o, jfieldID f) { (void)e; (void)o; (void)f; return 0; }
static jint JNICALL fGetVersion(JNIEnv* e) { (void)e; return JNI_VERSION_1_6; }
static jint JNICALL fCallIntMethod(JNIEnv* e, jobject o, jmethodID m, ...) { (void)e; (void)o; (void)m; return 0; }
static jlong JNICALL fCallLongMethod(JNIEnv* e, jobject o, jmethodID m, ...) { (void)e; (void)o; (void)m; return 0; }
static void JNICALL fCallVoidMethod(JNIEnv* e, jobject o, jmethodID m, ...) { (void)e; (void)o; (void)m; }
static const char* JNICALL fGetStringUTFChars(JNIEnv* e, jstring s, jboolean* c) { (void)e; if (c) *c = 0; return (const char*)s; }
static void JNICALL fReleaseStringUTFChars(JNIEnv* e, jstring s, const char* c) { (void)e; (void)s; (void)c; }

static struct JNINativeInterface_ fakeInterface;
static JNIEnv fakeEnv = &fakeInterface;
static jint JNICALL fGetJavaVM(JNIEnv* e, JavaVM** vm);
static jint JNICALL vAttach(JavaVM* vm, void** env, void* args) { (void)vm; (void)args; *env = &fakeEnv; return 0; }
static jint JNICALL vDetach(JavaVM* vm) { (void)vm; return 0; }
static struct JNIInvokeInterface_ fakeInvoke = {.AttachCurrentThread = vAttach, .DetachCurrentThread = vDetach};
static JavaVM fakeVM = &fakeInvoke;
static jint JNICALL fGetJavaVM(JNIEnv* e, JavaVM** vm) { (void)e; *vm = &fakeVM; return 0; }

static void initFakeJni(void) {
    fakeInterface.GetVersion = fGetVersion;
    fakeInterface.GetObjectClass = fGetObjectClass;
    fakeInterface.DeleteLocalRef = fDeleteLocalRef;
    fakeInterface.NewGlobalRef = fNewGlobalRef;
    fakeInterface.DeleteGlobalRef = fDeleteGlobalRef;
    fakeInterface.GetMethodID = fGetMethodID;
    fakeInterface.GetFieldID = fGetFieldID;
    fakeInterface.GetObjectField = fGetObjectField;
    fakeInterface.GetIntField = fGetIntField;
    fakeInterface.GetShortField = fGetShortField;
    fakeInterface.GetByteField = fGetByteField;
    fakeInterface.GetBooleanField = fGetBooleanField;
    fakeInterface.CallIntMethod = fCallIntMethod;
    fakeInterface.CallLongMethod = fCallLongMethod;
    fakeInterface.CallVoidMethod = fCallVoidMethod;
    fakeInterface.GetStringUTFChars = fGetStringUTFChars;
    fakeInterface.ReleaseStringUTFChars = fReleaseStringUTFChars;
    fakeInterface.GetJavaVM = fGetJavaVM;
}

/* AHardwareBuffer is unavailable on the host (ahb_compat.c is Android-only). */
int AHardwareBuffer_getFd(void* buffer) { (void)buffer; return -1; }

/* ---- server loop (mirrors VortekServerComponent.serve) ------------------------------------- */
static bool readFully(int fd, void* buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t r = read(fd, (char*)buf + off, n - off);
        if (r < 0 && errno == EINTR) continue;
        if (r <= 0) return false;
        off += (size_t)r;
    }
    return true;
}

static void* serveClient(void* arg) {
    int fd = (int)(intptr_t)arg;
    VkContext* context = NULL;
    int header[2];
    while (readFully(fd, header, sizeof(header))) {
        int code = header[0], length = header[1];
        if (code == REQUEST_CODE_CREATE_CONTEXT && !context) {
            context = createVkContext(&fakeEnv, (jobject)1, fd, (jobject)1);
            if (!context) break;
            fprintf(stderr, "vortek-host-server: client fd %d connected\n", fd);
        } else if (code > INT16_MAX && (code >> 16) == REQUEST_CODE_SEND_EXTRA_DATA && context) {
            if (!handleExtraDataRequest(context, code & 0xffff, length)) break;
        } else {
            fprintf(stderr, "vortek-host-server: unexpected request %d\n", code);
            break;
        }
    }
    if (context) destroyVkContext(&fakeEnv, context);
    close(fd);
    fprintf(stderr, "vortek-host-server: client fd %d closed\n", fd);
    return NULL;
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    initFakeJni();
    const char* path = getenv("VORTEK_SERVER_PATH");
    if (!path) { fprintf(stderr, "set VORTEK_SERVER_PATH\n"); return 2; }
    const char* lib = getenv("VORTEK_HOST_LIBVULKAN");
    void* libvulkan = dlopen(lib ? lib : "libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan) { fprintf(stderr, "dlopen libvulkan: %s\n", dlerror()); return 1; }
    initVulkanWrapper(&vulkanWrapper, libvulkan);

    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    unlink(path);
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) != 0 || listen(s, 16) != 0) { perror("bind/listen"); return 1; }
    fprintf(stderr, "vortek-host-server: listening on %s (pid %d)\n", path, getpid());
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; perror("accept"); return 1; }
        pthread_t t;
        pthread_create(&t, NULL, serveClient, (void*)(intptr_t)c);
        pthread_detach(t);
    }
}
