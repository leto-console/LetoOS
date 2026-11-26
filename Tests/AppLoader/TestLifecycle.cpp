/*
   Жизненный цикл загрузчика: dlopen → dlsym(__leto_exec_info__) → Init →
   Clean → dlclose. Проверяется то, что видно снаружи: указатель на текущее
   приложение, журнал вызовов игры и фактическая выгрузка библиотеки
   (RTLD_NOLOAD после выгрузки обязан вернуть nullptr).

   Отдельно проверяется откат общей арены: игра берёт из неё память в Init и не
   возвращает её в Clean, поэтому без отката по маркеру арена кончается за
   считанные циклы. Тест на 100 циклов ловит именно это.
*/

#if defined(__unix__) || defined(__APPLE__)

#include <gtest/gtest.h>

#include <AppLoader/AppLoader.hpp>

#include <LetoAPI_V1/Application/LetoApplication_V1.h>

#include <LetoAPI_V1_System/Make.hpp>
#include <System/SystemSceneManager.hpp>

#include <TestSupport.hpp>

#include <dlfcn.h>

#include <filesystem>
#include <string>

#ifndef LETO_TEST_FIXTURE_DIR
#error "LETO_TEST_FIXTURE_DIR must point at the built fixture directory"
#endif

namespace
{
    std::filesystem::path Fixture(const char* name)
    {
        return std::filesystem::path(LETO_TEST_FIXTURE_DIR) / name;
    }

    /// Библиотека всё ещё замаплена в процесс
    bool IsMapped(const std::filesystem::path& file)
    {
        void* handle = dlopen(file.c_str(), RTLD_NOLOAD | RTLD_NOW);
        if (!handle) return false;

        dlclose(handle);
        return true;
    }
}

class TestLifecycle : public ::testing::Test
{
protected:
    test::TempDir work{ "leto_lifecycle" };
    test::TempDir logs{ "leto_lifecycle_log" };
    test::ScopedCwd cwd{ work.Path() };
    test::ScopedEnv leto_path{ "LETO_PATH", nullptr };
    test::ScopedEnv log{ "LETO_TEST_LOG", nullptr };
    test::ScopedEnv loop_limit{ "LETO_TEST_LOOP_LIMIT", nullptr };

    std::filesystem::path calls() const { return logs.Path() / "calls.log"; }

    void SetUp() override
    {
        setenv("LETO_PATH", "", 1);
        setenv("LETO_TEST_LOG", calls().c_str(), 1);
        Make_LetoAPI_V1();
    }

    void TearDown() override
    {
        // Тест мог упасть до выгрузки: следующий начнёт с чистого загрузчика
        if (CurrentLoadedApp) UnloadApplication();
    }

    static void ExpectInitAndCleanOnce(const std::filesystem::path& log)
    {
        EXPECT_EQ(test::CountLines(log, "init"), 1u);
        EXPECT_EQ(test::CountLines(log, "clean"), 1u);
    }
};

/*
    Загрузка игры: заголовок скопирован, путь канонизирован, Init вызван.
*/
TEST_F(TestLifecycle, LoadCallsInit)
{
    const auto game = Fixture("libvalid_game.so");

    ASSERT_TRUE(LoadApplication(game.c_str()));
    ASSERT_NE(CurrentLoadedApp, nullptr);

    EXPECT_STREQ(CurrentLoadedAppInfo.en_name, "ValidGame");
    EXPECT_EQ(CurrentLoadedAppInfo.api_version, 1u);
    EXPECT_EQ(CurrentLoadedAppInfo.id, 0x0101u);
    EXPECT_TRUE(std::filesystem::path(CurrentLoadedAppInfo.path).is_absolute());
    EXPECT_TRUE(IsMapped(game));

    EXPECT_EQ(test::CountLines(calls(), "init"), 1u);
    EXPECT_EQ(test::CountLines(calls(), "clean"), 0u);
}

/*
    Выгрузка: Clean ровно один раз, указатель сброшен до dlclose, библиотека
    действительно ушла из процесса. Последнее — не формальность: без
    -fno-gnu-unique glibc держит DSO замапленным навсегда.
*/
TEST_F(TestLifecycle, UnloadCallsCleanOnceAndUnmaps)
{
    const auto game = Fixture("libvalid_game.so");

    ASSERT_TRUE(LoadApplication(game.c_str()));
    UnloadApplication();

    EXPECT_EQ(CurrentLoadedApp, nullptr);
    ExpectInitAndCleanOnce(calls());
    EXPECT_FALSE(IsMapped(game));
}

/*
    Повторная выгрузка без загрузки — не ошибка и не второй Clean: на выходе из
    сцены и по двойному нажатию меню загрузчик зовут безусловно.
*/
TEST_F(TestLifecycle, RepeatedUnloadIsNoOp)
{
    const auto game = Fixture("libvalid_game.so");

    ASSERT_TRUE(LoadApplication(game.c_str()));
    UnloadApplication();
    UnloadApplication();
    UnloadApplication();

    ExpectInitAndCleanOnce(calls());
}

