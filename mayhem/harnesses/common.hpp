// Shared helpers for the esmini libFuzzer harnesses.
#pragma once
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <string>
#include <unistd.h>
#include <climits>
#include <cstring>

// Repo root, derived from the harness location (<root>/mayhem/bin/<harness>), so the bundled
// resources/ (road networks, catalogs, scenarios) are found without hard-coding an absolute path.
static std::string RepoRoot()
{
    char buf[PATH_MAX] = {0};
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    std::string p = n > 0 ? std::string(buf, static_cast<size_t>(n)) : std::string(".");
    for (int i = 0; i < 3; i++)
    {
        size_t s = p.find_last_of('/');
        p = s == std::string::npos ? std::string(".") : p.substr(0, s);
    }
    return p;
}

// Write the input to a fresh file under $TMPDIR with the given suffix; returns its path ("" on failure).
static std::string WriteTemp(const uint8_t *data, size_t size, const char *suffix)
{
    const char *tmp = getenv("TMPDIR");
    std::string path = std::string(tmp && *tmp ? tmp : "/tmp") + "/esmini_fuzz_XXXXXX" + suffix;
    std::string buf = path;
    int fd = mkstemps(&buf[0], static_cast<int>(strlen(suffix)));
    if (fd < 0) return "";
    size_t off = 0;
    while (off < size)
    {
        ssize_t w = write(fd, data + off, size - off);
        if (w <= 0) break;
        off += static_cast<size_t>(w);
    }
    close(fd);
    return buf;
}
