#include "testkernel.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

// Minimal recursive removal helper because the standard remove_all relies on
// POSIX openat/fdopendir which aren't fully supported in this environment
// currently.
uintmax_t remove_all_manual(fs::path const &p)
{
    if (!fs::exists(p))
        return 0;
    uintmax_t count = 0;
    if (fs::is_directory(p))
    {
        for (auto const &entry : fs::directory_iterator(p))
        {
            count += remove_all_manual(entry.path());
        }
    }
    if (fs::remove(p))
        count++;
    return count;
}

int main(void)
{
    return CTestKernel::RunTests("06-filesystem");
}

TEST_CASE("Path manipulation")
{
    fs::path const p1 = "/usr/bin/clang";
    REQUIRE(p1.string() == "/usr/bin/clang");
    REQUIRE(p1.parent_path().string() == "/usr/bin");
    REQUIRE(p1.filename().string() == "clang");
    REQUIRE(p1.extension().string() == "");

    fs::path const p2 = "test.txt";
    REQUIRE(p2.extension().string() == ".txt");
    REQUIRE(p2.stem().string() == "test");

    fs::path p3 = "/tmp";
    p3 /= "subdir";
    p3 /= "file.dat";
    REQUIRE(p3.string() == "/tmp/subdir/file.dat");
}

TEST_CASE("Directory operations")
{
    fs::path const test_dir = "fs_test_dir";

    try
    {
        if (fs::exists(test_dir))
        {
            MESSAGE("Cleaning up existing test_dir");
            remove_all_manual(test_dir);
        }

        REQUIRE_FALSE(fs::exists(test_dir));

        MESSAGE("Creating test_dir");
        REQUIRE(fs::create_directory(test_dir));
        REQUIRE(fs::exists(test_dir));
        REQUIRE(fs::is_directory(test_dir));

        fs::path const nested_dir = test_dir / "a" / "b" / "c";
        MESSAGE("Creating nested_dir: " << nested_dir.string());
        REQUIRE(fs::create_directories(nested_dir));
        REQUIRE(fs::exists(nested_dir));
        REQUIRE(fs::is_directory(nested_dir));

        MESSAGE("Removing leaf directory: " << nested_dir.string());
        REQUIRE(fs::remove(nested_dir));
        REQUIRE_FALSE(fs::exists(nested_dir));

        MESSAGE("Testing manual recursive removal on test_dir");
        uintmax_t const removed = remove_all_manual(test_dir);
        MESSAGE("Removed " << removed << " entries");

        REQUIRE_FALSE(fs::exists(test_dir));
    }
    catch (std::exception const &e)
    {
        FAIL("Exception in Directory operations: " << e.what());
    }
}

TEST_CASE("File operations")
{
    fs::path const test_file = "fs_test_file.txt";

    try
    {
        if (fs::exists(test_file))
        {
            fs::remove(test_file);
        }

        MESSAGE("Writing to file: " << test_file.string());
        {
            std::ofstream ofs(test_file.string());
            ofs << "Hello, Filesystem!";
            ofs.close();
        }

        REQUIRE(fs::exists(test_file));
        REQUIRE(fs::is_regular_file(test_file));
        REQUIRE(fs::file_size(test_file) == 18);

        // Copy file is tricky if we don't have a reliable underlying copy.
        // We'll test rename which is simpler and implemented.
        fs::path const moved_file = "fs_test_file_moved.txt";
        if (fs::exists(moved_file))
        {
            fs::remove(moved_file);
        }

        MESSAGE("Renaming file");
        fs::rename(test_file, moved_file);
        REQUIRE_FALSE(fs::exists(test_file));
        REQUIRE(fs::exists(moved_file));

        MESSAGE("Cleaning up files");
        fs::remove(moved_file);
        REQUIRE_FALSE(fs::exists(moved_file));
    }
    catch (std::exception const &e)
    {
        FAIL("Exception in File operations: " << e.what());
    }
}

TEST_CASE("Directory iteration")
{
    fs::path const iter_dir = "iter_test";

    try
    {
        if (fs::exists(iter_dir))
        {
            remove_all_manual(iter_dir);
        }
        fs::create_directory(iter_dir);

        std::vector<fs::path> const test_files = {
            iter_dir / "f1.txt", iter_dir / "f2.txt", iter_dir / "f3.dat"};

        for (auto const &f : test_files)
        {
            std::ofstream ofs(f.string());
            ofs << "content";
            ofs.close();
        }

        fs::create_directory(iter_dir / "subdir");
        std::ofstream ofs_sub((iter_dir / "subdir" / "subf.txt").string());
        ofs_sub << "subcontent";
        ofs_sub.close();

        MESSAGE("Testing directory_iterator on " << iter_dir.string());
        int count = 0;
        for (auto const &entry : fs::directory_iterator(iter_dir))
        {
            MESSAGE("Found: " << entry.path().string());
            count++;
        }
        REQUIRE(count == 4);

        MESSAGE("Testing recursive_directory_iterator on "
                << iter_dir.string());
        int rec_count = 0;
        for (auto const &entry : fs::recursive_directory_iterator(iter_dir))
        {
            MESSAGE("Found (rec): " << entry.path().string());
            rec_count++;
        }
        REQUIRE(rec_count == 5);

        MESSAGE("Final cleanup");
        remove_all_manual(iter_dir);
        REQUIRE_FALSE(fs::exists(iter_dir));
    }
    catch (std::exception const &e)
    {
        FAIL("Exception in Directory iteration: " << e.what());
    }
}
