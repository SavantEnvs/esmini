// esmini binary recording (.dat): parse with the Replay class used by the replayer and dat2csv,
// then walk the timeline the way dat2csv does.
#include "common.hpp"
#include <exception>
#include "Replay.hpp"

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    SE_Env::Inst().GetOptions().SetOptionValue("disable_log", "", false, true);
    SE_Env::Inst().GetOptions().SetOptionValue("disable_stdout", "", false, true);
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    std::string path = WriteTemp(data, size, ".dat");
    if (path.empty()) return 0;
    try
    {
        scenarioengine::Replay r(path, true);
        for (size_t i = 0; i < r.timestamps_.size() && i < 2000; i++)
        {
            for (const auto &kv : r.objects_timeline_)
            {
                auto e = r.GetReplayEntryAtTimeIncremental(kv.first, r.timestamps_[i]);
                (void)e;
                auto b = r.GetReplayEntryAtTimeBinary(kv.first, r.timestamps_[i]);
                (void)b;
            }
        }
        r.GoToStart();
        for (int i = 0; i < 200; i++)
        {
            if (r.GoToNextFrame() != 0) break;
            for (int id : r.GetAllObjectIDs()) r.GetState(id);
        }
        r.GoToTime(r.GetStopTime() / 2);
        r.GoToEnd();
    }
    catch (const std::exception &)
    {
        // Replay reports malformed recordings by throwing (LOG_ERROR_AND_QUIT)
    }
    unlink(path.c_str());
    return 0;
}
