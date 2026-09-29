//
// sidworker.cpp
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.








#include "sidworker.h"

#include <limits.h>
#include <stdio.h>

extern "C" {
uint64_t circle_get_ticks64(void);
void sem_dec(uint32_t *semaphore);
void sem_inc(uint32_t *semaphore);
extern volatile unsigned long bmc_giri[4];
}

namespace {

#if BMX_SID_WORKER
struct alignas(64) SidWorkerJob {
  bmx_sid_calculate_fn calculate;
  struct sound_s *sid;
  int16_t *buffer;
  int frames;
  int interleave;
  uint64_t delta_t;
  uint64_t worker_us;
  int result;
};

SidWorkerJob g_job = {};
uint32_t g_request = 0;
uint32_t g_done = 0;
uint32_t g_ready = 0;
uint32_t g_busy = 0;
#ifdef BMX_SID_WORKER_TEST
uint32_t g_stop = 0;
#endif

bool MachineSupportsDualSIDWorker() {
#if defined(RASPI_C64) || defined(RASPI_C64SC) || defined(RASPI_SCPU64) || \
    defined(RASPI_C128)
  return true;
#else
  return false;
#endif
}
#endif

#if BMX_SID_DIAGNOSTICS
const char *MachineName() {
#if defined(RASPI_C64)
  return "c64";
#elif defined(RASPI_C64SC)
  return "c64sc";
#elif defined(RASPI_SCPU64)
  return "scpu64";
#elif defined(RASPI_C128)
  return "c128";
#elif defined(RASPI_VIC20)
  return "vic20";
#elif defined(RASPI_PLUS4)
  return "plus4";
#elif defined(RASPI_PET)
  return "pet";
#else
  return "unknown";
#endif
}

// stdout is a synchronous 115200-baud UART on debug builds.  Keep reports
// sparse enough that transmitting them cannot drain the roughly 35 ms audio
// queue.  On Pi5 these windows produce about one line per minute each.
constexpr uint64_t kPairReportBlocks = 1ULL << 20;
constexpr uint64_t kPCMHashValues = 1ULL << 22;
constexpr uint64_t kFNVOffset = 1469598103934665603ULL;
constexpr uint64_t kFNVPrime = 1099511628211ULL;

struct SidPairWindow {
  uint64_t blocks;
  uint64_t frames;
  uint64_t parallel_blocks;
  uint64_t fallback_blocks;
  uint64_t pair_us;
  uint64_t pair_us_max;
  uint64_t local_us;
  uint64_t worker_us;
  uint64_t wait_us;
  uint64_t sample_count_mismatches;
  uint64_t clock_mismatches;
};

SidPairWindow g_pair = {};
uint64_t g_pcm_hash = kFNVOffset;
uint64_t g_pcm_window_values = 0;
uint64_t g_pcm_total_values = 0;
uint64_t g_pcm_window = 0;
uint64_t g_jobs_submitted = 0;
uint64_t g_jobs_completed = 0;
unsigned g_queue_min_used = UINT_MAX;
uint64_t g_queue_empty_polls = 0;
uint64_t g_queue_polls = 0;

const char *WorkerMode() {
#if BMX_SID_WORKER
  return MachineSupportsDualSIDWorker() ? "on" : "unsupported";
#else
  return "off";
#endif
}

void ResetPairWindow() { g_pair = {}; }

void PrintPairWindow() {
  if (g_pair.blocks == 0) {
    return;
  }

  // Circle's AArch32 printf and the GCC variadic AAPCS alignment disagree
  // after the leading string arguments when several 64-bit values follow.
  // Every reported counter is bounded by this reset window, so publish the
  // metrics as 32-bit values on both boards and keep the wire log identical.
  printf("sidbench: machine=%s worker=%s blocks=%u parallel=%u "
         "fallback=%u frames=%u pair_us_avg=%u pair_us_max=%u "
         "local_us_avg=%u worker_us_avg=%u wait_us_avg=%u "
         "sample_mismatch=%u clock_mismatch=%u\r\n",
         MachineName(), WorkerMode(),
         static_cast<unsigned>(g_pair.blocks),
         static_cast<unsigned>(g_pair.parallel_blocks),
         static_cast<unsigned>(g_pair.fallback_blocks),
         static_cast<unsigned>(g_pair.frames),
         static_cast<unsigned>(g_pair.pair_us / g_pair.blocks),
         static_cast<unsigned>(g_pair.pair_us_max),
         static_cast<unsigned>(g_pair.local_us / g_pair.blocks),
         static_cast<unsigned>(g_pair.worker_us / g_pair.blocks),
         static_cast<unsigned>(g_pair.wait_us / g_pair.blocks),
         static_cast<unsigned>(g_pair.sample_count_mismatches),
         static_cast<unsigned>(g_pair.clock_mismatches));
  ResetPairWindow();
}

void PrintPCMWindow() {
  unsigned queue_min = g_queue_min_used == UINT_MAX ? 0 : g_queue_min_used;
  printf("sidpcm: machine=%s worker=%s window=%u values=%u "
         "hash=%08x%08x jobs_submitted=%u jobs_completed=%u "
         "queue_min_used=%u queue_empty=%u queue_polls=%u\r\n",
         MachineName(), WorkerMode(),
         static_cast<unsigned>(g_pcm_window),
         static_cast<unsigned>(g_pcm_window_values),
         static_cast<unsigned>(g_pcm_hash >> 32),
         static_cast<unsigned>(g_pcm_hash),
         static_cast<unsigned>(g_jobs_submitted),
         static_cast<unsigned>(g_jobs_completed), queue_min,
         static_cast<unsigned>(g_queue_empty_polls),
         static_cast<unsigned>(g_queue_polls));
  ++g_pcm_window;
  g_pcm_hash = kFNVOffset;
  g_pcm_window_values = 0;
  g_jobs_submitted = 0;
  g_jobs_completed = 0;
  g_queue_min_used = UINT_MAX;
  g_queue_empty_polls = 0;
  g_queue_polls = 0;
}

#endif

} // namespace

