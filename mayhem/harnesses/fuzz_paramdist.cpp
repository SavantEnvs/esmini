// OpenSCENARIO parameter value distribution file (esmini --param_dist): deterministic and stochastic
// distributions + permutations, applied to a fixed bundled scenario. Goes through SE_InitWithArgs,
// the library form of the esmini command line (which handles the parser's exceptions itself).
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
    std::string dist = WriteTemp(data, size, ".xosc");
    if (dist.empty()) return 0;
    std::string osc = g_root + "/resources/xosc/cut-in.xosc";
    const char *argv[] = {"esmini", "--headless", "--disable_log", "--disable_stdout", "--param_dist", dist.c_str(),
                          "--param_permutation", "1", "--osc", osc.c_str()};
    SE_ClearPaths();
    if (SE_InitWithArgs(static_cast<int>(sizeof(argv) / sizeof(argv[0])), argv) == 0)
    {
        SE_GetNumberOfPermutations();
        for (int i = 0; i < 5 && !SE_GetQuitFlag(); i++)
            if (SE_StepDT(0.05) != 0) break;
    }
    SE_Close();
    // The loaded distribution is process-global ("re-using parameter distribution"): reset per input.
    SE_ResetParameterDistribution();
    unlink(dist.c_str());
    return 0;
}
