#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace InventoryArchive {

constexpr std::uint32_t MaxRecords = 16384;

inline std::uint32_t ReadLittleEndian(std::string_view data, std::size_t offset, unsigned count) {
    std::uint32_t value = 0;
    for (unsigned i = 0; i < count; ++i) {
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + i])) << (8 * i);
    }
    return value;
}

inline bool ExtractRecords(std::string_view file, std::string& records) {
    // G2NRinventory writes a BIN_SAFE v2 archive with an integer recordCount
    // followed by that many strings sharing the record key.
    constexpr std::string_view signature = "ZenGin Archive\nver 1\nzCArchiverBinSafe\nBIN_SAFE\n";
    if (file.substr(0, signature.size()) != signature) return false;
    const auto endHeader = file.find("\nEND\n", signature.size() - 1);
    if (endHeader == std::string_view::npos) return false;
    const auto header = endHeader + 5;
    if (file.size() - header < 12 || ReadLittleEndian(file, header, 4) != 2) return false;

    const auto table = ReadLittleEndian(file, header + 8, 4);
    auto position = header + 12;
    if (table > file.size() || table < position || table - position < 10) return false;
    if (file[position] != 0x12 || file[position + 5] != 0x02) return false;
    const auto countKey = ReadLittleEndian(file, position + 1, 4);
    const auto count = ReadLittleEndian(file, position + 6, 4);
    if (!count || count > MaxRecords) return false;
    position += 10;

    std::string decoded;
    const auto recordKey = table - position >= 8 ? ReadLittleEndian(file, position + 1, 4) : 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        if (table - position < 8 || file[position] != 0x12 || file[position + 5] != 0x01
            || ReadLittleEndian(file, position + 1, 4) != recordKey) return false;
        const auto length = ReadLittleEndian(file, position + 6, 2);
        position += 8;
        if (!length || length > table - position) return false;
        const auto line = file.substr(position, length);
        if (line.find_first_of(std::string_view("\r\n\0", 3)) != std::string_view::npos) return false;
        decoded.append(line);
        decoded.push_back('\n');
        position += length;
    }
    if (position != table || file.size() - position < 4 || ReadLittleEndian(file, position, 4) != 2) return false;
    position += 4;
    bool foundCountKey = false;
    bool foundRecordKey = false;
    for (int i = 0; i < 2; ++i) {
        if (file.size() - position < 8) return false;
        const auto length = ReadLittleEndian(file, position, 2);
        const auto index = ReadLittleEndian(file, position + 2, 2);
        position += 8;
        if (length > file.size() - position) return false;
        const auto name = file.substr(position, length);
        foundCountKey |= index == countKey && name == "recordCount";
        foundRecordKey |= index == recordKey && name == "record";
        position += length;
    }
    if (!foundCountKey || !foundRecordKey || position != file.size()) return false;
    records.swap(decoded);
    return true;
}

} // namespace InventoryArchive
