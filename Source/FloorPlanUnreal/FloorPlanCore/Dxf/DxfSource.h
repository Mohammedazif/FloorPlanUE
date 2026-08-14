#pragma once

#include "Diagnostic.h"
#include "Dxf/DxfTagReader.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace FloorPlan::Dxf
{
    /// Owns a DXF byte buffer and hands out a reader matched to its ASCII or binary encoding.
    class DxfSource
    {
    public:
        static DxfSource FromFile(const std::string& path);
        static DxfSource FromBytes(std::vector<std::uint8_t> bytes);

        bool IsValid() const { return !FailureState.IsFailure(); }

        const Diagnostic& Failure() const { return FailureState; }

        bool IsBinary() const { return Binary; }

        std::size_t SizeInBytes() const { return Bytes.size(); }

        std::unique_ptr<DxfTagReader> OpenReader() const;

    private:
        DxfSource() = default;

        std::vector<std::uint8_t> Bytes;
        Diagnostic FailureState;
        bool Binary = false;
    };
}
