/*
   Фикстура «магия не та»: символ `__leto_exec_info__` есть и экспортирован,
   но type — не LETO_EXEC_GAME. Загрузчик обязан пропустить файл, не вызывая
   Init: иначе в список игр попал бы чужой бинарник.
*/

#include <LetoABI/AppBinHeader.h>

extern "C" __attribute__((visibility("default")))
const AppBinHeader LETO_EXECUTABLE_INFO_VAR_NAME = {
        "SOMETHING_ELSE",
        1,
        0x0102,
        "WrongMagic",
        "Не игра",
};
