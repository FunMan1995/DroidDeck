/*
 * ADPF hint session for the compositor thread (brief 004 §3).
 *
 * APerformanceHint_* is NDK API 33 (setThreads 34); minSdk is 26, so everything is resolved
 * with dlsym from libandroid.so and nothing links against it. Without the API, or if the
 * platform refuses a session, this stays off and logs why once.
 * The session lives on the compositor thread: created on the first composed frame with that
 * thread's tid and target = 1e9 / refresh, fed reportActualWorkDuration() per frame, and
 * updateTargetWorkDuration() when the output refresh rate changes.
 */
#include "adpf.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* from droiddeck_ext.c; declared here so this file needs no wayland headers */
void droiddeck_log(const char *tag, const char *fmt, ...);

typedef void *(*pfn_getManager)(void);
typedef void *(*pfn_createSession)(void *manager, const int32_t *tids, size_t n, int64_t target_ns);
typedef int (*pfn_updateTarget)(void *session, int64_t target_ns);
typedef int (*pfn_reportActual)(void *session, int64_t actual_ns);
typedef void (*pfn_closeSession)(void *session);
typedef int (*pfn_setThreads)(void *session, const pid_t *tids, size_t n);

static struct {
    pfn_getManager get_manager;
    pfn_createSession create;
    pfn_updateTarget update_target;
    pfn_reportActual report;
    pfn_closeSession close;
    pfn_setThreads set_threads;
} api;

static pthread_once_t api_once = PTHREAD_ONCE_INIT;
static int api_ok;
static void *session;
static int64_t session_target_ns;
static pid_t session_tid;
static int failed; /* 1 = gave up for this process (logged once) */
static unsigned report_errors;

static void load_api(void) {
    void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (!lib) return;
    api.get_manager = (pfn_getManager)dlsym(lib, "APerformanceHint_getManager");
    api.create = (pfn_createSession)dlsym(lib, "APerformanceHint_createSession");
    api.update_target = (pfn_updateTarget)dlsym(lib, "APerformanceHint_updateTargetWorkDuration");
    api.report = (pfn_reportActual)dlsym(lib, "APerformanceHint_reportActualWorkDuration");
    api.close = (pfn_closeSession)dlsym(lib, "APerformanceHint_closeSession");
    api.set_threads = (pfn_setThreads)dlsym(lib, "APerformanceHint_setThreads"); /* API 34, optional */
    api_ok = api.get_manager && api.create && api.update_target && api.report && api.close;
}

int adpf_available(void) {
    pthread_once(&api_once, load_api);
    return api_ok;
}

void adpf_frame(int64_t work_ns, int64_t target_ns) {
    if (failed || work_ns <= 0 || target_ns <= 0) return;
    if (!session) {
        if (!adpf_available()) {
            failed = 1;
            droiddeck_log("perf", "adpf: APerformanceHint unavailable (needs Android 13); compositor runs without a hint session");
            return;
        }
        void *manager = api.get_manager();
        const int32_t tid = (int32_t)gettid();
        session = manager ? api.create(manager, &tid, 1, target_ns) : NULL;
        if (!session) {
            failed = 1;
            droiddeck_log("perf", "adpf: the platform refused a hint session (manager %s); compositor runs without one",
                          manager ? "present" : "missing");
            return;
        }
        session_tid = tid;
        session_target_ns = target_ns;
        droiddeck_log("perf", "adpf: hint session for compositor thread %d, target %.2f ms", (int)tid, target_ns / 1e6);
    }
    if (target_ns != session_target_ns) {
        const int r = api.update_target(session, target_ns);
        droiddeck_log("perf", "adpf: target %.2f -> %.2f ms (%s)", session_target_ns / 1e6, target_ns / 1e6,
                      r == 0 ? "ok" : strerror(r));
        if (r == 0) session_target_ns = target_ns;
    }
    const int r = api.report(session, work_ns);
    if (r != 0 && report_errors++ < 3)
        droiddeck_log("perf", "adpf: reportActualWorkDuration failed: %s", strerror(r));
}

int adpf_add_threads(const pid_t *tids, size_t n) {
    if (!session) return -EAGAIN;
    if (!api.set_threads) return -ENOSYS;
    if (n > 64) n = 64;
    pid_t all[65];
    all[0] = session_tid;
    memcpy(all + 1, tids, n * sizeof(pid_t));
    const int r = api.set_threads(session, all, n + 1);
    if (r != 0) {
        /* Foreign (proot guest) tids may be refused; keep the compositor-only session. */
        api.set_threads(session, all, 1);
        droiddeck_log("perf", "adpf: guest threads refused (%s); compositor-only", strerror(r));
        return -r;
    }
    droiddeck_log("perf", "adpf: hint session now covers the compositor and %zu guest threads", n);
    return 0;
}

void adpf_close(void) {
    if (session) api.close(session);
    session = NULL;
    session_target_ns = 0;
}
