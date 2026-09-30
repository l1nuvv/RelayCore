#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>

#include "scoped_temp_directory.h"

TEST_CASE("scoped temp directories are unique and exist")
{
    RelayCore::test::ScopedTempDirectory first;
    RelayCore::test::ScopedTempDirectory second;

    CHECK(first.path() != second.path());
    CHECK(std::filesystem::is_directory(first.path()));
    CHECK(std::filesystem::is_directory(second.path()));
}

TEST_CASE("scoped temp directory removes its owned tree")
{
    std::filesystem::path path;
    {
        RelayCore::test::ScopedTempDirectory temp;
        path = temp.path();
        std::filesystem::create_directory(temp.path() / "nested");
        std::ofstream out{temp.path() / "nested" / "fixture.txt"};
        REQUIRE(out.is_open());
        out << "fixture";
    }

    CHECK_FALSE(std::filesystem::exists(path));
}

TEST_CASE("scoped temp directory cleanup does not affect another fixture")
{
    RelayCore::test::ScopedTempDirectory second;
    const std::filesystem::path second_path = second.path();
    const std::filesystem::path second_file = second_path / "second.txt";
    std::ofstream second_out{second_file};
    REQUIRE(second_out.is_open());
    second_out << "second";
    second_out.close();

    std::filesystem::path first_path;
    {
        RelayCore::test::ScopedTempDirectory first;
        first_path = first.path();
        std::ofstream first_out{first.path() / "first.txt"};
        REQUIRE(first_out.is_open());
        first_out << "first";
    }

    CHECK_FALSE(std::filesystem::exists(first_path));
    CHECK(std::filesystem::exists(second_path));
    CHECK(std::filesystem::exists(second_file));
}
