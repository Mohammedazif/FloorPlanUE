#include "Dxf/DxfSource.h"

#include "Dxf/DxfAsciiTagReader.h"
#include "Dxf/DxfBinaryTagReader.h"
#include "FloorPlanLimits.h"

#include <cstdio>

namespace FloorPlan::Dxf
{
    DxfSource DxfSource::FromBytes(std::vector<std::uint8_t> bytes)
    {
        DxfSource source;
        if (bytes.empty())
        {
            source.FailureState.Code = DiagnosticCode::FileEmpty;
            source.FailureState.Message = "zero bytes";
            return source;
        }
        if (bytes.size() > Limits::MaxFileBytes)
        {
            source.FailureState.Code = DiagnosticCode::FileTooLarge;
            source.FailureState.Message = std::to_string(bytes.size()) + " bytes";
            return source;
        }
        source.Binary = DxfBinaryTagReader::HasSentinel(bytes.data(), bytes.size());
        source.Bytes = std::move(bytes);
        return source;
    }

    DxfSource DxfSource::FromFile(const std::string& path)
    {
        DxfSource source;
        std::FILE* handle = std::fopen(path.c_str(), "rb");
        if (handle == nullptr)
        {
            source.FailureState.Code = DiagnosticCode::FileUnreadable;
            source.FailureState.Message = path;
            return source;
        }

        if (std::fseek(handle, 0, SEEK_END) != 0)
        {
            std::fclose(handle);
            source.FailureState.Code = DiagnosticCode::FileUnreadable;
            source.FailureState.Message = path;
            return source;
        }
        const long length = std::ftell(handle);
        if (length < 0)
        {
            std::fclose(handle);
            source.FailureState.Code = DiagnosticCode::FileUnreadable;
            source.FailureState.Message = path;
            return source;
        }
        const std::size_t size = static_cast<std::size_t>(length);
        if (size > Limits::MaxFileBytes)
        {
            std::fclose(handle);
            source.FailureState.Code = DiagnosticCode::FileTooLarge;
            source.FailureState.Message = std::to_string(size) + " bytes";
            return source;
        }
        std::rewind(handle);

        std::vector<std::uint8_t> bytes(size);
        const std::size_t read = size == 0 ? 0 : std::fread(bytes.data(), 1, size, handle);
        std::fclose(handle);
        if (read != size)
        {
            source.FailureState.Code = DiagnosticCode::FileUnreadable;
            source.FailureState.Message = path;
            return source;
        }
        return FromBytes(std::move(bytes));
    }

    std::unique_ptr<DxfTagReader> DxfSource::OpenReader() const
    {
        if (!IsValid())
        {
            return nullptr;
        }
        if (Binary)
        {
            return std::make_unique<DxfBinaryTagReader>(Bytes.data(), Bytes.size());
        }
        return std::make_unique<DxfAsciiTagReader>(Bytes.data(), Bytes.size());
    }
}
