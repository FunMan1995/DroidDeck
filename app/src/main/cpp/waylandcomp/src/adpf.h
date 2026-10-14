#ifndef DROIDDECK_ADPF_H
#define DROIDDECK_ADPF_H
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* ADPF (APerformanceHint) for the compositor thread, resolved at runtime (brief 004 §3). */
int adpf_available(void);
/* Compositor thread only: per composed frame, work time and target (1e9 / refresh). */
void adpf_frame(int64_t work_ns, int64_t target_ns);
/* Experimental: add guest tids (API 34 setThreads); 0 or -errno, compositor-only on failure. */
int adpf_add_threads(const pid_t *tids, size_t n);
void adpf_close(void);

#endif
