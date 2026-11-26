/*
   Каскад поиска каталога игр: <cwd>/Apps/<preset> → <cwd>/Apps →
   $LETO_PATH/Apps/<preset> → $LETO_PATH/Apps. Первый найденный выигрывает,
   иначе одна и та же игра попала бы в список дважды.

   Резолвер живёт в анонимном namespace AppLoader.cpp, поэтому проверяется через
   единственную наблюдаемую точку — ScanApps. Игры во всех каталогах разные по
   id и имени, так что по результату видно, какой шаг каскада выиграл.
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
    const char* const ValidGame = "libvalid_game.so";
    const char* const HiddenGame = "libhidden_visibility.so";
}

class TestResolveAppsDir : public ::testing::Test
{
protected:
    test::TempDir cwd_root{ "leto_resolve_cwd" };
    test::TempDir env_root{ "leto_resolve_env" };
    test::ScopedCwd cwd{ cwd_root.Path() };
    test::ScopedEnv leto_path{ "LETO_PATH", nullptr };

    void SetUp() override { Make_LetoAPI_V1(); }

    /// Игра в <root>/Apps/[<sub>/]<Game>/
    std::filesystem::path Place(const std::filesystem::path& root,
            const std::string& sub, const std::string& game, const char* fixture)
    {
        const std::filesystem::path dir =
                sub.empty() ? root / "Apps" / game : root / "Apps" / sub / game;
        std::filesystem::create_directories(dir);
        std::filesystem::copy_file(std::filesystem::path(LETO_TEST_FIXTURE_DIR) / fixture,
                dir / fixture, std::filesystem::copy_options::overwrite_existing);
        return dir;
    }
};

/*
    Каталог текущего пресета важнее общего Apps/: в Apps/ может лежать сборка
    другого пресета, и она не должна подменять игры текущей.
*/
TEST_F(TestResolveAppsDir, PresetInCwdWinsOverPlainCwd)
{
    Place(env_root.Path(), LETO_TEST_PRESET_NAME, "FromPreset", ValidGame);
    Place(env_root.Path(), "", "FromPlain", HiddenGame);

    setenv("LETO_PATH", env_root.Path().c_str(), 1);

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "ValidGame");
}

/*
    cwd важнее $LETO_PATH: загрузчик стоит рядом с играми своей сборки, и
    переменная окружения — только запасной путь.
*/
TEST_F(TestResolveAppsDir, CwdWinsOverLetoPath)
{
    Place(cwd_root.Path(), LETO_TEST_PRESET_NAME, "FromCwd", ValidGame);
    Place(env_root.Path(), LETO_TEST_PRESET_NAME, "FromEnv", HiddenGame);

    setenv("LETO_PATH", env_root.Path().c_str(), 1);

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "ValidGame");
}

/*
    Пресет в $LETO_PATH важнее общего Apps/ в $LETO_PATH.
*/
TEST_F(TestResolveAppsDir, PresetInLetoPathWinsOverPlainLetoPath)
{
    Place(env_root.Path(), LETO_TEST_PRESET_NAME, "FromPreset", ValidGame);
    Place(env_root.Path(), "", "FromPlain", HiddenGame);

    setenv("LETO_PATH", env_root.Path().c_str(), 1);

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "ValidGame");
}

/*
    Общий Apps/ в $LETO_PATH — последний шаг каскада; он работает, когда cwd
    игр не содержит.
*/
TEST_F(TestResolveAppsDir, PlainLetoPathIsTheLastStep)
{
    Place(env_root.Path(), "", "FromPlain", HiddenGame);

    setenv("LETO_PATH", env_root.Path().c_str(), 1);

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "HiddenGame");
}

/*
    $LETO_PATH без завершающего слэша — норма: склейка через operator/ не
    зависит от того, как переменную выставили. Проверка парная, чтобы отличить
    «без слэша» от «слэш съелся и путь неверный».
*/
TEST_F(TestResolveAppsDir, LetoPathWithoutTrailingSlash)
{
    Place(env_root.Path(), LETO_TEST_PRESET_NAME, "FromEnv", ValidGame);

    setenv("LETO_PATH", env_root.Path().c_str(), 1);
    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u) << "без слэша";

    const std::string with_slash = env_root.Path().string() + "/";
    setenv("LETO_PATH", with_slash.c_str(), 1);
    AppInfo again[8]{};
    ASSERT_EQ(ScanApps(again, 8), 1u) << "со слэшем";
    EXPECT_STREQ(again[0].en_name, "ValidGame");
}

/*
    Несуществующая директория — не ошибка, шаг каскада просто пропускается.
*/
TEST_F(TestResolveAppsDir, MissingDirectoryIsSkipped)
{
    setenv("LETO_PATH", (env_root.Path() / "no-such-dir").c_str(), 1);

    AppInfo apps[8]{};
    EXPECT_EQ(ScanApps(apps, 8), 0u);
}

/*
    Пустой $LETO_PATH не должен превращаться в корень файловой системы: путь
    "Apps" относительно пустой строки дал бы "/Apps".
*/
TEST_F(TestResolveAppsDir, EmptyLetoPathIsIgnored)
{
    Place(cwd_root.Path(), LETO_TEST_PRESET_NAME, "FromCwd", ValidGame);
    setenv("LETO_PATH", "", 1);

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "ValidGame");
}

/*
    Совсем без $LETO_PATH загрузчик обязан работать: cwd — самостоятельный корень.
*/
TEST_F(TestResolveAppsDir, NoLetoPathAtAll)
{
    Place(cwd_root.Path(), "", "FromCwd", ValidGame);
    unsetenv("LETO_PATH");

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "ValidGame");
}

/*
    Каталог пресета в cwd выигрывает у общего Apps/ в $LETO_PATH: шаги идут по
    корням, и внутри корня конкретное раньше общего.
*/
TEST_F(TestResolveAppsDir, CwdPresetWinsOverEnvPlain)
{
    Place(cwd_root.Path(), LETO_TEST_PRESET_NAME, "FromCwd", HiddenGame);
    Place(env_root.Path(), "", "FromEnv", ValidGame);

    setenv("LETO_PATH", env_root.Path().c_str(), 1);

    AppInfo apps[8]{};
    ASSERT_EQ(ScanApps(apps, 8), 1u);
    EXPECT_STREQ(apps[0].en_name, "HiddenGame");
}

#endif // defined(__unix__) || defined(__APPLE__)
