/**
 * @file Language.hpp
 * @date Oct 10, 2026
 * @author Rakhimov T.
 */

#ifndef INC_SYSTEM_LANGUAGE_HPP_
#define INC_SYSTEM_LANGUAGE_HPP_

#include "LetoOSExport.h"

#include <Data/IDataCell.hpp>
#include <cstdint>

#include <LetoAPI_V1/LetoAPI_V1.h>

/**
 * @brief Проинициализировать ячейку с текущем языком системы
 */
extern LETO_OS_EXPORT void InitSystemLanguageCell(IDataCell<LetoLanguage_V1>* cell);

/**
 * @brief Установить текущий язык системы
 * @param[in] language `true` - включен, `false` - выключен
 */
extern LETO_OS_EXPORT void SetSystemLanguage(LetoLanguage_V1 language);

/**
 * @brief Получить отладочный режим работы системы
 * @return `true` - включен, `false` - выключен
 */
extern LETO_OS_EXPORT LetoLanguage_V1 GetSystemLanguage();

#endif
