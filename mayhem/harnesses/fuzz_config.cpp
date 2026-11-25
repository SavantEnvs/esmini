// esmini YAML config file (--config_file_path): TINY_YAML parser + option injection into the
// command-line option parser, then a short run of a fixed bundled scenario.
#include "common.hpp"
#include "esminiLib.hpp"

static std::string g_root;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    g_root = RepoRoot();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string cfg = WriteTemp(data, size, ".yml");
    if (cfg.empty()) return 0;
    std::string osc = g_root + "/resources/xosc/cut-in.xosc";
    const char *argv[] = {"esmini", "--headless", "--disable_log", "--disable_stdout", "--config_file_path", cfg.c_str(),
                          "--osc", osc.c_str()};
    SE_ClearPaths();
    if (SE_InitWithArgs(static_cast<int>(sizeof(argv) / sizeof(argv[0])), argv) == 0)
    {
        for (int i = 0; i < 5 && !SE_GetQuitFlag(); i++)
            if (SE_StepDT(0.05) != 0) break;
    }
    SE_Close();
    unlink(cfg.c_str());
    return 0;
}