extern "C" {

void bmx_sid_worker_run(void) {
#if BMX_SID_WORKER
  if (!MachineSupportsDualSIDWorker()) {
    return;
  }

  __atomic_store_n(&g_ready, 1U, __ATOMIC_RELEASE);
  for (;;) {
    sem_dec(&g_request);
#ifdef BMX_SID_WORKER_TEST
    if (__atomic_load_n(&g_stop, __ATOMIC_ACQUIRE) != 0) {
      __atomic_store_n(&g_ready, 0U, __ATOMIC_RELEASE);
      return;
    }
#endif
#if BMX_SID_DIAGNOSTICS
    const uint64_t begin = bmx_sid_diag_now_us();
#endif
    g_job.result = g_job.calculate(g_job.sid, g_job.buffer, g_job.frames,
                                   g_job.interleave, &g_job.delta_t);
#if BMX_SID_DIAGNOSTICS
    g_job.worker_us = bmx_sid_diag_now_us() - begin;
#else
    g_job.worker_us = 0;
#endif
    sem_inc(&g_done);
    bmc_giri[2]++;
  }
#endif
}

int bmx_sid_worker_available(void) {
#if BMX_SID_WORKER
  return MachineSupportsDualSIDWorker() &&
         __atomic_load_n(&g_ready, __ATOMIC_ACQUIRE) != 0;
#else
  return 0;
#endif
}

int bmx_sid_worker_submit(bmx_sid_calculate_fn calculate,
                          struct sound_s *sid, int16_t *buffer, int frames,
                          int interleave, uint64_t delta_t) {
#if BMX_SID_WORKER
  if (!calculate || !sid || !buffer || frames <= 0 || interleave <= 0 ||
      !bmx_sid_worker_available()) {
    return 0;
  }
  if (__atomic_exchange_n(&g_busy, 1U, __ATOMIC_ACQ_REL) != 0) {
    return 0;
  }

  g_job.calculate = calculate;
  g_job.sid = sid;
  g_job.buffer = buffer;
  g_job.frames = frames;
  g_job.interleave = interleave;
  g_job.delta_t = delta_t;
  g_job.worker_us = 0;
  g_job.result = 0;
#if BMX_SID_DIAGNOSTICS
  ++g_jobs_submitted;
#endif
  sem_inc(&g_request);
  return 1;
#else
  (void)calculate;
  (void)sid;
  (void)buffer;
  (void)frames;
  (void)interleave;
  (void)delta_t;
  return 0;
#endif
}

int bmx_sid_worker_wait(int *result, uint64_t *delta_t,
                        uint64_t *worker_us) {
#if BMX_SID_WORKER
  if (__atomic_load_n(&g_busy, __ATOMIC_ACQUIRE) == 0) {
    return 0;
  }
  sem_dec(&g_done);
  if (result) {
    *result = g_job.result;
  }
  if (delta_t) {
    *delta_t = g_job.delta_t;
  }
  if (worker_us) {
    *worker_us = g_job.worker_us;
  }
#if BMX_SID_DIAGNOSTICS
  ++g_jobs_completed;
#endif
  __atomic_store_n(&g_busy, 0U, __ATOMIC_RELEASE);
  return 1;
#else
  (void)result;
  (void)delta_t;
  (void)worker_us;
  return 0;
#endif
}

uint64_t bmx_sid_diag_now_us(void) {
#if BMX_SID_DIAGNOSTICS
  return circle_get_ticks64();
#else
  return 0;
#endif
}

void bmx_sid_diag_record_pair(const struct bmx_sid_pair_metrics *metrics) {
#if BMX_SID_DIAGNOSTICS
  if (!metrics) {
    return;
  }
  ++g_pair.blocks;
  g_pair.frames += metrics->frames;
  g_pair.parallel_blocks += metrics->parallel ? 1U : 0U;
  g_pair.fallback_blocks += metrics->fallback ? 1U : 0U;
  g_pair.pair_us += metrics->pair_us;
  if (metrics->pair_us > g_pair.pair_us_max) {
    g_pair.pair_us_max = metrics->pair_us;
  }
  g_pair.local_us += metrics->local_us;
  g_pair.worker_us += metrics->worker_us;
  g_pair.wait_us += metrics->wait_us;
  g_pair.sample_count_mismatches +=
      metrics->sample_count_mismatch ? 1U : 0U;
  g_pair.clock_mismatches += metrics->clock_mismatch ? 1U : 0U;
  if (g_pair.blocks >= kPairReportBlocks) {
    PrintPairWindow();
  }
#else
  (void)metrics;
#endif
}

void bmx_sid_diag_record_pcm(const int16_t *samples, size_t count) {
#if BMX_SID_DIAGNOSTICS
  if (!samples) {
    return;
  }
  for (size_t i = 0; i < count; ++i) {
    uint16_t value = static_cast<uint16_t>(samples[i]);
    g_pcm_hash ^= value & 0xffU;
    g_pcm_hash *= kFNVPrime;
    g_pcm_hash ^= value >> 8;
    g_pcm_hash *= kFNVPrime;
    ++g_pcm_window_values;
    ++g_pcm_total_values;
    if (g_pcm_window_values == kPCMHashValues) {
      PrintPCMWindow();
    }
  }
#else
  (void)samples;
  (void)count;
#endif
}

void bmx_sid_diag_record_queue(unsigned capacity_frames,
                               unsigned free_frames) {
#if BMX_SID_DIAGNOSTICS
  if (g_pcm_total_values == 0 || capacity_frames == 0) {
    return;
  }
  if (free_frames > capacity_frames) {
    free_frames = capacity_frames;
  }
  const unsigned used_frames = capacity_frames - free_frames;
  if (used_frames < g_queue_min_used) {
    g_queue_min_used = used_frames;
  }
  if (used_frames == 0) {
    ++g_queue_empty_polls;
  }
  ++g_queue_polls;
#else
  (void)capacity_frames;
  (void)free_frames;
#endif
}

#ifdef BMX_SID_WORKER_TEST
void bmx_sid_worker_test_stop(void) {
#if BMX_SID_WORKER
  __atomic_store_n(&g_stop, 1U, __ATOMIC_RELEASE);
  sem_inc(&g_request);
#endif
}
#endif

} // extern "C"
