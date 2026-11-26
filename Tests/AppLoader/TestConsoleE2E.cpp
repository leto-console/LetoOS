/*
   E2E на настоящем эмуляторе: отдельным процессом запускается leto-console с
   --user 0 --game <фикстура>, игра стартует, сама закрывается, и процесс
   остаётся жив — то есть выход из игры не роняет консоль.

   Почему снаружи, а не в процессе теста: проверяется именно собранный
   эмулятор вместе с SDL. Состояние игры внутри .so тесту недоступно, поэтому
   факт запуска и выхода читается из журнала фикстуры ($LETO_TEST_LOG): файл
   переживает границу процесса.

   Без дисплея SDL не поднимется, поэтому обязателен SDL_VIDEODRIVER=dummy —
   тот же режим, что в CI. Каталог для запуска — временный: эмулятор пишет в
   cwd server.eeprom и server.img.
*/

#if defined(__unix__) || defined(__APPLE__)

#include <gtest/gtest.h>

#include <TestSupport.hpp>

#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef LETO_TEST_FIXTURE_DIR
#error "LETO_TEST_FIXTURE_DIR must point at the built fixture directory"
#endif

namespace
{
    /// Сколько ждать события от эмулятора; отладка на медленной машине — до 30 с
    constexpr int WaitMs = 30000;
    constexpr int PollMs = 50;

    /**
     * @brief Найти собранный эмулятор
     *
     * Явный путь из $LETO_TEST_CONSOLE важнее поиска: CI передаёт его сам.
     * Иначе перебираются каталоги Console/<preset>/bin — пресет эмулятора
     * (linux-st7735-debug) отличается от пресета LetoOS, поэтому подставить
     * LETO_TEST_PRESET_NAME нельзя.
     */
    std::filesystem::path FindConsole()
    {
        if (const char* explicit_path = getenv("LETO_TEST_CONSOLE"))
            if (*explicit_path && std::filesystem::exists(explicit_path))
                return explicit_path;

        const char* leto_path = getenv("LETO_PATH");
        if (!leto_path || !*leto_path) return {};

        std::error_code ec;
        const std::filesystem::path console_root = std::filesystem::path(leto_path) / "Console";
        for (const auto& preset : std::filesystem::directory_iterator(console_root, ec))
        {
            const std::filesystem::path candidate = preset.path() / "bin" / "leto-console";
            if (std::filesystem::exists(candidate)) return candidate;
        }

        return {};
    }

    /// Прочитать файл, если он есть; иначе пустая строка
    std::string ReadAll(const std::filesystem::path& path)
    {
        std::string text;
        for (const std::string& line : test::ReadLines(path))
        {
            text += line;
            text += '\n';
        }
        return text;
    }

    bool Alive(pid_t pid)
    {
        int status = 0;
        if (waitpid(pid, &status, WNOHANG) != 0) return false;
        return kill(pid, 0) == 0;
    }

    void Terminate(pid_t pid)
    {
        kill(pid, SIGTERM);

        for (int waited = 0; waited < 5000; waited += PollMs)
        {
            int status = 0;
            if (waitpid(pid, &status, WNOHANG) != 0) return;
            usleep(PollMs * 1000);
        }

        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
    }
}

class TestConsoleE2E : public ::testing::Test
{
protected:
    test::TempDir work{ "leto_console_e2e" };
    test::ScopedEnv video{ "SDL_VIDEODRIVER", "dummy" };
    test::ScopedEnv log{ "LETO_TEST_LOG", nullptr };
    test::ScopedEnv loop_limit{ "LETO_TEST_LOOP_LIMIT", nullptr };

    std::filesystem::path calls() const { return work.Path() / "calls.log"; }

    /**
     * @brief Заполнить путь к эмулятору
     *
     * $LETO_TEST_CONSOLE — контракт с CI: если он задан, а файла нет, это
     * ошибка конфигурации, а не повод молча пропустить тест, иначе джоба
     * останется зелёной вообще без E2E. Без переменной консоль ищется по
     * $LETO_PATH, и её отсутствие — законный пропуск: сборка LetoOS через
     * FetchContent эмулятор не собирает.
     *
     * @return false, если тест выполняться не должен. GTEST_SKIP() здесь не
     *         годится: он делает return только из этой функции, и тело теста
     *         продолжилось бы с пустым путём. Пропуск объявляет вызывающий.
     */
    bool RequireConsole(std::filesystem::path& console)
    {
        const char* explicit_path = getenv("LETO_TEST_CONSOLE");
        console = (explicit_path && *explicit_path) ? std::filesystem::path(explicit_path)
                                                    : FindConsole();

        return !console.empty() && std::filesystem::exists(console);
    }

    void SetUp() override
    {
        setenv("LETO_TEST_LOG", calls().c_str(), 1);
        // Игра закрывается после нескольких итераций: иначе сцена приложения
        // не вернулась бы в GameCenter и выход из игры было бы нечем наблюдать
        setenv("LETO_TEST_LOOP_LIMIT", "5", 1);
    }

