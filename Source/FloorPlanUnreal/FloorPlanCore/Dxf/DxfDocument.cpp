#include "Dxf/DxfDocument.h"

namespace FloorPlan::Dxf
{
    double MillimetresPerUnit(int insertUnits)
    {
        switch (insertUnits)
        {
        case 1: return 25.4;
        case 2: return 304.8;
        case 3: return 1609344.0;
        case 4: return 1.0;
        case 5: return 10.0;
        case 6: return 1000.0;
        case 7: return 1000000.0;
        case 8: return 25.4e-6;
        case 9: return 25.4e-3;
        case 10: return 914.4;
        case 11: return 1.0e-7;
        case 12: return 1.0e-6;
        case 13: return 1.0e-3;
        case 14: return 100.0;
        case 15: return 10000.0;
        case 16: return 100000.0;
        case 20: return 3.0857e19;
        case 21: return 304.80060960122;
        case 22: return 25.400050800102;
        case 23: return 914.40182880366;
        case 24: return 1609347.2186944;
        default: return 0.0;
        }
    }
}
