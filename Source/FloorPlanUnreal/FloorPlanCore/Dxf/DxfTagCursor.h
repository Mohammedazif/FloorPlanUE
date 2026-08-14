#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfTagReader.h"

#include <string>

namespace FloorPlan::Dxf
{
    /// One-tag lookahead over a tag reader, with the section and entity boundary tests.
    class DxfTagCursor
    {
    public:
        explicit DxfTagCursor(DxfTagReader& reader);

        bool Advance();

        /// Advances until the next group-code-zero tag, or the stream ends.
        bool SkipToNextStart();

        /// Consumes the remainder of the current record, stopping on the next start tag.
        void SkipRecord();

        const DxfTag& Tag() const { return Current; }

        bool Valid() const { return HasCurrent; }

        bool IsStart() const { return HasCurrent && Current.IsStart(); }

        bool IsStartOf(const char* name) const { return HasCurrent && Current.IsStartOf(name); }

        bool AtSectionEnd() const { return IsStartOf("ENDSEC") || IsStartOf("EOF"); }

        bool Failed() const { return FailureState.IsFailure(); }

        const Diagnostic& Failure() const { return FailureState; }

        bool SawEndOfFile() const { return Reader.SawEndOfFile(); }

        bool Fail(DiagnosticCode code, std::string message);

    private:
        DxfTagReader& Reader;
        DxfTag Current;
        bool HasCurrent = false;
        bool Exhausted = false;
        Diagnostic FailureState;
    };
}
