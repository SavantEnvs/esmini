// OpenDRIVE (.xodr) road network: parse + position queries through the public esminiRMLib C API.
// Layout: bytes 0..7 pick query parameters, the rest is the OpenDRIVE XML (NUL-terminated copy).
#include "common.hpp"
#include "esminiRMLib.hpp"

extern "C" int LLVMFuzzerInitialize(int *, char ***)
{
    RM_SetOptionPersistent("disable_log");
    RM_SetOptionPersistent("disable_stdout");
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size < 8) return -1;
    const uint8_t *p = data;
    std::string xml(reinterpret_cast<const char *>(data + 8), size - 8);
    if (RM_InitWithString(xml.c_str()) == 0)
    {
        int h = RM_CreatePosition();
        int nroads = RM_GetNumberOfRoads();
        for (int i = 0; i < nroads && i < 16; i++)
        {
            id_t rid = RM_GetIdOfRoadFromIndex(static_cast<unsigned int>(i));
            double len = RM_GetRoadLength(rid);
            double s = len * (p[0] / 255.0);
            RM_GetRoadIdString(rid);
            int nl = RM_GetRoadNumberOfLanes(rid, s, -1);
            for (int l = 0; l < nl && l < 8; l++)
            {
                int lane_id = 0;
                RM_GetLaneIdByIndex(rid, l, s, -1, &lane_id);
                double w;
                RM_GetLaneWidthByRoadId(rid, lane_id, s, &w);
                RM_GetLaneTypeByRoadId(rid, lane_id, s);
                if (RM_SetLanePosition(h, rid, lane_id, 0.0, s, true) == 0)
                {
                    RM_RoadLaneInfo li;
                    RM_RoadProbeInfo pi;
                    RM_GetLaneInfo(h, 5.0 + p[1], &li, p[2] % 3, p[2] & 4);
                    RM_GetProbeInfo(h, 5.0 + p[1], &pi, p[2] % 3, p[2] & 4);
                    RM_PositionMoveForward(h, (p[3] - 128) * 0.5, (p[4] - 128) / 40.0);
                    RM_PositionData pd;
                    RM_GetPositionData(h, &pd);
                    RM_GetSpeedLimit(h);
                }
            }
            int nsigns = RM_GetNumberOfRoadSigns(rid);
            for (int k = 0; k < nsigns && k < 8; k++)
            {
                RM_RoadSign rs;
                RM_GetRoadSign(rid, static_cast<unsigned int>(k), &rs);
                int nv = RM_GetNumberOfRoadSignValidityRecords(rid, static_cast<unsigned int>(k));
                for (int v = 0; v < nv && v < 4; v++)
                {
                    RM_RoadObjValidity val;
                    RM_GetRoadSignValidityRecord(rid, static_cast<unsigned int>(k), static_cast<unsigned int>(v), &val);
                }
            }
        }
        // World -> track coordinate mapping (XYZ2TrackPos)
        double x = (static_cast<int8_t>(p[5])) * 4.0, y = (static_cast<int8_t>(p[6])) * 4.0, hd = p[7] / 40.0;
        RM_SetWorldXYHPosition(h, x, y, hd);
        RM_SetWorldPosition(h, y, x, 0.0, hd, 0.0, 0.0);
        RM_PositionData pd;
        RM_GetPositionData(h, &pd);
        RM_GeoReference geo;
        RM_GetOpenDriveGeoReference(&geo);
    }
    RM_Close();
    return 0;
}
