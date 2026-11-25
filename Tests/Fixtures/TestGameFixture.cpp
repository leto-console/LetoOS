/*
   Фикстура для тестов загрузчика: разделяемая библиотека с заголовком игры,
   но без реализации LetoAPI. Тестам нужен только `__leto_exec_info__`, поэтому
   Init/Clean/Loop здесь не нужны — они проверяются в E2E, а не в юнит-тестах.
*/

#include <LetoABI/AppBinHeader.h>

extern "C" __attribute__((visibility("default")))
const AppBinHeader LETO_EXECUTABLE_INFO_VAR_NAME = {
        LETO_EXECUTABLE_TYPE_GAME,
        1,
        7,
        "ScanFixture",
        "Фикстура",
};
