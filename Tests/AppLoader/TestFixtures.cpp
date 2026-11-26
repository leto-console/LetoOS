/*
   Поведение загрузчика на каждой фикстуре: хорошая игра должна находиться и
   загружаться, а всё остальное — молча пропускаться. Каталоги строятся во
   временном месте, поэтому тесты не зависят от LETO_PATH машины разработчика и
   не требуют собранных игр.

   Текст сообщений в VirtualConsole не проверяется: у консоли нет доступа к
   списку сообщений снаружи, а добавлять его ради теста — менять API LetoOS.
   Проверяется наблюдаемое следствие: файл пропущен, Init не вызван.
*/

#if defined(__unix__) || defined(__APPLE__)

#include <gtest/gtest.h>

#include <AppLoader/AppLoader.hpp>

#include <LetoAPI_V1_System/Make.hpp>

#include <TestSupport.hpp>

#include <filesystem>
#include <string>

#ifndef LETO_TEST_FIXTURE_DIR
#error "LETO_TEST_FIXTURE_DIR must point at the built fixture directory"
#endif

#ifndef LETO_TEST_PRESET_NAME
#error "LETO_TEST_PRESET_NAME must name the preset the loader searches for"
#endif

namespace
{
    std::filesystem::path Fixture(const char* name)
    {
        return std::filesystem::path(LETO_TEST_FIXTURE_DIR) / name;
    }

    /// Единственная найденная игра; 0 — не нашли
    uint32_t ScanOne(AppInfo& info)
    {
        AppInfo apps[8]{};
        const uint32_t count = ScanApps(apps, 8);
        if (count) info = apps[0];
        return count;
    }
}

class TestFixtures : public ::testing::Test
{
protected:
    // Порядок объявления задаёт порядок создания: рабочий каталог нужен раньше,
    // чем ScopedCwd в него войдёт, и должен жить дольше него.
    test::TempDir root{ "leto_fixture" };
    test::TempDir work{ "leto_fixture_cwd" };
    test::ScopedCwd cwd{ work.Path() };
    test::ScopedEnv leto_path{ "LETO_PATH", nullptr };

    void SetUp() override
    {
        setenv("LETO_PATH", root.Path().c_str(), 1);
        Make_LetoAPI_V1();
    }

    /// Положить файл фикстуры в <root>/Apps/<preset>/<Game>/
    std::filesystem::path Place(const std::string& game, const char* fixture)
    {
        return test::MakeGameDir(root.Path(), LETO_TEST_PRESET_NAME, game, Fixture(fixture));
    }

    /// Файл пропускается и сканированием, и прямой загрузкой
    static void ExpectSkipped(const std::filesystem::path& file)
    {
        AppInfo apps[8]{};
        EXPECT_EQ(ScanApps(apps, 8), 0u) << file;

        EXPECT_FALSE(LoadApplication(file.c_str())) << file;
        EXPECT_EQ(CurrentLoadedApp, nullptr) << file;
    }
};

/*
    Корректная игра: находится сканированием с метаданными из заголовка и
    загружается, вызывая Init ровно один раз.
*/
TEST_F(TestFixtures, ValidGameIsFoundAndLoaded)
{
    const auto dir = Place("ValidGame", "libvalid_game.so");

    AppInfo info{};
    ASSERT_EQ(ScanOne(info), 1u);
    EXPECT_STREQ(info.en_name, "ValidGame");
    EXPECT_STREQ(info.ru_name, "Тестовая игра");
    EXPECT_EQ(info.api_version, 1u);
    EXPECT_EQ(info.id, 0x0101u);
    EXPECT_EQ(std::filesystem::path(info.path).parent_path(), dir);

    test::TempDir log_dir{ "leto_fixture_log" };
    test::ScopedEnv log{ "LETO_TEST_LOG", (log_dir.Path() / "calls.log").c_str() };

    ASSERT_TRUE(LoadApplication((dir / "libvalid_game.so").c_str()));
    ASSERT_NE(CurrentLoadedApp, nullptr);
    EXPECT_STREQ(CurrentLoadedAppInfo.en_name, "ValidGame");
    EXPECT_EQ(CurrentLoadedAppInfo.id, 0x0101u);
    EXPECT_EQ(test::CountLines(log_dir.Path() / "calls.log", "init"), 1u);

    UnloadApplication();
    EXPECT_EQ(CurrentLoadedApp, nullptr);
    EXPECT_EQ(test::CountLines(log_dir.Path() / "calls.log", "clean"), 1u);
}