    /// Запустить эмулятор на фикстуре в отдельном процессе
    pid_t Spawn(const std::filesystem::path& console, const std::filesystem::path& game)
    {
        const pid_t pid = fork();
        if (pid < 0) return -1;

        if (pid == 0)
        {
            // cwd — временный каталог: эмулятор пишет туда server.eeprom и
            // server.img, и в дерево репозитория они попадать не должны
            if (chdir(work.Path().c_str()) != 0) _exit(127);

            execl(console.c_str(), console.c_str(), "--user", "0",
                    "--game", game.c_str(), static_cast<char*>(nullptr));

            _exit(127);
        }

        return pid;
    }

    /**
     * @brief Дождаться события в журнале фикстуры
     *
     * Ждём именно терминальное событие, а не первое попавшееся: между init и
     * clean лежит несколько итераций Loop(), и проверка «успела ли игра
     * закрыться» имеет смысл только после clean.
     *
     * @return true, если событие появилось раньше, чем процесс умер
     */
    bool WaitFor(const char* event, pid_t pid, int timeout_ms = WaitMs)
    {
        const std::filesystem::path log_file = calls();

        for (int waited = 0; waited < timeout_ms; waited += PollMs)
        {
            if (ReadAll(log_file).find(event) != std::string::npos) return true;
            if (!Alive(pid)) return ReadAll(log_file).find(event) != std::string::npos;
            usleep(PollMs * 1000);
        }

        return false;
    }
};

/*
    --user 0 --game <фикстура> запускает игру сразу, без прохода меню: в
    журнале появляется init, и ни одного события меню между ними нет.
*/
TEST_F(TestConsoleE2E, LaunchesGameFromCommandLine)
{
    std::filesystem::path console;
    if (!RequireConsole(console))
    {
        if (getenv("LETO_TEST_CONSOLE"))
            FAIL() << "$LETO_TEST_CONSOLE указывает на несуществующий файл: " << console;

        GTEST_SKIP() << "leto-console не собран: $LETO_PATH/Console/*/bin/leto-console";
    }

    const std::filesystem::path game =
            std::filesystem::path(LETO_TEST_FIXTURE_DIR) / "libvalid_game.so";

    const pid_t pid = Spawn(console, game);
    ASSERT_GT(pid, 0);

    const bool started = WaitFor("init", pid);
    const std::string log_text = ReadAll(calls());
    Terminate(pid);

    EXPECT_TRUE(started) << "игра не стартовала, журнал:\n" << log_text;
    EXPECT_EQ(test::CountLines(calls(), "init"), 1u) << log_text;
}

/*
    Игра закрывается сама — эмулятор обрабатывает LETO_V1_CLOSE_STATUS, зовёт
    Clean и возвращается в GameCenter. Процесс при этом обязан остаться жив:
    выход из игры не должен ронять консоль.
*/
TEST_F(TestConsoleE2E, SurvivesGameExit)
{
    std::filesystem::path console;
    if (!RequireConsole(console))
    {
        if (getenv("LETO_TEST_CONSOLE"))
            FAIL() << "$LETO_TEST_CONSOLE указывает на несуществующий файл: " << console;

        GTEST_SKIP() << "leto-console не собран: $LETO_PATH/Console/*/bin/leto-console";
    }

    const std::filesystem::path game =
            std::filesystem::path(LETO_TEST_FIXTURE_DIR) / "libvalid_game.so";

    const pid_t pid = Spawn(console, game);
    ASSERT_GT(pid, 0);

    const bool finished = WaitFor("clean", pid);
    // Дать сцене вернуться в GameCenter и отработать хотя бы один Loop()
    usleep(500 * 1000);

    const bool alive = Alive(pid);
    const std::string log_text = ReadAll(calls());
    Terminate(pid);

    EXPECT_TRUE(finished) << "игра не закрылась, журнал:\n" << log_text;
    EXPECT_EQ(test::CountLines(calls(), "clean"), 1u) << log_text;
    EXPECT_TRUE(alive) << "эмулятор завершился после выхода из игры, журнал:\n" << log_text;
}

/*
    Битый файл в --game не валит эмулятор: путь идёт через тот же
    LoadApplication, что и выбор из меню, и обязан вернуть отказ.
*/
TEST_F(TestConsoleE2E, RejectsBrokenGameFromCommandLine)
{
    std::filesystem::path console;
    if (!RequireConsole(console))
    {
        if (getenv("LETO_TEST_CONSOLE"))
            FAIL() << "$LETO_TEST_CONSOLE указывает на несуществующий файл: " << console;

        GTEST_SKIP() << "leto-console не собран: $LETO_PATH/Console/*/bin/leto-console";
    }

    const std::filesystem::path broken =
            std::filesystem::path(LETO_TEST_FIXTURE_DIR) / "libbroken_file.so";

    const pid_t pid = Spawn(console, broken);
    ASSERT_GT(pid, 0);

    // Эмулятор обязан пережить отказ: init не появится, процесс останется жив
    usleep(1500 * 1000);
    const bool alive = Alive(pid);
    Terminate(pid);

    EXPECT_TRUE(alive) << "эмулятор упал на битом --game";
    EXPECT_EQ(test::CountLines(calls(), "init"), 0u);
}

#endif // defined(__unix__) || defined(__APPLE__)
