/*
   100 циклов «запуск игры из GameCenter → выход → снова GameCenter» в одном
   процессе: Init ровно один раз на цикл, Clean ровно один раз на цикл, общая
   арена и RSS не растут.

   Сцены гоняются напрямую через SystemSceneManager, а не через эмулятор:
   запуск процесса на каждый цикл стоил бы минуты и ничего не добавил бы к
   проверке утечек — утечка видна только в одном долгоживущем процессе.

   Кнопки подаются событиями в обход клавиатуры, поэтому дисплей не нужен.
*/

#if defined(__unix__) || defined(__APPLE__)

#include <gtest/gtest.h>

#include <AppLoader/AppLoader.hpp>

#include <LetoAPI_V1_System/Make.hpp>
#include <System/SystemSceneManager.hpp>

#include <AppLoader/AppScene.hpp>
#include <GamesSupport/GameCenter.hpp>
#include <Input/ButtonEvent.hpp>
#include <Input/SystemInputID.hpp>

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
    /*
       Идентификаторы сцен проекта. Здесь они свои, а не SceneID из
       leto-console: тесты LetoOS не должны зависеть от точки входа, а
       SystemSceneManager только хранит builders по номеру.
    */
    constexpr uint32_t AppSceneID = 7;
    constexpr uint32_t GameCenterID = 8;

    AppEvent Button(uint16_t id)
    {
        AppEvent event{};
        event.source = AE_Button;
        event.id = id;
        event.data = ButtonEvent::PRESSED;
        return event;
    }
}

class TestGameCenterCycles : public ::testing::Test
{
protected:
    test::TempDir root{ "leto_cycles" };
    test::TempDir work{ "leto_cycles_cwd" };
    test::TempDir logs{ "leto_cycles_log" };
    test::ScopedCwd cwd{ work.Path() };
    test::ScopedEnv leto_path{ "LETO_PATH", nullptr };
    test::ScopedEnv log{ "LETO_TEST_LOG", nullptr };
    test::ScopedEnv loop_limit{ "LETO_TEST_LOOP_LIMIT", nullptr };

    std::filesystem::path calls() const { return logs.Path() / "calls.log"; }

    void SetUp() override
    {
        setenv("LETO_PATH", root.Path().c_str(), 1);
        setenv("LETO_TEST_LOG", calls().c_str(), 1);
        Make_LetoAPI_V1();

        test::MakeGameDir(root.Path(), LETO_TEST_PRESET_NAME, "ValidGame",
                std::filesystem::path(LETO_TEST_FIXTURE_DIR) / "libvalid_game.so");

        auto& manager = SystemSceneManager::Instance();
        manager.AddSceneBuilder<AppScene>(AppSceneID);
        manager.AddSceneBuilder<GameCenter>(GameCenterID, AppSceneID);
    }

    void TearDown() override
    {
        if (CurrentLoadedApp) UnloadApplication();
        SystemSceneManager::Instance().ClearScenes();
    }
};

/*
    Один проход: открыть GameCenter, нажать Enter, дать игре закрыться самой,
    вернуться в GameCenter.
*/
TEST_F(TestGameCenterCycles, HundredLaunchExitCyclesKeepMemoryFlat)
{
    auto& manager = SystemSceneManager::Instance();

    // Прогрев: первый проход поднимает общую арену до рабочего размера, поэтому
    // он не показателен для роста
    const auto run_cycle = [&manager](int cycle) {
        manager.SwitchScene(GameCenterID);
        manager.Loop();
        ASSERT_EQ(manager.GetCurrentSceneID(), GameCenterID) << "цикл " << cycle;

        ASSERT_TRUE(manager.ProcessEvent(Button(SYSTEM_BTN_ENTER))) << "цикл " << cycle;
        manager.Loop();
        ASSERT_EQ(manager.GetCurrentSceneID(), AppSceneID) << "цикл " << cycle;
        ASSERT_NE(CurrentLoadedApp, nullptr) << "цикл " << cycle;

        // Игра закрывается сама после LETO_TEST_LOOP_LIMIT итераций
        for (int loop = 0; loop < 10 && CurrentLoadedApp; ++loop)
            manager.Loop();

        ASSERT_EQ(CurrentLoadedApp, nullptr) << "игра не закрылась, цикл " << cycle;
        EXPECT_EQ(manager.GetCurrentSceneID(), GameCenterID) << "цикл " << cycle;
    };

    setenv("LETO_TEST_LOOP_LIMIT", "1", 1);
    run_cycle(0);

    auto& allocator = manager.GetCommonAllocator();
    const int arena_after_warmup = allocator.GetUsedBytes();
    const long rss_after_warmup = test::ResidentKb();

    constexpr int Cycles = 100;
    for (int cycle = 1; cycle <= Cycles; ++cycle)
        run_cycle(cycle);

    EXPECT_EQ(test::CountLines(calls(), "init"), static_cast<size_t>(Cycles) + 1);
    EXPECT_EQ(test::CountLines(calls(), "clean"), static_cast<size_t>(Cycles) + 1);

    EXPECT_EQ(allocator.GetUsedBytes(), arena_after_warmup)
            << "общая арена не вернулась к исходному размеру";

    const long rss_growth = test::ResidentKb() - rss_after_warmup;
    EXPECT_LT(rss_growth, 4096) << "RSS вырос на " << rss_growth << " КБ за "
            << Cycles << " циклов";
}

/*
    Возврат из GameCenter не должен выгружать игру: меню — не выход из неё.
*/
TEST_F(TestGameCenterCycles, ReturnFromGameCenterKeepsGameLoaded)
{
    auto& manager = SystemSceneManager::Instance();
    setenv("LETO_TEST_LOOP_LIMIT", "0", 1);

    manager.SwitchScene(GameCenterID);
    manager.Loop();
    ASSERT_TRUE(manager.ProcessEvent(Button(SYSTEM_BTN_ENTER)));
    manager.Loop();
    ASSERT_NE(CurrentLoadedApp, nullptr);

    // Игра не закрывается сама (лимит 0), поэтому сцена остаётся на месте
    for (int loop = 0; loop < 5; ++loop)
        manager.Loop();

    EXPECT_EQ(manager.GetCurrentSceneID(), AppSceneID);
    EXPECT_NE(CurrentLoadedApp, nullptr);
}

#endif // defined(__unix__) || defined(__APPLE__)
