/*
   Общие помощники тестов: временные каталоги, переменные окружения и рабочий
   каталог как RAII. Загрузчик ищет Apps относительно cwd и $LETO_PATH, поэтому
   тесту нужно менять и то, и другое, не протекая в соседние тесты.
*/

#ifndef INC_TESTS_TEST_SUPPORT_HPP_
#define INC_TESTS_TEST_SUPPORT_HPP_

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

namespace test
{
    /// Временный каталог, удаляемый вместе с содержимым
    class TempDir
    {
    public:
        TempDir(const char* prefix)
                : path{ std::filesystem::temp_directory_path()
                        / (std::string(prefix) + "_" + std::to_string(::getpid())) }
        {
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path);
        }

        ~TempDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }

        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;

        const std::filesystem::path& Path() const { return path; }

    private:
        std::filesystem::path path;
    };

    /// Переменная окружения, восстанавливаемая при выходе из области видимости
    class ScopedEnv
    {
    public:
        ScopedEnv(const char* name, const char* value) : name{ name }
        {
            const char* old = getenv(name);
            had_value = old != nullptr;
            if (had_value) old_value = old;

            if (value)
                setenv(name, value, 1);
            else
                unsetenv(name);
        }

        ~ScopedEnv()
        {
            if (had_value)
                setenv(name.c_str(), old_value.c_str(), 1);
            else
                unsetenv(name.c_str());
        }

        ScopedEnv(const ScopedEnv&) = delete;
        ScopedEnv& operator=(const ScopedEnv&) = delete;

    private:
        std::string name;
        std::string old_value;
        bool had_value;
    };

    /// Рабочий каталог, восстанавливаемый при выходе из области видимости
    class ScopedCwd
    {
    public:
        explicit ScopedCwd(const std::filesystem::path& next)
                : previous{ std::filesystem::current_path() }
        {
            std::filesystem::current_path(next);
        }

        ~ScopedCwd()
        {
            std::error_code ec;
            std::filesystem::current_path(previous, ec);
        }

        ScopedCwd(const ScopedCwd&) = delete;
        ScopedCwd& operator=(const ScopedCwd&) = delete;

    private:
        std::filesystem::path previous;
    };

    inline void WriteFile(const std::filesystem::path& path, const std::string& data)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    /// Строки файла журнала фикстуры; отсутствующий файл — пустой список
    inline std::vector<std::string> ReadLines(const std::filesystem::path& path)
    {
        std::vector<std::string> lines;

        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lines.push_back(line);
        }

        return lines;
    }

    inline size_t CountLines(const std::filesystem::path& path, const std::string& value)
    {
        size_t count = 0;
        for (const std::string& line : ReadLines(path))
            if (line == value) count++;
        return count;
    }

    /// Резидентная память процесса в килобайтах; -1, если /proc недоступен
    inline long ResidentKb()
    {
        std::ifstream status("/proc/self/status");
        std::string key;
        while (status >> key)
        {
            if (key != "VmRSS:") continue;

            long value = -1;
            status >> value;
            return value;
        }

        return -1;
    }

    /// Собрать каталог игры с одной библиотекой: <root>/Apps/<sub>/<Game>/lib<name>.so
    inline std::filesystem::path MakeGameDir(const std::filesystem::path& root,
            const std::string& sub, const std::string& game, const std::string& source)
    {
        const std::filesystem::path dir = root / "Apps" / sub / game;
        std::filesystem::create_directories(dir);
        std::filesystem::copy_file(source, dir / std::filesystem::path(source).filename(),
                std::filesystem::copy_options::overwrite_existing);
        return dir;
    }
}

#endif
