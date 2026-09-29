//
// sidworker.h
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

#ifndef bmx_sidworker_h
#define bmx_sidworker_h

#include <stddef.h>
#include <stdint.h>

#ifndef BMX_SID_WORKER
#define BMX_SID_WORKER 0
#endif

#ifndef BMX_SID_DIAGNOSTICS
#define BMX_SID_DIAGNOSTICS 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct sound_s;

typedef int (*bmx_sid_calculate_fn)(struct sound_s *sid, int16_t *buffer,
                                    int frames, int interleave,
                                    uint64_t *delta_t);

struct bmx_sid_pair_metrics {
  uint64_t pair_us;
  uint64_t local_us;
  uint64_t worker_us;
  uint64_t wait_us;
  uint32_t frames;
  int parallel;
  int fallback;
  int sample_count_mismatch;
  int clock_mismatch;
};

// Core 2 entry point. Returns immediately when the build or current machine
// does not support the worker; otherwise it never returns.
void bmx_sid_worker_run(void);

// The producer is the VICE thread on core 1. There can be at most one
// outstanding job, and a successful submit must be paired with wait.
int bmx_sid_worker_available(void);
int bmx_sid_worker_submit(bmx_sid_calculate_fn calculate,
                          struct sound_s *sid, int16_t *buffer, int frames,
                          int interleave, uint64_t delta_t);
int bmx_sid_worker_wait(int *result, uint64_t *delta_t,
                        uint64_t *worker_us);

uint64_t bmx_sid_diag_now_us(void);
void bmx_sid_diag_record_pair(const struct bmx_sid_pair_metrics *metrics);
void bmx_sid_diag_record_pcm(const int16_t *samples, size_t count);
void bmx_sid_diag_record_queue(unsigned capacity_frames,
                               unsigned free_frames);

#ifdef BMX_SID_WORKER_TEST
void bmx_sid_worker_test_stop(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
