#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#include <new>
#include <windows.h>
#elif __cplusplus >= 201703L
#include <filesystem>
#else // Assume POSIX
#include <sys/stat.h>
#endif

#include "FileSystem.hpp"
#include "pbg3/Pbg3Archive.hpp"
#include "utils.hpp"

#ifndef _WIN32
#include <cctype>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#endif

u32 g_LastFileSize;

#ifndef _WIN32
static bool PathExists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string UpperAscii(const std::string &str)
{
    std::string out = str;
    for (char &c : out)
    {
        c = (char)std::toupper((unsigned char)c);
    }
    return out;
}

static bool EndsWith(const std::string &str, const std::string &suffix)
{
    return str.size() >= suffix.size() && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string FindEntry(const std::string &dir, const std::string &name, bool lastComponent)
{
    DIR *handle = opendir(dir.empty() ? "." : dir.c_str());
    if (handle == NULL)
    {
        return "";
    }

    std::string wanted = UpperAscii(name);
    std::string datSuffix;
    if (lastComponent && wanted.size() >= 6 && EndsWith(wanted, ".DAT"))
    {
        datSuffix = wanted.substr(wanted.size() - 6);
    }

    std::string exact;
    std::string suffixMatch;
    struct dirent *entry;
    while ((entry = readdir(handle)) != NULL)
    {
        std::string entryName = entry->d_name;
        std::string upper = UpperAscii(entryName);
        if (upper == wanted)
        {
            exact = entryName;
            break;
        }
        if (!datSuffix.empty() && suffixMatch.empty() && EndsWith(upper, datSuffix))
        {
            suffixMatch = entryName;
        }
    }
    closedir(handle);
    return !exact.empty() ? exact : suffixMatch;
}
#endif

std::string FileSystem::ResolvePath(const char *filepath)
{
    std::string path = filepath != NULL ? filepath : "";
#ifndef _WIN32
    for (char &c : path)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    if (path.empty() || PathExists(path))
    {
        return path;
    }

    std::string resolved;
    std::size_t pos = 0;
    if (path[0] == '/')
    {
        resolved = "/";
        pos = 1;
    }

    while (pos <= path.size())
    {
        std::size_t next = path.find('/', pos);
        bool last = next == std::string::npos;
        std::string component = path.substr(pos, last ? std::string::npos : next - pos);
        pos = last ? path.size() + 1 : next + 1;

        if (component.empty() || component == ".")
        {
            continue;
        }

        std::string candidate = resolved;
        if (!candidate.empty() && candidate.back() != '/')
        {
            candidate += '/';
        }

        if (component != ".." && !PathExists(candidate + component))
        {
            std::string found = FindEntry(resolved, component, last);
            if (found.empty())
            {
                return path;
            }
            component = found;
        }
        resolved = candidate + component;
    }
    return resolved;
#else
    return path;
#endif
}

FILE *FileSystem::FopenUTF8(const char *filepath, const char *mode)
{
#ifndef _WIN32
    FILE *file = std::fopen(filepath, mode);
    if (file == NULL && mode != NULL && mode[0] == 'r')
    {
        std::string resolved = ResolvePath(filepath);
        if (resolved != filepath)
        {
            file = std::fopen(resolved.c_str(), mode);
        }
    }
    return file;
#else
    u32 filepathWLen = MultiByteToWideChar(CP_UTF8, 0, filepath, -1, NULL, 0) * 2;
    u32 modeWLen = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0) * 2;

    if (filepathWLen == 0 || modeWLen == 0)
    {
        return NULL;
    }

    wchar_t *filepathW = new wchar_t[filepathWLen];
    wchar_t *modeW = new wchar_t[modeWLen];

    MultiByteToWideChar(CP_UTF8, 0, filepath, -1, filepathW, filepathWLen / 2);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, modeW, modeWLen / 2);

    FILE *f = _wfopen(filepathW, modeW);

    delete[] filepathW;
    delete[] modeW;

    return f;
#endif
}

void FileSystem::CreateDir(const char *path)
{
#ifdef _WIN32
    _mkdir(path);
#elif __cplusplus >= 201703L
    auto p = std::filesystem::path(path);
    std::filesystem::create_directory(p);
#else
    mkdir(path, 0755);
#endif
}

u8 *FileSystem::OpenPath(const char *filepath, int isExternalResource)
{
    u8 *data;
    FILE *file;
    size_t fsize;
    i32 entryIdx;
    const char *entryname;
    i32 pbg3Idx;

    entryIdx = -1;
    if (isExternalResource == 0)
    {
        entryname = std::strrchr(filepath, '\\');
        if (entryname == (char *)0x0)
        {
            entryname = filepath;
        }
        else
        {
            entryname = entryname + 1;
        }
        entryname = std::strrchr(entryname, '/');
        if (entryname == (char *)0x0)
        {
            entryname = filepath;
        }
        else
        {
            entryname = entryname + 1;
        }
        if (g_Pbg3Archives != NULL)
        {
            for (pbg3Idx = 0; pbg3Idx < 0x10; pbg3Idx += 1)
            {
                if (g_Pbg3Archives[pbg3Idx] != NULL)
                {
                    entryIdx = g_Pbg3Archives[pbg3Idx]->FindEntry(entryname);
                    if (entryIdx >= 0)
                    {
                        break;
                    }
                }
            }
        }
        if (entryIdx < 0)
        {
            return NULL;
        }
    }
    if (entryIdx >= 0)
    {
        utils::DebugPrint2("%s Decode ... \n", entryname);
        data = g_Pbg3Archives[pbg3Idx]->ReadDecompressEntry(entryIdx, entryname);
        g_LastFileSize = g_Pbg3Archives[pbg3Idx]->GetEntrySize(entryIdx);
    }
    else
    {
        utils::DebugPrint2("%s Load ... \n", filepath);
        file = FopenUTF8(filepath, "rb");
        if (file == NULL)
        {
            utils::DebugPrint2("error : %s is not found.\n", filepath);
            return NULL;
        }
        else
        {
            std::fseek(file, 0, SEEK_END);
            fsize = std::ftell(file);
            g_LastFileSize = fsize;
            std::fseek(file, 0, SEEK_SET);
            data = (u8 *)std::malloc(fsize);
            std::fread(data, 1, fsize, file);
            std::fclose(file);
        }
    }
    return data;
}

int FileSystem::WriteDataToFile(const char *path, const void *data, size_t size)
{
    FILE *f;

    f = FopenUTF8(path, "wb");
    if (f == NULL)
    {
        return -1;
    }
    else
    {
        if (std::fwrite(data, 1, size, f) != size)
        {
            std::fclose(f);
            return -2;
        }
        else
        {
            std::fclose(f);
            return 0;
        }
    }
}
