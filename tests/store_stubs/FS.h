#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <string>
using String = std::string;
using std::max;

struct File
{
    std::string *data = nullptr;
    size_t pos = 0;
    size_t limit = SIZE_MAX;

    explicit operator bool() const
    {
        return data != nullptr;
    }

    size_t size() const
    {
        return data ? data->size() : 0;
    }

    int read()
    {
        return data && pos < data->size() ? uint8_t((*data)[pos++]) : -1;
    }

    size_t readBytes(char *out, size_t n)
    {
        size_t count = 0;
        for (int c; count < n && (c = read()) >= 0;)
        {
            out[count++] = char(c);
        }
        return count;
    }

    size_t write(uint8_t c)
    {
        if (!data || data->size() >= limit)
        {
            return 0;
        }
        data->push_back(char(c));
        return 1;
    }

    size_t write(const uint8_t *p, size_t n)
    {
        size_t count = 0;
        while (count < n && write(p[count]))
        {
            ++count;
        }
        return count;
    }

    void flush()
    {
    }

    void close()
    {
        data = nullptr;
    }
};

struct FS
{
    std::map<std::string, std::string> files;
    size_t writeLimit = SIZE_MAX;
    bool failOpen = false;

    void mkdir(const char *)
    {
    }

    File open(const String &path, const char *mode)
    {
        if (*mode == 'w')
        {
            if (failOpen)
            {
                return {};
            }
            files[path].clear();
            return {&files[path], 0, writeLimit};
        }
        auto found = files.find(path);
        return found == files.end() ? File {} : File {&found->second};
    }
};
