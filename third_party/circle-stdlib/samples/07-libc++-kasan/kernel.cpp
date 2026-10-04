//
// kernel.cpp
//
// Demonstrates KASan container overflow detection for libc++ containers.
// Build with KASAN_ENABLED=1 to activate the sanitizer.
//
// Each test function commits a deliberate out-of-bounds read that falls
// inside the allocated storage but outside the container's live region.
// libc++ annotates that gap via the sanitizer hooks, so the read hits a
// shadow byte of 0xfc (container overflow) and KASan logs the violation.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "kernel.h"

#include <cstdio>
#include <deque>
#include <string>
#include <vector>

namespace
{

// Test 1: std::vector container overflow
//
// __sanitizer_annotate_contiguous_container is called by libc++ on every
// size change.  After reserve(16) + push_back(42), size==1 and capacity>=16.
// The region [v.data()+1, v.data()+capacity) is poisoned with 0xfc.
// Reading v.data()[5] through a volatile pointer bypasses bounds checking
// and hits the poisoned shadow, triggering a KASan container-overflow report.
void test_vector_overflow()
{
    std::printf("--- Test 1: std::vector container overflow ---\n");

    std::vector<int> v;
    v.reserve(16);
    v.push_back(42);
    // v.size()==1, v.capacity()>=16; v.data()[1..15] is poisoned (0xfc).
    volatile int const *const p = v.data();
    volatile int const legal = p[0]; // legal: index 0 is within size
    (void)legal;
    volatile int const x = p[5]; // KASan: container overflow
    (void)x;

    std::printf("Test 1 complete\n");
}

// Test 2: std::string container overflow
//
// Same mechanism as Test 1 but for std::basic_string.
// After reserve(64) + assignment of "hello", size==5 and capacity>=64.
// The region [s.data()+5, s.data()+capacity) is poisoned with 0xfc.
void test_string_overflow()
{
    std::printf("--- Test 2: std::string container overflow ---\n");

    std::string s;
    s.reserve(64);
    s = "hello";
    // s.size()==5, s.capacity()>=64; s.data()[5..63] is poisoned (0xfc).
    volatile char const *const p = s.data();
    volatile char const legal = p[4]; // legal: index 4 is within size
    (void)legal;

    // TODO: This illegal access should be detected, but it is not.
    std::printf("Note: The following access is expected to trigger a KASan container overflow report, but it is not detected.\n");
    volatile char const c = p[32]; // KASan: container overflow
    (void)c;

    std::printf("Test 2 complete\n");
}

// Test 3: std::deque container overflow
//
// __sanitizer_annotate_double_ended_contiguous_container is called by
// libc++ on every push_back/push_front.  Each internal deque chunk has
// unused space before the first live element and after the last live
// element; both regions are poisoned with 0xfc.
//
// libc++ initialises __start_ to __block_size/2 when the first block is
// allocated, so there are __block_size/2 unused slots before d.front()
// even though all three elements were pushed to the back.  Reading one
// int before &d.front() therefore hits the poisoned front gap.
//
// __sanitizer_verify_double_ended_contiguous_container is exercised by
// libc++'s internal consistency checks that follow deque mutations.
void test_deque_overflow()
{
    std::printf("--- Test 3: std::deque container overflow ---\n");

    std::deque<int> d;
    d.push_back(1);
    d.push_back(2);
    d.push_back(3);

    // Legal access: read the middle element via operator[].
    volatile int const legal = d[1]; // legal: index 1 is within size
    (void)legal;

    // Access one int before the front element.  The chunk was allocated
    // with __start_ at its midpoint, leaving poisoned space at the front.
    volatile int const *const q = &d.front();
    volatile int const y = q[-1]; // KASan: container overflow before live region
    (void)y;

    // Access one int past the back element.  The memory is within the
    // same chunk but beyond the back boundary, still poisoned.
    volatile int const *const p = &d.back();
    volatile int const x = p[1]; // KASan: container overflow past live region
    (void)x;

    std::printf("Test 3 complete\n");
}

} // namespace

CKernel::CKernel(void)
    : CStdlibAppStdio("07-libc++-kasan")
{
    mActLED.Blink(5);
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
    mLogger.Write(GetKernelName(), LogNotice,
        "libc++ KASan container overflow demo");
    mLogger.Write(GetKernelName(), LogNotice,
        "Build with KASAN_ENABLED=1 to activate sanitizer checks");

    test_vector_overflow();
    test_string_overflow();
    test_deque_overflow();

    mLogger.Write(GetKernelName(), LogNotice, "Demo complete");
    return ShutdownHalt;
}
