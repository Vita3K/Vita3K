// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#include <packages/sce_types.h>
#include <self.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

static bool fits(size_t size, uint64_t offset, uint64_t length) {
    return offset <= size && length <= size - offset;
}

static int fail(const char *message) {
    std::cerr << message << '\n';
    return 1;
}

static int extract(const fs::path &input_path, const fs::path &output_path) {
    std::error_code error;
    const bool output_exists = fs::exists(output_path, error);
    if (error)
        return fail("Cannot check the output path.");
    if (output_exists)
        return fail("Output already exists; refusing to overwrite it.");
    std::ifstream input(input_path, std::ios::binary);
    if (!input)
        return fail("Cannot open input file.");
    const std::vector<uint8_t> data{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    if (input.bad() || data.size() < sizeof(SCE_header))
        return fail("Cannot read a complete SELF header.");

    SCE_header header{};
    std::memcpy(&header, data.data(), sizeof(header));
    if (header.magic != SCE_MAGIC || header.version != 3 || header.header_type != 1)
        return fail("Input is not a supported Vita SELF.");
    if (!fits(data.size(), header.appinfo_offset, AppInfoHeader::Size)
        || !fits(data.size(), header.elf_offset, ElfHeader::Size)
        || !fits(data.size(), header.controlinfo_offset, 2 * SceControlInfo::Size + SceControlInfoDigest256::Size)
        || header.header_len > data.size() || header.metadata_offset > header.header_len
        || header.header_len - header.metadata_offset < 48 + MetadataInfo::Size + MetadataHeader::Size)
        return fail("SELF header contains invalid offsets.");

    const AppInfoHeader app(reinterpret_cast<const char *>(data.data() + header.appinfo_offset));
    if (app.self_type != SelfType::KERNEL && app.self_type != SelfType::USER)
        return fail("Only kernel and user firmware modules are supported; game SELF files need a license key.");
    const ElfHeader elf(reinterpret_cast<const char *>(data.data() + header.elf_offset));
    if (std::memcmp(elf.e_ident_1, "\x7f"
                                   "ELF\x01\x01\x01",
            7)
            != 0
        || elf.e_machine != 40 || elf.e_phnum == 0
        || !fits(data.size(), header.phdr_offset, uint64_t(elf.e_phnum) * ElfPhdr::Size)
        || !fits(data.size(), header.section_info_offset, uint64_t(elf.e_phnum) * SegmentInfo::Size))
        return fail("SELF contains invalid ARM ELF headers.");

    for (uint16_t i = 0; i < elf.e_phnum; ++i) {
        const SegmentInfo segment(reinterpret_cast<const char *>(data.data() + header.section_info_offset + i * SegmentInfo::Size));
        const ElfPhdr program(reinterpret_cast<const char *>(data.data() + header.phdr_offset + i * ElfPhdr::Size));
        if (!fits(data.size(), segment.offset, segment.size) || segment.plaintext != SecureBool::NO)
            return fail("Only encrypted firmware SELF segments are supported by this extractor.");
        if (!fits(header.elf_filesize, program.p_offset, program.p_filesz))
            return fail("ELF segment extends beyond the declared file size.");
    }

    const SceHeader sce(reinterpret_cast<const char *>(data.data()));
    KeyStore keys;
    register_keys(keys, 1);
    const auto key = keys.get(KeyType::METADATA, sce.sce_type, app.sys_version, sce.key_revision, app.self_type);
    if (key.key.empty() || key.iv.empty())
        return fail("No built-in metadata key matches this firmware module.");

    const std::array<uint8_t, 16> no_license{};
    const std::vector<uint8_t> decrypted = decrypt_fself(data, no_license.data());
    if (decrypted.size() < sizeof(SCE_header))
        return fail("SELF decryption failed.");
    SCE_header result{};
    std::memcpy(&result, decrypted.data(), sizeof(result));
    if (!fits(decrypted.size(), result.header_len, result.elf_filesize) || result.elf_filesize < ElfHeader::Size)
        return fail("Decrypted ELF has an invalid size.");
    const uint8_t *elf_data = decrypted.data() + result.header_len;
    if (std::memcmp(elf_data, "\x7f"
                              "ELF\x01\x01\x01",
            7)
        != 0)
        return fail("Decrypted output is not an ARM ELF.");

    std::ofstream output(output_path, std::ios::binary);
    if (!output)
        return fail("Cannot create output file.");
    output.write(reinterpret_cast<const char *>(elf_data), static_cast<std::streamsize>(result.elf_filesize));
    output.close();
    if (!output)
        return fail("Failed to write the complete output file.");
    std::cout << "ELF extracted successfully.\n";
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
#else
int main(int argc, char **argv) {
#endif
    if (argc != 3)
        return fail("Usage: vita3k-self-elf <encrypted firmware SELF> <output ELF>");
    return extract(fs::path(argv[1]), fs::path(argv[2]));
}
