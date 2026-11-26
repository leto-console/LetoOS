#include "AppLoader.hpp"

#include <LetoAPI_V1/Application/LetoApplication_V1.h>
#include <LetoAPI_V1_System/Make.hpp>
#include <VirtualConsole/VirtualConsole.hpp>

// ======================================================================
//                    Независимый от платформы код
// ======================================================================

AppBinHeader* CurrentLoadedApp = nullptr;
AppInfo CurrentLoadedAppInfo = {};

bool CheckGame(const char *path, AppInfo& info)
{
    AppBinHeader bin_info{};

    if (!GetBinary(path, &bin_info, sizeof(AppBinHeader))) return false;

    info.FromBinary(bin_info);
    snprintf(info.path, sizeof(info.path), "%s", path);

    return true;
}

// ======================================================================

// ======================================================================
//                        Реализация для Windows
// ======================================================================

#ifdef _WIN32

#include <Windows.h>
#include <string>

static HMODULE hLoadedDll;

uint32_t ScanApps(AppInfo* array, uint32_t available)
{
    uint32_t count = 0;
    std::string base_dir;

#ifdef LETOCORE_SHARED
    const char* env_path = getenv("LETO_PATH");
    if (!env_path) return 0;
    base_dir = std::string(env_path) + "Apps/win-debug/";
#else
    base_dir = "Apps/";
#endif

    // 1. Ищем все подпапки в base_dir
    std::string dir_mask = base_dir + "*";
    WIN32_FIND_DATAA dir_data;
    HANDLE hDir = FindFirstFileA(dir_mask.c_str(), &dir_data);
    
    if (hDir == INVALID_HANDLE_VALUE) return 0;

    do 
    {
        // Пропускаем системные ссылки . и ..
        if (strcmp(dir_data.cFileName, ".") == 0 || 
            strcmp(dir_data.cFileName, "..") == 0)
            continue;
        
        // Нас интересуют только директории (папки с играми)
        if (!(dir_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            continue;

        // 2. Внутри каждой папки ищем *.dll
        std::string file_mask = base_dir + dir_data.cFileName + "/*.dll";
        WIN32_FIND_DATAA file_data;
        HANDLE hFile = FindFirstFileA(file_mask.c_str(), &file_data);
        
        if (hFile != INVALID_HANDLE_VALUE) {
            do 
            {
                // Проверяем, что это файл, а не вложенная папка
                if (!(file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) 
                {
                    std::string full_path = base_dir + dir_data.cFileName + "/" + file_data.cFileName;
                    
                    if (count < available && CheckGame(full_path.c_str(), array[count])) 
                    {
                        count++;
                    }
                }
            } 
            while (count < available && FindNextFileA(hFile, &file_data));
            
            FindClose(hFile);
        }
        
    } 
    while (count < available && FindNextFileA(hDir, &dir_data));
    
    FindClose(hDir);
    return count;
}
bool GetBinary(const char *path, void* bin_info, uint32_t info_size, bool load_in_memory)
{	
	HMODULE hDll = LoadLibraryA(path);

	if (!hDll) return false;
	
    void *bin_ptr = reinterpret_cast<void*>(GetProcAddress(hDll, LETO_EXECUTABLE_INFO_VAR_TEXT));

	if (!bin_ptr || strncmp(reinterpret_cast<const char*>(bin_ptr), LETO_EXECUTABLE_TYPE_GAME, 16) != 0 )
	{
		FreeLibrary(hDll);
		return false;
	}

    memcpy(bin_info, bin_ptr, info_size);

    if (load_in_memory)
    {
        if (hLoadedDll) FreeLibrary(hLoadedDll);
        hLoadedDll = hDll;
    }
    else
        FreeLibrary(hDll);
	return true;
}

static void UnloadBinary()
{
    if (hLoadedDll)
    {
        FreeLibrary(hLoadedDll);
        hLoadedDll = nullptr;
    }
}

#endif

// ======================================================================

// ======================================================================
//                        Реализация для STM32
// ======================================================================

#ifdef USE_HAL_DRIVER

#include <FatFs/low_level/ff.h>
#include <HAL_include/HAL.hpp>
#include <LetoAPI_V1_System/File/FileHandlerFatFs.hpp>

extern SPI_HandleTypeDef hspi1;

uint32_t ScanApps(AppInfo* array, uint32_t available)
{
    if (!array || available == 0)
        return 0;

	FRESULT res;
    uint32_t count = 0;

    const char* base_path = "/";

    DIR dir;
    res = f_opendir(&dir, base_path);
    if(res != FR_OK)
    {
    	VC_Printf("[SG]f_opendir() failed, res = %d\r\n", RedColor, res);
        return 0;
    }

    FILINFO fileInfo;
    while(count < available)
    {
        res = f_readdir(&dir, &fileInfo);
        if((res != FR_OK) || (fileInfo.fname[0] == '\0'))
            break;

        if (fileInfo.fattrib & AM_DIR)
            continue;

        char rel_path[128];
        snprintf(rel_path, sizeof(rel_path), "%s%s", base_path, fileInfo.fname);

        if (CheckGame(rel_path, array[count]))
        {
            VC_Printf("[SG] %s\r\n", BlueColor, array[count].en_name);
            count++;
        }
    }

    f_closedir(&dir);
    VC_Printf("[SG]Done!\r\n", BlueColor);
    return count;
}

constexpr uint32_t GRAM_BASE = 0x20016000;

bool GetBinary(const char *path, void* bin_info, uint32_t info_size, bool load_in_memory)
{
	static char static_buffer[256];

	char* buffer 			= load_in_memory ? (char*)GRAM_BASE	: static_buffer;
	uint32_t buffer_size 	= load_in_memory ? 40 * 1024 		: sizeof(static_buffer);

	FIL file;
	FRESULT res;
	UINT bytes_read;

	res = f_open(&file, path, FA_READ);
	if (res != FR_OK)
	{
		return false;
	}

	res = f_read(&file, buffer, buffer_size, &bytes_read);
	f_close(&file);

	if (res == FR_OK && bytes_read > 0 &&
		strncmp(buffer, LETO_EXECUTABLE_TYPE_GAME, 16) == 0)
	{
		memcpy(bin_info, buffer, info_size);
		return true;
	}
	return false;
}

static void UnloadBinary()
{
    FileHandlerFatFs::ClearPool();
}

#endif

// ======================================================================
//                    Реализация для Unix (Linux и macOS)
// ======================================================================

#if defined(__unix__) || defined(__APPLE__)

#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <System/SystemSceneManager.hpp>

/// Загруженная библиотека игры: держим между LoadApplication и UnloadApplication
static void* hLoadedLibrary = nullptr;

/// Путь загруженной библиотеки: нужен для проверки выгрузки в отладочной сборке
static std::string loadedLibraryPath;

/// Состояние общей арены перед запуском игры: к нему она откатывается при выгрузке
static void* arenaMarker = nullptr;

namespace
{
    /// Корни для поиска каталога Apps: текущий каталог, затем $LETO_PATH
    std::vector<std::filesystem::path> SearchRoots()
    {
        std::vector<std::filesystem::path> roots;

        std::error_code ec;
        std::filesystem::path cwd = std::filesystem::current_path(ec);
        if (!ec) roots.push_back(cwd);

        const char* leto_path = getenv("LETO_PATH");
        if (leto_path && *leto_path) roots.emplace_back(leto_path);

        return roots;
    }

    /// Имя пресета из -DLETO_PRESET_NAME; пусто, если дефайна нет
    std::filesystem::path PresetPath()
    {
#ifdef LETO_PRESET_NAME
        return std::filesystem::path(LETO_PRESET_NAME);
#else
        return {};
#endif
    }

    bool IsDir(const std::filesystem::path& path)
    {
        std::error_code ec;
        return std::filesystem::is_directory(path, ec);
    }

    /**
     * @brief Каталоги с играми в порядке приоритета
     *
     * <root>/Apps/<preset>/ раньше, чем <root>/Apps/: более конкретный каталог
     * побеждает. Отсутствие каталога не ошибка — шаг просто пропускается.
     */
    std::vector<std::filesystem::path> ResolveAppsDirs()
    {
        std::vector<std::filesystem::path> dirs;
        const std::filesystem::path preset = PresetPath();

        // Порядок каскада: текущий каталог раньше $LETO_PATH, конкретный пресет
        // раньше общего Apps/. Склейка через operator/ не зависит от того,
        // оканчивается ли $LETO_PATH слэшем.
        for (const auto& root : SearchRoots())
        {
            if (!preset.empty())
            {
                std::filesystem::path with_preset = root / "Apps" / preset;
                if (IsDir(with_preset)) dirs.push_back(with_preset);
            }

            std::filesystem::path plain = root / "Apps";
            if (IsDir(plain)) dirs.push_back(plain);
        }

        return dirs;
    }

    /// Расширения игровых библиотек: на macOS ещё и .dylib
    bool HasLibraryExtension(const std::filesystem::path& path)
    {
        const std::string ext = path.extension().string();
        if (ext == ".so") return true;
#ifdef __APPLE__
        if (ext == ".dylib") return true;
#endif
        return false;
    }

    void* OpenLibrary(const char* path)
    {
        return dlopen(path, RTLD_NOW | RTLD_LOCAL);
    }

    /**
     * @brief Причина, по которой файл не принят за игру
     *
     * Сканирование идёт раз в секунду, поэтому сообщение пишется только при
     * смене причины для конкретного пути: иначе один битый файл вытеснит из
     * консоли всё остальное.
     */
    class ScanLog
    {
    public:
        void Report(const std::string& path, const char* reason)
        {
            auto it = std::find_if(entries, entries + count,
                    [&path](const Entry& entry) { return entry.path == path; });

            if (it == entries + count)
            {
                if (count == MaxEntries) return;

                it = &entries[count++];
                it->path = path;
            }
            else if (it->reason == reason)
            {
                return;
            }

            it->reason = reason;

            // Консоль хранит 31 символ на строку, поэтому абсолютный путь
            // вытеснил бы причину: печатается только имя файла
            VC_Printf("%s: %s\r\n", YellowColor, std::filesystem::path(path).filename().c_str(), reason);
        }

        void ForgetMissing(const std::string& path)
        {
            auto it = std::find_if(entries, entries + count,
                    [&path](const Entry& entry) { return entry.path == path; });

            if (it == entries + count) return;

            *it = entries[--count];
        }

    private:
        static constexpr uint32_t MaxEntries = 8;

        struct Entry
        {
            std::string path;
            std::string reason;
        };

        Entry entries[MaxEntries];
        uint32_t count{};
    };

    ScanLog& ScanLogInstance()
    {
        static ScanLog log;
        return log;
    }

    /**
     * @brief Прочитать заголовок игры, не оставляя библиотеку загруженной
     *
     * Единственная точка входа — переменная `__leto_exec_info__`: имена функций
     * игра наружу не экспортирует, поэтому `dlsym(handle, "Init")` бесполезен.
     */
    bool ReadHeader(const char* path, AppBinHeader& header)
    {
        void* handle = OpenLibrary(path);
        if (!handle)
        {
            ScanLogInstance().Report(path, "не открывается");
            return false;
        }

        const void* info = dlsym(handle, LETO_EXECUTABLE_INFO_VAR_TEXT);
        if (info) memcpy(&header, info, sizeof(AppBinHeader));

        dlclose(handle);

        if (!info)
        {
            ScanLogInstance().Report(path, "нет " LETO_EXECUTABLE_INFO_VAR_TEXT);
            return false;
        }

        if (strncmp(header.type, LETO_EXECUTABLE_TYPE_GAME, sizeof(header.type)) != 0)
        {
            ScanLogInstance().Report(path, "не игра");
            return false;
        }

        ScanLogInstance().ForgetMissing(path);
        return true;
    }
}

uint32_t ScanApps(AppInfo* array, uint32_t available)
{
    if (!array || available == 0)
        return 0;

    uint32_t count = 0;

    for (const auto& apps_dir : ResolveAppsDirs())
    {
        std::error_code ec;
        std::filesystem::directory_iterator games{ apps_dir, ec };
        if (ec) continue;

        const uint32_t found_before = count;

        for (; !ec && games != std::filesystem::directory_iterator() && count < available;
                games.increment(ec))
        {
            if (!games->is_directory(ec)) continue;

            std::error_code file_ec;
            std::filesystem::directory_iterator files{ games->path(), file_ec };

            for (; !file_ec && files != std::filesystem::directory_iterator() && count < available;
                    files.increment(file_ec))
            {
                const std::filesystem::path candidate = files->path();

                if (!HasLibraryExtension(candidate) || !files->is_regular_file(file_ec))
                    continue;

                AppBinHeader header{};
                if (!ReadHeader(candidate.c_str(), header))
                    continue;

                array[count].FromBinary(header);
                snprintf(array[count].path, sizeof(array[count].path), "%s", candidate.c_str());
                count++;
            }
        }

        // Каталог с играми выигрывает у менее конкретного: дальше не ищем,
        // иначе одна и та же игра попадёт в список дважды
        if (count > found_before) break;
    }

    return count;
}

bool GetBinary(const char *path, void* bin_info, uint32_t info_size, bool load_in_memory)
{
    if (!path || !bin_info || !info_size)
        return false;

    void* handle = OpenLibrary(path);
    if (!handle)
        return false;

    const void* info = dlsym(handle, LETO_EXECUTABLE_INFO_VAR_TEXT);
    if (!info || strncmp(static_cast<const char*>(info), LETO_EXECUTABLE_TYPE_GAME, 16) != 0)
    {
        dlclose(handle);
        return false;
    }

    memcpy(bin_info, info, info_size);

    if (load_in_memory)
    {
        if (hLoadedLibrary) dlclose(hLoadedLibrary);
        hLoadedLibrary = handle;
    }
    else
    {
        dlclose(handle);
    }

    return true;
}

static void UnloadBinary()
{
    if (hLoadedLibrary)
    {
        dlclose(hLoadedLibrary);
        hLoadedLibrary = nullptr;
    }
}

bool LoadApplication(const char *path)
{
    if (!path || !*path)
        return false;

    if (CurrentLoadedApp)
        return false;

    std::error_code ec;
    const std::filesystem::path full = std::filesystem::weakly_canonical(path, ec);
    if (ec)
    {
        VC_Printf("%s: путь не разрешается\r\n", RedColor, std::filesystem::path(path).filename().c_str());
        return false;
    }

    const std::string full_path = full.string();

    // Консоль хранит 31 символ на строку: с абсолютным путём причина отказа
    // обрезалась бы, поэтому в сообщениях только имя библиотеки
    const std::string lib_name = full.filename().string();

    void* handle = OpenLibrary(full_path.c_str());
    if (!handle)
    {
        VC_Printf("%s: не открывается\r\n", RedColor, lib_name.c_str());
        return false;
    }

    const void* info = dlsym(handle, LETO_EXECUTABLE_INFO_VAR_TEXT);
    if (!info)
    {
        VC_Printf("%s: нет %s\r\n", RedColor, lib_name.c_str(), LETO_EXECUTABLE_INFO_VAR_TEXT);
        dlclose(handle);
        return false;
    }

    AppBinHeader bin_info{};
    memcpy(&bin_info, info, sizeof(AppBinHeader));

    if (strncmp(bin_info.type, LETO_EXECUTABLE_TYPE_GAME, sizeof(bin_info.type)) != 0)
    {
        VC_Printf("%s: не игра\r\n", RedColor, lib_name.c_str());
        dlclose(handle);
        return false;
    }

    if (bin_info.api_version != 1)
    {
        VC_Printf("%s: App_V%d нет\r\n", RedColor, lib_name.c_str(), bin_info.api_version);
        dlclose(handle);
        return false;
    }

    if (hLoadedLibrary) dlclose(hLoadedLibrary);
    hLoadedLibrary = handle;
    loadedLibraryPath = full_path;

    /*
        Игра аллоцирует себя из общей арены хоста через LetoAPI, а её Clean()
        объект не разрушает. Без отката по маркеру память не вернётся, если
        сцена не переключится: SystemSceneManager::OnMenuDoubleClick как раз
        выгружает игру без переключения сцены.
    */
    arenaMarker = SystemSceneManager::Instance().GetCommonAllocator().Alloc(1);

    static LetoApplication_V1 app;
    memcpy(&app, info, sizeof(LetoApplication_V1));

    LetoResult_V1 res = app.Init(leto_api_v1);
    if (res != LETO_V1_SUCCESS)
    {
        VC_Printf("%s: Init %d\r\n", RedColor, lib_name.c_str(), res);
        UnloadApplication();
        return false;
    }

    CurrentLoadedApp = reinterpret_cast<AppBinHeader*>(&app);

    CurrentLoadedAppInfo.FromBinary(bin_info);
    snprintf(CurrentLoadedAppInfo.path, sizeof(CurrentLoadedAppInfo.path), "%s", full_path.c_str());

    return true;
}

void UnloadApplication()
{
    if (CurrentLoadedApp)
    {
        if (CurrentLoadedApp->api_version == 1)
            reinterpret_cast<LetoApplication_V1*>(CurrentLoadedApp)->Clean();

        // До dlclose: указатель смотрит в библиотеку
        CurrentLoadedApp = nullptr;
    }

    if (leto_api_v1)
        leto_api_v1->Lobby->QuitLobby();

    UnloadBinary();

    /*
        Откат арены строго последним: Clear() обнуляет всё, что игра успела
        выделить, включая объекты, на которые ещё ссылаются её же сцены.
    */
    if (arenaMarker)
    {
        SystemSceneManager::Instance().GetCommonAllocator().Clear(arenaMarker);
        arenaMarker = nullptr;
    }

#ifndef NDEBUG
    if (!loadedLibraryPath.empty())
    {
        // Возвращённый хендл означает, что в .so остались STB_GNU_UNIQUE:
        // glibc такой DSO не выгружает, нужен -fno-gnu-unique
        if (void* mapped = dlopen(loadedLibraryPath.c_str(), RTLD_NOLOAD | RTLD_NOW))
        {
            VC_Printf("%s: не выгрузился\r\n", YellowColor, std::filesystem::path(loadedLibraryPath).filename().c_str());
            dlclose(mapped);
        }
        loadedLibraryPath.clear();
    }
#endif
}

// ======================================================================
//          Общая часть: заголовок игры по пути (через GetBinary)
// ======================================================================

#else

bool LoadApplication(const char *path)
{
        VC_Printf("LoadApplication\r\n", BlueColor);

    if (CurrentLoadedApp) return false;

    AppBinHeader bin_info{};
    if (!GetBinary(path, &bin_info, sizeof(AppBinHeader)))
    {
        VC_Printf("Get AppBinHeader fail! \r\n", RedColor);
        return false;
    }
        VC_Printf("Get AppBinHeader success! \r\n", GreenColor);

    if (bin_info.api_version == 1)
    {
        static LetoApplication_V1 app;
        if (!GetBinary(path, &app, sizeof(LetoApplication_V1), true))
        {
                VC_Printf("Get App_V1 fail \r\n", RedColor);
            return false;
        }
        VC_Printf("Get App_V1 success!\r\n", GreenColor);

#ifdef USE_HAL_DRIVER
        __DSB(); // Ждем, пока ВСЕ данные физически запишутся в RAM из буферов
        __ISB(); // Очищаем конвейер, чтобы процессор гарантированно читал новые инструкции
#endif

        LetoResult_V1 res = app.Init(leto_api_v1);
        if (res != LETO_V1_SUCCESS)
        {
                VC_Printf("App_V1 Init fail\r\n", RedColor);
                VC_Printf("%res=%d\r\n", RedColor, res);
            UnloadApplication();
            return false;
        }
        VC_Printf("App_V1 Init success!\r\n", GreenColor);

        CurrentLoadedApp = reinterpret_cast<AppBinHeader*>(&app);

        CurrentLoadedAppInfo.FromBinary(bin_info);
        snprintf(CurrentLoadedAppInfo.path, sizeof(CurrentLoadedAppInfo.path), "%s", path);

        return true;
    }

        VC_Printf("App_V%d not supported! \r\n", RedColor, bin_info.api_version);
    UnloadApplication();
    return false;
}


void UnloadApplication()
{
    if (CurrentLoadedApp)
    {
        if (CurrentLoadedApp->api_version == 1)
        {
            reinterpret_cast<LetoApplication_V1*>(CurrentLoadedApp)->Clean();
        }
        CurrentLoadedApp = nullptr;
    }

    leto_api_v1->Lobby->QuitLobby();

    UnloadBinary();
}

#endif
