#pragma once

#include <cstddef>
#include <string>

namespace FloorPlan
{
    enum class DiagnosticCode
    {
        None,
        FileEmpty,
        FileTooLarge,
        FileUnreadable,
        NotDxf,
        UnexpectedEndOfInput,
        MissingEndOfFileMarker,
        TrailingBytesAfterEndOfFile,
        LineTooLong,
        StringTooLong,
        InvalidGroupCode,
        GroupCodeOutOfRange,
        MalformedInteger,
        MalformedReal,
        NonFiniteValue,
        ValueOutOfRange,
        TagLimitExceeded,
        EntityLimitExceeded,
        VertexLimitExceeded,
        BlockLimitExceeded,
        LayerLimitExceeded,
        DeclaredCountImplausible,
        UnterminatedSection,
        UnterminatedEntity,
        BlockRecursionTooDeep,
        BlockReferenceCycle,
        UnknownBlockReference,
        UnsupportedBinaryEncoding,
        BinaryChunkTooLong,
        ArrangementTooComplex,
        WeldChainTooLong,
        SnapDidNotConverge,
        WallLimitExceeded,
        RoomLimitExceeded,
        OpeningLimitExceeded,
        IdentityCollision
    };

    /// A parse or geometry failure, carrying the location that produced it.
    struct Diagnostic
    {
        DiagnosticCode Code = DiagnosticCode::None;
        std::string Message;
        std::size_t ByteOffset = 0;
        std::size_t LineNumber = 0;

        bool IsFailure() const { return Code != DiagnosticCode::None; }
    };

    const char* ToString(DiagnosticCode code);

    std::string Format(const Diagnostic& diagnostic);
}
