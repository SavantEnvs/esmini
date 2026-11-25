// OpenSCENARIO vehicle catalog file: a fixed bundled scenario (resources/xosc/trailer_connect.xosc,
// a car + trailer) whose VehicleCatalog directory points at a per-input temp dir holding the fuzz
// input as VehicleCatalog.xosc. Reaches the catalog loader, vehicle/trailer entry parsing and the
// entity setup that consumes it, through the public esminiLib C API.
#include "common.hpp"
#include <fstream>
#include <sstream>
#include "esminiLib.hpp"

static std::string g_root;
static std::string g_scenario;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    g_root = RepoRoot();
    std::ifstream f(g_root + "/resources/xosc/trailer_connect.xosc");
    std::stringstream ss;
    ss << f.rdbuf();
    g_scenario = ss.str();
    SE_SetOptionPersistent("disable_log");
    SE_SetOptionPersistent("disable_stdout");
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const char *tmp = getenv("TMPDIR");
    std::string dir = std::string(tmp && *tmp ? tmp : "/tmp") + "/esmini_cat_XXXXXX";
    if (mkdtemp(&dir[0]) == nullptr) return 0;
    std::string cat = dir + "/VehicleCatalog.xosc";
    {
        std::ofstream o(cat, std::ios::binary);
        o.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
    }
    std::string xosc = g_scenario;
    const std::string from = "../xosc/Catalogs/Vehicles";
    size_t pos = xosc.find(from);
    if (pos != std::string::npos) xosc.replace(pos, from.size(), dir);

    SE_ClearPaths();
    SE_AddPath((g_root + "/resources/xosc").c_str());
    SE_AddPath((g_root + "/resources/xodr").c_str());
    if (SE_InitWithString(xosc.c_str(), 0, 0, 0, 0) == 0)
    {
        for (int i = 0; i < 10 && !SE_GetQuitFlag(); i++)
            if (SE_StepDT(0.05) != 0) break;
    }
    SE_Close();
    unlink(cat.c_str());
    rmdir(dir.c_str());
    return 0;
}
