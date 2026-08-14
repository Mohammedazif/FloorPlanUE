#include "Diagnostic.h"

namespace FloorPlan
{
    const char* ToString(DiagnosticCode code)
    {
        switch (code)
        {
        case DiagnosticCode::None: return "None";
        case DiagnosticCode::FileEmpty: return "FileEmpty";
        case DiagnosticCode::FileTooLarge: return "FileTooLarge";
        case DiagnosticCode::FileUnreadable: return "FileUnreadable";
        case DiagnosticCode::NotDxf: return "NotDxf";
        case DiagnosticCode::UnexpectedEndOfInput: return "UnexpectedEndOfInput";
        case DiagnosticCode::MissingEndOfFileMarker: return "MissingEndOfFileMarker";
        case DiagnosticCode::TrailingBytesAfterEndOfFile: return "TrailingBytesAfterEndOfFile";
        case DiagnosticCode::LineTooLong: return "LineTooLong";
        case DiagnosticCode::StringTooLong: return "StringTooLong";
        case DiagnosticCode::InvalidGroupCode: return "InvalidGroupCode";
        case DiagnosticCode::GroupCodeOutOfRange: return "GroupCodeOutOfRange";
        case DiagnosticCode::MalformedInteger: return "MalformedInteger";
        case DiagnosticCode::MalformedReal: return "MalformedReal";
        case DiagnosticCode::NonFiniteValue: return "NonFiniteValue";
        case DiagnosticCode::ValueOutOfRange: return "ValueOutOfRange";
        case DiagnosticCode::TagLimitExceeded: return "TagLimitExceeded";
        case DiagnosticCode::EntityLimitExceeded: return "EntityLimitExceeded";
        case DiagnosticCode::VertexLimitExceeded: return "VertexLimitExceeded";
        case DiagnosticCode::BlockLimitExceeded: return "BlockLimitExceeded";
        case DiagnosticCode::LayerLimitExceeded: return "LayerLimitExceeded";
        case DiagnosticCode::DeclaredCountImplausible: return "DeclaredCountImplausible";
        case DiagnosticCode::UnterminatedSection: return "UnterminatedSection";
        case DiagnosticCode::UnterminatedEntity: return "UnterminatedEntity";
        case DiagnosticCode::BlockRecursionTooDeep: return "BlockRecursionTooDeep";
        case DiagnosticCode::BlockReferenceCycle: return "BlockReferenceCycle";
        case DiagnosticCode::UnknownBlockReference: return "UnknownBlockReference";
        case DiagnosticCode::UnsupportedBinaryEncoding: return "UnsupportedBinaryEncoding";
        case DiagnosticCode::BinaryChunkTooLong: return "BinaryChunkTooLong";
        case DiagnosticCode::ArrangementTooComplex: return "ArrangementTooComplex";
        case DiagnosticCode::WeldChainTooLong: return "WeldChainTooLong";
        case DiagnosticCode::SnapDidNotConverge: return "SnapDidNotConverge";
        case DiagnosticCode::WallLimitExceeded: return "WallLimitExceeded";
        case DiagnosticCode::RoomLimitExceeded: return "RoomLimitExceeded";
        case DiagnosticCode::OpeningLimitExceeded: return "OpeningLimitExceeded";
        case DiagnosticCode::IdentityCollision: return "IdentityCollision";
        }
        return "Unknown";
    }

    std::string Format(const Diagnostic& diagnostic)
    {
        std::string text = ToString(diagnostic.Code);
        if (diagnostic.LineNumber > 0)
        {
            text += " at line " + std::to_string(diagnostic.LineNumber);
        }
        if (diagnostic.ByteOffset > 0)
        {
            text += " (byte " + std::to_string(diagnostic.ByteOffset) + ")";
        }
        if (!diagnostic.Message.empty())
        {
            text += ": " + diagnostic.Message;
        }
        return text;
    }
}
