#include "testkernel.h"

#include <mutex>
#include <string>
#include <thread>
#include <exception>
#include <condition_variable>
#include <future>
#include <vector>
#include <numeric>
#include <chrono>
#include <atomic>

int main(void)
{
    return CTestKernel::RunTests("06-basic-thread");
}

TEST_CASE("Basic thread operations")
{
    std::mutex m;
    int counter = 0;

    auto thread_func = [&m, &counter]()
    {
        for (int i = 0; i < 100; ++i)
        {
            std::lock_guard<std::mutex> lock(m);
            counter++;
        }
    };

    MESSAGE("Creating threads");
    std::thread t1(thread_func);
    std::thread t2(thread_func);

    MESSAGE("Joining threads");
    t1.join();
    t2.join();

    REQUIRE(counter == 200);
    MESSAGE("Thread test successful");
}

/*
 * Test std::call_once.
 * based on https://en.cppreference.com/w/cpp/thread/call_once
 */
TEST_CASE("std::call_once test")
{
    std::once_flag flag1, flag2;
    int simple_called = 0;
    int may_throw_called = 0;
    int may_throw_did_not_throw = 0;

    auto simple_do_once = [&flag1, &simple_called]()
    {
        std::call_once(flag1, [&simple_called](){
            MESSAGE("Simple example: called once");
            simple_called++;
        });
    };

    auto may_throw_function = [&may_throw_called, &may_throw_did_not_throw](bool do_throw)
    {
        may_throw_called++;
        if (do_throw)
        {
            MESSAGE("Throw: call_once will retry");
            throw std::exception();
        }
        MESSAGE("Did not throw, call_once will not attempt again");
        may_throw_did_not_throw++;
    };

    auto do_once = [&flag2, &may_throw_function](bool do_throw)
    {
        try
        {
            std::call_once(flag2, may_throw_function, do_throw);
        }
        catch (...)
        {
        }
    };

    MESSAGE("Testing simple_do_once with 4 threads");
    std::thread st1(simple_do_once);
    std::thread st2(simple_do_once);
    std::thread st3(simple_do_once);
    std::thread st4(simple_do_once);
    st1.join();
    st2.join();
    st3.join();
    st4.join();

    REQUIRE(simple_called == 1);

    MESSAGE("Testing may_throw_function with 4 threads");
    std::thread t1(do_once, true);
    std::thread t2(do_once, true);
    std::thread t3(do_once, false);
    std::thread t4(do_once, true);
    t1.join();
    t2.join();
    t3.join();
    t4.join();

    REQUIRE(may_throw_did_not_throw == 1);
    REQUIRE(may_throw_called >= 1);
}

/*
 * Test std::condition_variable example
 * based on https://en.cppreference.com/w/cpp/thread/condition_variable
 */
TEST_CASE("std::condition_variable test")
{
    std::mutex m;
    std::condition_variable cv;
    std::string data;
    bool ready = false;
    bool processed = false;

    auto worker_thread = [&m, &cv, &data, &ready, &processed]()
    {
        // wait until main() sends data
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&ready]{ return ready; });

        // after the wait, we own the lock
        MESSAGE("Worker thread is processing data");
        data += " after processing";

        // send data back to main()
        processed = true;
        MESSAGE("Worker thread signals data processing completed");

        // manual unlocking is done before notifying, to avoid waking up
        // the waiting thread only to block again (see notify_one for details)
        lk.unlock();
        cv.notify_one();
    };

    MESSAGE("Testing std::condition_variable with 2 threads");
    std::thread worker(worker_thread);

    data = "Example data";
    // send data to the worker thread
    {
        std::lock_guard<std::mutex> lk(m);
        ready = true;
        MESSAGE("main() signals data ready for processing");
    }
    cv.notify_one();

    // wait for the worker
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [&processed]{ return processed; });
    }
    MESSAGE("Back in main(), data checked");
    REQUIRE(data == "Example data after processing");

    worker.join();
}

/*
 * Test std::promise and std::future
 * based on https://en.cppreference.com/w/cpp/thread/promise
 */
TEST_CASE("std::promise and std::future test")
{
    auto accumulate_func = [](std::vector<int>::iterator first,
                              std::vector<int>::iterator last,
                              std::promise<int> accumulate_promise)
    {
        int sum = std::accumulate(first, last, 0);
        accumulate_promise.set_value(sum);  // Notify future
    };

    auto do_work_func = [](std::promise<void> barrier)
    {
        MESSAGE("Worker thread sleeping for a bit");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        barrier.set_value();
    };

    MESSAGE("Demonstrate using promise<int>");
    std::vector<int> numbers = {1, 2, 3, 4, 5, 6};
    std::promise<int> accumulate_promise;
    std::future<int> accumulate_future = accumulate_promise.get_future();
    std::thread work_thread(accumulate_func, numbers.begin(), numbers.end(),
                            std::move(accumulate_promise));

    int result = accumulate_future.get();
    MESSAGE("result=" << result);
    REQUIRE(result == 21);
    work_thread.join();

    MESSAGE("Demonstrate using promise<void>");
    std::promise<void> barrier;
    std::future<void> barrier_future = barrier.get_future();
    std::thread new_work_thread(do_work_func, std::move(barrier));
    barrier_future.wait();
    new_work_thread.join();
}

/*
 * Test thread_local variables
 */
TEST_CASE("thread_local test")
{
    static std::atomic<int> destructor_count{0};
    
    struct ThreadLocalData {
        int value;
        ThreadLocalData() : value(0) {}
        ~ThreadLocalData() {
            destructor_count++;
        }
    };
    
    auto thread_func = [](int id) {
        thread_local ThreadLocalData tl_data;
        tl_data.value = id;
        
        std::this_thread::yield();
        
        REQUIRE(tl_data.value == id);
    };

    MESSAGE("Creating 3 threads to test thread_local");
    std::thread t1(thread_func, 1);
    std::thread t2(thread_func, 2);
    std::thread t3(thread_func, 3);
    
    t1.join();
    t2.join();
    t3.join();
    
    REQUIRE(destructor_count.load() == 3);
    MESSAGE("thread_local destructors executed correctly");

    auto incrementing_thread_func = [](int thread_id) {
        thread_local int counter = 0;
        counter++;
        
        MESSAGE("Thread " << thread_id << " call " << counter << " counter value: " << counter);
        REQUIRE(counter > 0);

        auto const save_counter = counter;

        // Allow other threads to run and potentially call this function again to check if
        // the counter retains its value across calls within the same thread.
        std::this_thread::yield();

        REQUIRE(counter == save_counter);
        
        return counter;
    };

    MESSAGE("Testing thread_local value retention across multiple calls");
    std::thread t4([&]() {
        int result1 = incrementing_thread_func(4);
        int result2 = incrementing_thread_func(4);
        int result3 = incrementing_thread_func(4);
        
        REQUIRE(result1 == 1);
        REQUIRE(result2 == 2);
        REQUIRE(result3 == 3);
        MESSAGE("Thread 4: counter values " << result1 << ", " << result2 << ", " << result3);
    });
    
    std::thread t5([&]() {
        int result1 = incrementing_thread_func(5);
        int result2 = incrementing_thread_func(5);
        
        REQUIRE(result1 == 1);
        REQUIRE(result2 == 2);
        MESSAGE("Thread 5: counter values " << result1 << ", " << result2);
    });
    
    t4.join();
    t5.join();
    MESSAGE("thread_local value retention test completed");
}
