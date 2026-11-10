
#pragma once

#ifdef LETO_OS_EXPORT_LIBRARY
#include <LetoAPI_V1/LetoAPI_V1_Version.h>
#define LETO_API_V1_USE LETO_API_V1_MINOR
#endif

#if defined(_WIN32) && defined(LETO_OS_SHARED)
    #ifdef LETO_OS_EXPORT_LIBRARY
        #define LETO_OS_EXPORT __declspec(dllexport)
    #else
        #define LETO_OS_EXPORT __declspec(dllimport)
    #endif
#else
    #define LETO_OS_EXPORT
#endif
