//
// kernel.cpp
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

#include <algorithm>
#include <cassert>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <climits>
#include <limits>

namespace
{

void cxx_test(void);

}

CKernel::CKernel(void) : CStdlibAppStdio("04-std")
{
    mActLED.Blink(5); // show we are alive
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
    mLogger.Write(GetKernelName(), LogNotice, "C++ Standard Library Demo");

    cxx_test();

    mLogger.Write(GetKernelName(), LogNotice,
                  "C++ Standard Library Test finished");

    return ShutdownHalt;
}

namespace
{
struct ooops : std::exception
{
    const char *what() const noexcept
    {
        return "Ooops!";
    }
};

void barf(void)
{
    std::cerr << "Throwing exception..." << std::endl;
    throw ooops();
}

struct a
{
    static unsigned long counter;
    a()
    {
        counter += 1;
    }
    ~a()
    {
        assert(counter > 0);
        counter -= 1;
        if (counter == 0)
        {
            std::cout << "all 'struct a' instances cleaned up..." << std::endl;
        }
    }
    int filler[5];
};

unsigned long a::counter = 0;

void cxx_test(void)
{
    std::vector<std::string> const v = {"vector entry 1", "vector entry 2"};

    std::cout << "Opening file via std::ofstream..." << std::endl;

    std::ofstream ofs("test.txt", std::ofstream::out);
    if (!ofs.is_open())
    {
        std::cerr << "Failed to open file 'test.txt'!" << std::endl;
        return;
    }

    try
    {
        barf();
    }
    catch (std::exception &e)
    {
        std::cerr << "Caught exception..." << std::endl;
        ofs << "lorem ipsum" << std::endl;
        std::string s(e.what());
        ofs << s.c_str() << std::endl;
    }
    std::cout << "Use <algorithm>..." << std::endl;
    for_each(v.begin(), v.end(),
             [&](std::string const &s) { ofs << s.c_str() << std::endl; });

    // Test out-of-memory condition
    try
    {
        std::cout << "size of a: " << sizeof(a) << std::endl;

        std::vector<std::unique_ptr<a[]>> ptrs;
        while (true)
        {
            // provoke out-of-memory error, destructors of "a" and of the vector
            // must be called
            std::cout << "Allocating large array of 'a' instances" << std::endl;
            ptrs.emplace_back(new a[10000000U]);
            std::cout << "Allocated pointer " << std::hex << ptrs.back().get()
                      << std::endl;
        }
    }
    catch (std::bad_alloc &ba)
    {
        std::cerr << "bad_alloc caught: " << ba.what() << std::endl;
    }

    ofs.close();

    std::wstring const ws = L"Hello, world with wchar_t!";
    std::wcout << ws.c_str() << std::endl;

    std::string_view const s{"Hello, world with std::string_view!"};
    std::cout << s << std::endl;

    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 9);

    // Generate and print 10 random numbers in the range [0, 9].
    std::cout << "Random numbers:";
    for (int i = 0; i < 10; ++i)
    {
        std::cout << " " << dist(rd);
    }
    std::cout << std::endl;

    std::cout << "Limits:" << std::endl;
    std::cout << "  long long: " << std::numeric_limits<long long>::min()
              << " to " << std::numeric_limits<long long>::max() << std::endl;
    
    std::cout << "Limit macros:" << std::endl;
    std::cout << "  LLONG_MIN: " << LLONG_MIN << std::endl;
    std::cout << "  LLONG_MAX: " << LLONG_MAX << std::endl;
}
} // namespace
