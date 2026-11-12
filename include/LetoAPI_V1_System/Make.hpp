/**
 * @file Init_V1.hpp
 * @date May 23, 2026
 * @author Rakhimov T.
 */

#ifndef INC_LETO_API_V1_MAKE_HPP_
#define INC_LETO_API_V1_MAKE_HPP_

#include "LetoOSExport.h"

#include <LetoAPI_V1/LetoAPI_V1.h>

/**
 * @brief Create and initialize system API
 * 
 * System API object creation is allowed only within the system itself
 * 
 * @warning Calling this function in application causes UB
 */
extern LETO_OS_EXPORT const LetoAPI_V1* Make_LetoAPI_V1();

struct LETO_OS_EXPORT LetoAPI_V1_StaticMaker
{
    LetoAPI_V1_StaticMaker();
};

static LetoAPI_V1_StaticMaker __LetoAPI_V1_Static_Maker__;

#endif
