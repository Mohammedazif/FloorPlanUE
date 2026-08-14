#include "Dxf/DxfTagCursor.h"

namespace FloorPlan::Dxf
{
    DxfTagCursor::DxfTagCursor(DxfTagReader& reader) : Reader(reader) {}

    bool DxfTagCursor::Fail(DiagnosticCode code, std::string message)
    {
        FailureState.Code = code;
        FailureState.Message = std::move(message);
        FailureState.LineNumber = Current.LineNumber;
        FailureState.ByteOffset = Current.ByteOffset;
        return false;
    }

    bool DxfTagCursor::Advance()
    {
        if (Exhausted)
        {
            HasCurrent = false;
            return false;
        }
        if (!Reader.Next(Current))
        {
            Exhausted = true;
            HasCurrent = false;
            if (Reader.Failed())
            {
                FailureState = Reader.Failure();
            }
            return false;
        }
        HasCurrent = true;
        return true;
    }

    bool DxfTagCursor::SkipToNextStart()
    {
        while (Advance())
        {
            if (Current.IsStart())
            {
                return true;
            }
        }
        return false;
    }

    void DxfTagCursor::SkipRecord()
    {
        while (Advance() && !Current.IsStart())
        {
        }
    }
}