TEST_F(TestFixtures, NoSymbolIsSkipped)
{
    const auto dir = Place("NoSymbol", "libno_symbol.so");
    ExpectSkipped(dir / "libno_symbol.so");
}

TEST_F(TestFixtures, WrongMagicIsSkipped)
{
    const auto dir = Place("WrongMagic", "libwrong_magic.so");
    ExpectSkipped(dir / "libwrong_magic.so");
}

TEST_F(TestFixtures, BrokenFileIsSkipped)
{
    const auto dir = Place("BrokenFile", "libbroken_file.so");
    ExpectSkipped(dir / "libbroken_file.so");
}

TEST_F(TestFixtures, EmptyFileIsSkipped)
{
    const auto dir = Place("EmptyFile", "libempty_file.so");
    ExpectSkipped(dir / "libempty_file.so");
}

/*
    Контракт #92-2: библиотека собрана с -fvisibility=hidden, и точку входа
    видно только через экспортный макрос. Если макрос сломают, здесь будет 0.
*/
TEST_F(TestFixtures, HiddenVisibilityGameIsLoaded)
{
    const auto dir = Place("HiddenGame", "libhidden_visibility.so");

    AppInfo info{};
    ASSERT_EQ(ScanOne(info), 1u);
    EXPECT_STREQ(info.en_name, "HiddenGame");
    EXPECT_EQ(info.id, 0x0103u);

    test::TempDir log_dir{ "leto_fixture_log" };
    test::ScopedEnv log{ "LETO_TEST_LOG", (log_dir.Path() / "calls.log").c_str() };

    ASSERT_TRUE(LoadApplication((dir / "libhidden_visibility.so").c_str()));
    EXPECT_EQ(test::CountLines(log_dir.Path() / "calls.log", "init"), 1u);

    UnloadApplication();
    EXPECT_EQ(CurrentLoadedApp, nullptr);
}

/*
    Один повреждённый файл не имеет права ломать весь список: сканирование идёт
    раз в секунду, и хорошая игра должна находиться и среди битых соседей.
*/
TEST_F(TestFixtures, ValidGameSurvivesBrokenNeighbours)
{
    const auto dir = Place("ValidGame", "libvalid_game.so");
    test::WriteFile(dir / "libempty.so", "");
    test::WriteFile(dir / "libtext.so", "not an elf\n");
    test::WriteFile(dir / "libtruncated.so", std::string("\x7f" "ELF\x02\x01\x01", 7));

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "ValidGame");
}

/*
    Повторное сканирование даёт тот же результат: причина отказа пишется один
    раз на переход состояния, а не каждый проход.
*/
TEST_F(TestFixtures, RescansAreStable)
{
    Place("ValidGame", "libvalid_game.so");

    AppInfo apps[8]{};
    for (int pass = 0; pass < 3; ++pass)
        EXPECT_EQ(ScanApps(apps, 8), 1u);
}

TEST_F(TestFixtures, RespectsAvailableSpace)
{
    Place("ValidGame", "libvalid_game.so");

    AppInfo apps[1]{};
    EXPECT_EQ(ScanApps(apps, 0), 0u);
    EXPECT_LE(ScanApps(apps, 1), 1u);
}

TEST_F(TestFixtures, CheckGameRejectsNonGames)
{
    AppInfo info{};

    EXPECT_TRUE(CheckGame(Fixture("libvalid_game.so").c_str(), info));
    EXPECT_STREQ(info.en_name, "ValidGame");

    EXPECT_FALSE(CheckGame(Fixture("libno_symbol.so").c_str(), info));
    EXPECT_FALSE(CheckGame(Fixture("libwrong_magic.so").c_str(), info));
    EXPECT_FALSE(CheckGame(Fixture("libbroken_file.so").c_str(), info));
    EXPECT_FALSE(CheckGame(Fixture("libempty_file.so").c_str(), info));
}

#endif // defined(__unix__) || defined(__APPLE__)