/*
    Повторная загрузка после выгрузки: игра запускается с нуля, а не получает
    старый хендл. Init второй раз — доказательство, что библиотека грузилась
    заново.
*/
TEST_F(TestLifecycle, ReloadAfterUnloadStartsFromScratch)
{
    const auto game = Fixture("libvalid_game.so");

    ASSERT_TRUE(LoadApplication(game.c_str()));
    UnloadApplication();

    ASSERT_TRUE(LoadApplication(game.c_str()));
    ASSERT_NE(CurrentLoadedApp, nullptr);
    EXPECT_TRUE(IsMapped(game));
    EXPECT_EQ(test::CountLines(calls(), "init"), 2u);

    UnloadApplication();
    EXPECT_EQ(test::CountLines(calls(), "clean"), 2u);
}

/*
    Второй LoadApplication без выгрузки отклоняется: две игры одновременно не
    живут, иначе общая арена смешала бы их объекты.
*/
TEST_F(TestLifecycle, SecondLoadIsRejected)
{
    const auto game = Fixture("libvalid_game.so");

    ASSERT_TRUE(LoadApplication(game.c_str()));
    EXPECT_FALSE(LoadApplication(game.c_str()));

    EXPECT_EQ(test::CountLines(calls(), "init"), 1u);

    UnloadApplication();
    ExpectInitAndCleanOnce(calls());
}

TEST_F(TestLifecycle, RejectsBadPaths)
{
    EXPECT_FALSE(LoadApplication(nullptr));
    EXPECT_FALSE(LoadApplication(""));
    EXPECT_FALSE(LoadApplication((work.Path() / "no-such-game.so").c_str()));
    EXPECT_EQ(CurrentLoadedApp, nullptr);
    EXPECT_EQ(test::CountLines(calls(), "init"), 0u);
}

/*
    Путь длиннее, чем позволяет AppPath: полный путь сохраняется целиком.
*/
TEST_F(TestLifecycle, KeepsLongAbsolutePath)
{
    const std::string deep(120, 'd');
    const auto dir = work.Path() / deep / "Games" / "LongNamedGame";
    std::filesystem::create_directories(dir);
    std::filesystem::copy_file(Fixture("libvalid_game.so"), dir / "libvalid_game.so");

    const auto game = dir / "libvalid_game.so";
    ASSERT_GT(game.string().size(), 128u);

    ASSERT_TRUE(LoadApplication(game.c_str()));
    EXPECT_EQ(std::filesystem::path(CurrentLoadedAppInfo.path), game);

    UnloadApplication();
}

/*
    100 циклов «загрузка/выгрузка»: Init и Clean по одному разу на цикл, общая
    арена возвращается к исходному размеру, RSS не растёт.

    Игра намеренно не освобождает память в Clean, поэтому рост арены означал бы
    отсутствующий откат по маркеру, а рост RSS — отсутствующий dlclose.
*/
TEST_F(TestLifecycle, HundredCyclesKeepArenaAndRssFlat)
{
    const auto game = Fixture("libvalid_game.so");

    auto& allocator = SystemSceneManager::Instance().GetCommonAllocator();

    // Прогрев: первый цикл поднимает арену до рабочего размера и грузит
    // зависимости библиотеки, поэтому он не показателен
    ASSERT_TRUE(LoadApplication(game.c_str()));
    UnloadApplication();

    const int arena_after_warmup = allocator.GetUsedBytes();
    const long rss_after_warmup = test::ResidentKb();

    constexpr int Cycles = 100;
    for (int cycle = 0; cycle < Cycles; ++cycle)
    {
        ASSERT_TRUE(LoadApplication(game.c_str())) << "цикл " << cycle;
        UnloadApplication();
        ASSERT_EQ(CurrentLoadedApp, nullptr) << "цикл " << cycle;
    }

    EXPECT_EQ(test::CountLines(calls(), "init"), static_cast<size_t>(Cycles) + 1);
    EXPECT_EQ(test::CountLines(calls(), "clean"), static_cast<size_t>(Cycles) + 1);

    EXPECT_EQ(allocator.GetUsedBytes(), arena_after_warmup)
            << "общая арена не вернулась к исходному размеру";

    const long rss_growth = test::ResidentKb() - rss_after_warmup;
    EXPECT_LT(rss_growth, 4096) << "RSS вырос на " << rss_growth << " КБ за "
            << Cycles << " циклов";
}

/*
    GetBinary не оставляет библиотеку загруженной: заголовок читается и хендл
    сразу закрывается. Иначе сканирование раз в секунду накопило бы все игры в
    памяти процесса.
*/
TEST_F(TestLifecycle, GetBinaryDoesNotKeepLibraryMapped)
{
    const auto game = Fixture("libvalid_game.so");

    AppBinHeader header{};
    ASSERT_TRUE(GetBinary(game.c_str(), &header, sizeof(header)));
    EXPECT_STREQ(header.en_name, "ValidGame");
    EXPECT_FALSE(IsMapped(game));
}

/*
    GetBinary(load_in_memory) — путь Windows/STM32; на Unix он обязан сохранить
    хендл до UnloadApplication и не звать Init.
*/
TEST_F(TestLifecycle, GetBinaryCanKeepLibraryMapped)
{
    const auto game = Fixture("libvalid_game.so");

    LetoApplication_V1 app{};
    ASSERT_TRUE(GetBinary(game.c_str(), &app, sizeof(app), true));
    EXPECT_TRUE(IsMapped(game));
    EXPECT_EQ(test::CountLines(calls(), "init"), 0u);

    UnloadApplication();
    EXPECT_FALSE(IsMapped(game));
}

#endif // defined(__unix__) || defined(__APPLE__)
