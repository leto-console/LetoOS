/*
   Игра для тестов загрузчика: заголовок, Init/Clean/Loop и журнал вызовов в
   файл из $LETO_TEST_LOG. Журнал читают и юнит-тесты, и E2E-тест: состояние
   внутри .so тесту недоступно, а файл переживает границу процесса.

   Игра закрывается сама после $LETO_TEST_LOOP_LIMIT итераций Loop(): иначе
   сцена приложения не вернулась бы в GameCenter, и цикл «запуск/выход» нечем
   было бы завершить.

   Init берёт память из общей арены хоста и не возвращает её в Clean — так же,
   как настоящие игры: освобождает загрузчик откатом по маркеру. Если откат
   сломан, арена кончается за считанные циклы, и в журнале появляется
   init_no_memory.

   Один и тот же исходник собирается дважды: обычная библиотека и библиотека с
   -fvisibility=hidden. Во второй точке входа видна только благодаря
   LETO_API_EXPORT из LetoAPI — это и есть проверка контракта #92-2.
*/

#include <LetoAPI_V1/LetoAPI_V1_Export.h>

#include <LetoABI/AppBinHeader.h>
#include <LetoABI/AppEvent.h>
#include <LetoAPI_V1/Application/LetoApplication_V1.h>

#include <cstdio>
#include <cstdlib>

#ifndef LETO_TEST_FIXTURE_ID
#define LETO_TEST_FIXTURE_ID 0x0101
#endif

#ifndef LETO_TEST_FIXTURE_NAME
#define LETO_TEST_FIXTURE_NAME "ValidGame"
#endif

#ifndef LETO_TEST_FIXTURE_RU_NAME
#define LETO_TEST_FIXTURE_RU_NAME "Тестовая игра"
#endif

namespace
{
    /// Сколько байт просить у общей арены на каждую загрузку
    constexpr uint32_t ArenaBlockSize = 2048;

    void Log(const char* event)
    {
        const char* path = getenv("LETO_TEST_LOG");
        if (!path || !*path) return;

        FILE* file = fopen(path, "a");
        if (!file) return;

        fprintf(file, "%s\n", event);
        fclose(file);
    }

    /// Сколько итераций Loop() выполнить до CLOSE_STATUS; 0 — не закрываться
    int LoopLimit()
    {
        const char* limit = getenv("LETO_TEST_LOOP_LIMIT");
        return limit ? atoi(limit) : 0;
    }

    int loops;

    LetoResult_V1 Init(const LetoAPI_V1* api)
    {
        if (!api || !api->Globals || !api->Globals->GetAllocator)
        {
            Log("init_no_api");
            return LETO_V1_ARGS_ERROR;
        }

        if (!api->Globals->GetAllocator()->Alloc(ArenaBlockSize))
        {
            Log("init_no_memory");
            return LETO_V1_OUT_OF_MEMORY;
        }

        loops = 0;
        Log("init");
        return LETO_V1_SUCCESS;
    }

    LetoResult_V1 Clean()
    {
        Log("clean");
        return LETO_V1_SUCCESS;
    }

    LetoAppStatus_V1 GetStatus()
    {
        const int limit = LoopLimit();
        return (limit > 0 && loops >= limit) ? LETO_V1_CLOSE_STATUS : LETO_V1_WORK_STATUS;
    }

    LetoResult_V1 ProcessEvent(AppEvent)
    {
        return LETO_V1_SUCCESS;
    }

    LetoResult_V1 Draw(LetoScreen_V1*)
    {
        return LETO_V1_SUCCESS;
    }

    LetoResult_V1 Loop()
    {
        loops++;
        Log("loop");
        return LETO_V1_SUCCESS;
    }

    LetoApplication_V1 application = {
            { LETO_EXECUTABLE_TYPE_GAME, 1, LETO_TEST_FIXTURE_ID,
                    LETO_TEST_FIXTURE_NAME, LETO_TEST_FIXTURE_RU_NAME },
            &Init,
            &Clean,
            &GetStatus,
            &ProcessEvent,
            &Draw,
            &Loop,
    };
}

extern "C" LETO_API_EXPORT
LetoApplication_V1 LETO_EXECUTABLE_INFO_VAR_NAME = application;
