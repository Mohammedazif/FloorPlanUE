#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfTag.h"

namespace FloorPlan::Dxf
{
    /// Streams group-code/value pairs from a DXF byte buffer, one tag at a time.
    class DxfTagReader
    {
    public:
        virtual ~DxfTagReader() = default;

        DxfTagReader(const DxfTagReader&) = delete;
        DxfTagReader& operator=(const DxfTagReader&) = delete;

        /// Returns false at clean end of input and on failure; check Failure() to distinguish.
        virtual bool Next(DxfTag& tag) = 0;

        const Diagnostic& Failure() const { return FailureState; }

        bool Failed() const { return FailureState.IsFailure(); }

        /// True once the terminating (0, "EOF") tag has been consumed.
        bool SawEndOfFile() const { return EndOfFileSeen; }

        /// True when the reader stopped exactly at the end of the buffer.
        virtual bool ConsumedEntireInput() const = 0;

    protected:
        DxfTagReader() = default;

        bool Fail(DiagnosticCode code, std::string message, std::size_t line,
                  std::size_t byteOffset)
        {
            FailureState.Code = code;
            FailureState.Message = std::move(message);
            FailureState.LineNumber = line;
            FailureState.ByteOffset = byteOffset;
            return false;
        }

        Diagnostic FailureState;
        bool EndOfFileSeen = false;
    };
}
