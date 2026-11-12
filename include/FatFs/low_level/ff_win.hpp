#ifndef FF_WIN_DEFINED
#define FF_WIN_DEFINED

#ifdef _WIN32

#include "LetoOSExport.h"

#include "ff.h"
#include <string>

// Инициализирует FatFS с авто-форматированием
extern LETO_OS_EXPORT FATFS* fatfs_image_initialize(const char* image_path = "disk.img");

#endif

#endif