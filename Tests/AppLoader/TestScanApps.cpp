/*
    Тест про Unix-загрузчик: сканирование ищет *.so и открывает файл через
    dlopen. Под Windows каталог сканируется по *.dll и открывается через
    LoadLibrary, поэтому здесь нечего проверять — и фикстура для него не
    собирается (см. Tests/CMakeLists.txt).
*/
#if defined(__unix__) || defined(__APPLE__)

#include <gtest/gtest.h>

#include <AppLoader/AppLoader.hpp>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#ifndef LETO_TEST_FIXTURE_DIR
#error "LETO_TEST_FIXTURE_DIR must point at the built fixture directory"
#endif

/*
    Каталог Apps сканируется раз в секунду, поэтому битый, пустой и не-игровой
    файл обязан пропускаться без падения: иначе одна повреждённая библиотека
    ломает весь список игр. Настоящая игра при этом должна находиться — иначе
    тест прошёл бы и на полностью неработающем сканировании.
*/
class TestScanApps : public ::testing::Test
{
protected:
    std::filesystem::path root;
    std::filesystem::path game_dir;

    void SetUp() override
    {
        root = std::filesystem::temp_directory_path()
                / ("leto_scan_" + std::to_string(::getpid()));
        std::filesystem::remove_all(root);

        game_dir = root / "Apps" / "linux-debug" / "Broken";
        std::filesystem::create_directories(game_dir);

        Write(game_dir / "libEmpty.so", "");
        Write(game_dir / "libText.so", "not an elf\n");
        Write(game_dir / "libTruncated.so", std::string("\x7f" "ELF\x02\x01\x01", 7));

        setenv("LETO_PATH", root.c_str(), 1);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(root);
        unsetenv("LETO_PATH");
    }

    static void Write(const std::filesystem::path& path, const std::string& data)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    void AddRealGame() const
    {
        std::filesystem::copy_file(
                std::filesystem::path(LETO_TEST_FIXTURE_DIR) / "libTestGameFixture.so",
                game_dir / "libTestGameFixture.so");
    }
};

TEST_F(TestScanApps, FindsRealGameNextToBrokenFiles)
{
    AddRealGame();

    AppInfo apps[8]{};
    const uint32_t count = ScanApps(apps, 8);

    ASSERT_EQ(count, 1u);
    EXPECT_STREQ(apps[0].en_name, "ScanFixture");
    EXPECT_EQ(apps[0].api_version, 1u);
    EXPECT_EQ(apps[0].id, 7u);
    EXPECT_NE(std::string(apps[0].path).find("libTestGameFixture.so"), std::string::npos);
}

TEST_F(TestScanApps, SkipsBrokenLibrariesWithoutRealGame)
{
    AppInfo apps[8]{};

    EXPECT_EQ(ScanApps(apps, 8), 0u);
}

/*
    Повторное сканирование не должно менять результат: причина отказа пишется
    один раз на переход состояния, а не каждый проход.
*/
TEST_F(TestScanApps, RescansAreStable)
{
    AddRealGame();

    AppInfo apps[8]{};
    for (int pass = 0; pass < 3; ++pass)
        EXPECT_EQ(ScanApps(apps, 8), 1u);
}

TEST_F(TestScanApps, RespectsAvailableSpace)
{
    AddRealGame();

    AppInfo apps[1]{};

    EXPECT_EQ(ScanApps(apps, 0), 0u);
    EXPECT_LE(ScanApps(apps, 1), 1u);
}

#endif // defined(__unix__) || defined(__APPLE__)
