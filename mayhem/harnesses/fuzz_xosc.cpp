// OpenSCENARIO (.xosc) scenario: parse + run through the public esminiLib C API.
// Reaches ScenarioReader, entities/trailers, actions, conditions, routes, controllers, expressions
// and RoadManager (via the bundled .xodr files the scenario references).
#include "common.hpp"
#include "esminiLib.hpp"

static std::string g_root;

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    g_root = RepoRoot();
    SE_SetOptionPersistent("disable_log");
    SE_SetOptionPersistent("disable_stdout");
    return 0;
}

static void AddResourcePaths()
{
    // Reset every input: search paths are process-global.
    SE_ClearPaths();
    SE_AddPath((g_root + "/resources/xosc").c_str());
    SE_AddPath((g_root + "/resources/xodr").c_str());
    SE_AddPath((g_root + "/resources/xosc/Catalogs/Vehicles").c_str());
    SE_AddPath((g_root + "/resources").c_str());
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string xml(reinterpret_cast<const char *>(data), size);  // NUL-terminated copy (C string API)
    AddResourcePaths();
    if (SE_InitWithString(xml.c_str(), 0, 0, 0, 0) == 0)
    {
        for (int i = 0; i < 40 && !SE_GetQuitFlag(); i++)
        {
            if (SE_StepDT(0.05) != 0) break;
            int n = SE_GetNumberOfObjects();
            for (int j = 0; j < n; j++)
            {
                SE_ScenarioObjectState st;
                SE_GetObjectState(SE_GetId(j), &st);
            }
        }
    }
    SE_Close();
    return 0;
}
