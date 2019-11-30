// Vita3K emulator project
// Copyright (C) 2018 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <crypto/aes.h>
#include <host/pup_types.h>
#include <miniz.h>
#include <util/fs.h>

#include <fstream>

// Credits to TeamMolecule for their original work on this https://github.com/TeamMolecule/sceutils

static const std::map<int, std::string> PUP_TYPES = {
    { 0x100, "version.txt" },
    { 0x101, "license.xml" },
    { 0x200, "psp2swu.self" },
    { 0x204, "cui_setupper.self" },
    { 0x400, "package_scewm.wm" },
    { 0x401, "package_sceas.as" },
    { 0x2005, "UpdaterES1.CpUp" },
    { 0x2006, "UpdaterES2.CpUp" },
};

static const char *FSTYPE[] = {
    "unknown0",
    "os0",
    "unknown2",
    "unknown3",
    "vs0_chmod",
    "unknown5",
    "unknown6",
    "unknown7",
    "pervasive8",
    "boot_slb2",
    "vs0",
    "devkit_cp",
    "motionC",
    "bbmc",
    "unknownE",
    "motionF",
    "touch10",
    "touch11",
    "syscon12",
    "syscon13",
    "pervasive14",
    "unknown15",
    "vs0_tarpatch",
    "sa0",
    "pd0",
    "pervasive19",
    "unknown1A",
    "psp_emulist",
};

static std::string decompress_segments(const std::vector<uint8_t> &decrypted_data, const uint64_t &size) {
    mz_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = 0;
    stream.next_in = Z_NULL;
    if (mz_inflateInit(&stream) != MZ_OK) {
        LOG_ERROR("inflateInit failed while decompressing");
        return "";
    }

    const std::string compressed_data((char *)&decrypted_data[0], size);
    stream.next_in = (Bytef *)compressed_data.data();
    stream.avail_in = compressed_data.size();

    int ret = 0;
    char outbuffer[4096];
    std::string decompressed_data;

    do {
        stream.next_out = reinterpret_cast<Bytef *>(outbuffer);
        stream.avail_out = sizeof(outbuffer);

        ret = mz_inflate(&stream, 0);

        if (decompressed_data.size() < stream.total_out) {
            decompressed_data.append(outbuffer, stream.total_out - decompressed_data.size());
        }
    } while (ret == MZ_OK);

    mz_inflateEnd(&stream);

    if (ret != MZ_STREAM_END) {
        LOG_ERROR("Exception during zlib decompression: ({}) {}", ret, stream.msg);
        return "";
    }
    return decompressed_data;
}

static void self2elf(const std::string &infile, const std::string &outfile, KeyStore &SCE_KEYS, const int klictxt) {
    std::ifstream filein(infile, std::ios::binary);
    std::ofstream fileout(outfile, std::ios::binary);

    int npdrmtype = 0;

    char sceheaderbuffer[SceHeader::Size];
    char selfheaderbuffer[SelfHeader::Size];
    char appinfobuffer[AppInfoHeader::Size];
    char verinfobuffer[SceVersionInfo::Size];
    char controlinfobuffer[SceControlInfo::Size];

    filein.read(sceheaderbuffer, SceHeader::Size);
    filein.read(selfheaderbuffer, SelfHeader::Size);

    const SceHeader sce_hdr = SceHeader(sceheaderbuffer);
    const SelfHeader self_hdr = SelfHeader(selfheaderbuffer);

    filein.seekg(self_hdr.appinfo_offset);
    filein.read(appinfobuffer, AppInfoHeader::Size);

    const AppInfoHeader appinfo_hdr = AppInfoHeader(appinfobuffer);

    filein.seekg(self_hdr.sceversion_offset);
    filein.read(verinfobuffer, SceVersionInfo::Size);

    const SceVersionInfo verinfo_hdr = SceVersionInfo(verinfobuffer);

    filein.seekg(self_hdr.controlinfo_offset);
    filein.read(controlinfobuffer, SceControlInfo::Size);

    SceControlInfo controlinfo_hdr = SceControlInfo(controlinfobuffer);
    auto ci_off = SceControlInfo::Size;

    if (controlinfo_hdr.type == ControlType::DIGEST_SHA256) {
        filein.seekg(self_hdr.controlinfo_offset + ci_off);
        ci_off += SceControlInfoDigest256::Size;
        char controldigest256buffer[SceControlInfoDigest256::Size];
        filein.read(controldigest256buffer, SceControlInfoDigest256::Size);
        const SceControlInfoDigest256 controldigest256 = SceControlInfoDigest256(controldigest256buffer);
    }
    filein.seekg(self_hdr.controlinfo_offset + ci_off);
    filein.read(controlinfobuffer, SceControlInfo::Size);
    controlinfo_hdr = SceControlInfo(controlinfobuffer);
    ci_off += SceControlInfo::Size;

    if (controlinfo_hdr.type == ControlType::NPDRM_VITA) {
        filein.seekg(self_hdr.controlinfo_offset + ci_off);
        ci_off += SceControlInfoDRM::Size;
        char controlnpdrmbuffer[SceControlInfoDRM::Size];
        filein.read(controlnpdrmbuffer, SceControlInfoDRM::Size);
        const SceControlInfoDRM controlnpdrm = SceControlInfoDRM(controlnpdrmbuffer);
        npdrmtype = controlnpdrm.npdrm_type;
    }

    filein.seekg(self_hdr.elf_offset);
    char dat[ElfHeader::Size];
    filein.read(dat, ElfHeader::Size);
    fileout.write(dat, ElfHeader::Size);

    const ElfHeader elf_hdr = ElfHeader(dat);
    std::vector<ElfPhdr> elf_phdrs;
    std::vector<SegmentInfo> segment_infos;
    bool encrypted = false;
    uint64_t at = ElfHeader::Size;

    for (uint16_t i = 0; i < elf_hdr.e_phnum; i++) {
        filein.seekg(self_hdr.phdr_offset + i * ElfPhdr::Size);
        char dat[ElfPhdr::Size];
        filein.read(dat, ElfPhdr::Size);
        const ElfPhdr phdr = ElfPhdr(dat);
        elf_phdrs.push_back(phdr);
        fileout.write(dat, ElfPhdr::Size);
        at += ElfPhdr::Size;

        filein.seekg(self_hdr.segment_info_offset + i * SegmentInfo::Size);
        char segmentinfobuffer[SegmentInfo::Size];
        filein.read(segmentinfobuffer, SegmentInfo::Size);
        const SegmentInfo segment_info = SegmentInfo(segmentinfobuffer);
        segment_infos.push_back(segment_info);

        if (segment_info.plaintext == SecureBool::NO)
            encrypted = true;
    }

    std::vector<SceSegment> scesegs;

    if (encrypted) {
        scesegs = get_segments(filein, sce_hdr, SCE_KEYS, appinfo_hdr.sys_version, appinfo_hdr.self_type, npdrmtype, klictxt);
    }

    for (uint16_t i = 0; i < elf_hdr.e_phnum; i++) {
        int idx = 0;

        if (!scesegs.empty())
            idx = scesegs[i].idx;
        else
            idx = i;
        if (elf_phdrs[idx].p_filesz == 0)
            continue;

        const int pad_len = elf_phdrs[idx].p_offset - at;
        if (pad_len < 0)
            LOG_ERROR("ELF p_offset Invalid");

        std::vector<char> padding;
        for (int i = 0; i < pad_len; i++) {
            padding.push_back('\0');
        }

        fileout.write(padding.data(), pad_len);

        at += pad_len;

        filein.seekg(segment_infos[idx].offset);
        std::vector<unsigned char> dat(segment_infos[idx].size);
        filein.read((char *)&dat[0], segment_infos[idx].size);

        std::vector<unsigned char> decrypted_data(segment_infos[idx].size);
        if (segment_infos[idx].plaintext == SecureBool::NO) {
            aes_context aes_ctx;
            aes_setkey_enc(&aes_ctx, (unsigned char *)scesegs[i].key.c_str(), 128);
            size_t ctr_nc_off = 0;
            unsigned char ctr_stream_block[0x10];
            aes_crypt_ctr(&aes_ctx, segment_infos[idx].size, &ctr_nc_off, (unsigned char *)scesegs[i].iv.c_str(), ctr_stream_block, &dat[0], &decrypted_data[0]);
        }

        if (segment_infos[idx].compressed == SecureBool::YES) {
            const std::string decompressed_data = decompress_segments(decrypted_data, segment_infos[idx].size);
            fileout.write(decompressed_data.c_str(), decompressed_data.length());
            at += decompressed_data.length();
        } else {
            fileout.write((char *)&decrypted_data[0], segment_infos[idx].size);
            at += segment_infos[idx].size;
        }
    }
    filein.close();
    fileout.close();
}

static std::string make_filename(unsigned char *hdr, int64_t filetype) {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t flags = 0;
    uint32_t moffs = 0;
    uint64_t metaoffs = 0;
    memcpy(&magic, &hdr[0], 4);
    memcpy(&version, &hdr[4], 4);
    memcpy(&flags, &hdr[8], 4);
    memcpy(&moffs, &hdr[12], 4);
    memcpy(&metaoffs, &hdr[16], 8);

    if (magic == SCE_MAGIC && version == 3 && flags == 0x30040) {
        std::vector meta = std::vector<unsigned char>(&hdr[0] + metaoffs, &hdr[0] + (HEADER_LENGTH - metaoffs));
        char t = 0;
        memcpy(&t, &meta[4], 1);

        static int typecount = 0;

        if (t < 0x1C) { // 0x1C is the file seperator
            std::string name = fmt::format("{}-{:0>2}.pkg", FSTYPE[t], typecount);
            typecount++;
            return name;
        }
    }
    return fmt::format("unknown-0x{:X}.pkg", filetype);
}

static void extract_pup_files(const std::string &pup, const std::string &output) {
    constexpr int SCEUF_HEADER_SIZE = 0x80;
    constexpr int SCEUF_FILEREC_SIZE = 0x20;
    std::ifstream infile(pup, std::ios::binary);
    char header[SCEUF_HEADER_SIZE];
    infile.read(header, SCEUF_HEADER_SIZE);

    if (strncmp(header, "SCEUF", 5) != 0) {
        LOG_ERROR("Invalid PUP");
        return;
    }

    uint32_t cnt = 0;
    uint32_t pup_version = 0;
    uint32_t firmware_version = 0;
    uint32_t build_number = 0;
    memcpy(&cnt, &header[0x18], 4);
    memcpy(&pup_version, &header[8], 4);
    memcpy(&firmware_version, &header[0x10], 4);
    memcpy(&build_number, &header[0x14], 4);

    LOG_INFO("PUP Version: 0x{:0}", pup_version);
    LOG_INFO("Firmware Version: 0x{:X}", firmware_version);
    LOG_INFO("Build Number: {:0}", build_number);
    LOG_INFO("Number Of Files: {}", cnt);

    for (uint32_t x = 0; x < cnt; x++) {
        infile.seekg(SCEUF_HEADER_SIZE + x * SCEUF_FILEREC_SIZE);
        char rec[SCEUF_FILEREC_SIZE];
        infile.read(rec, SCEUF_FILEREC_SIZE);

        uint64_t filetype = 0;
        uint64_t offset = 0;
        uint64_t length = 0;
        uint64_t flags = 0;

        memcpy(&filetype, &rec[0], 8);
        memcpy(&offset, &rec[8], 8);
        memcpy(&length, &rec[16], 8);
        memcpy(&flags, &rec[24], 8);

        std::string filename = "";
        if (PUP_TYPES.count(filetype)) {
            filename = PUP_TYPES.at(filetype);
        } else {
            infile.seekg(offset);
            char hdr[HEADER_LENGTH];
            infile.read(hdr, HEADER_LENGTH);
            filename = make_filename((unsigned char *)hdr, filetype);
        }

        std::ofstream outfile(fmt::format("{}/{}", output, filename), std::ios::binary);
        infile.seekg(offset);
        std::vector<char> buffer(length);
        infile.read(&buffer[0], length);
        outfile.write(&buffer[0], length);

        outfile.close();
    }
    infile.close();
}

static void decrypt_segments(std::ifstream &infile, const std::string &outdir, const std::string &filename, KeyStore &SCE_KEYS) {
    char sceheaderbuffer[SceHeader::Size];
    infile.read(sceheaderbuffer, SceHeader::Size);
    const SceHeader sce_hdr = SceHeader(sceheaderbuffer);

    const auto sysver = std::get<0>(get_key_type(infile, sce_hdr));
    const SelfType selftype = std::get<1>(get_key_type(infile, sce_hdr));

    const auto scesegs = get_segments(infile, sce_hdr, SCE_KEYS, sysver, selftype);
    for (const auto &sceseg : scesegs) {
        std::ofstream outfile(fmt::format("{}/{}.seg02", outdir, filename), std::ios::binary);
        infile.seekg(sceseg.offset);
        std::vector<unsigned char> encrypted_data(sceseg.size);
        infile.read((char *)&encrypted_data[0], sceseg.size);
        aes_context aes_ctx;
        aes_setkey_enc(&aes_ctx, (unsigned char *)sceseg.key.c_str(), 128);
        size_t ctr_nc_off = 0;
        unsigned char ctr_stream_block[0x10];
        std::vector<unsigned char> decrypted_data(sceseg.size);
        aes_crypt_ctr(&aes_ctx, sceseg.size, &ctr_nc_off, (unsigned char *)sceseg.iv.c_str(), ctr_stream_block, &encrypted_data[0], &decrypted_data[0]);
        if (sceseg.compressed) {
            const std::string decompressed_data = decompress_segments(decrypted_data, sceseg.size);
            outfile.write(decompressed_data.c_str(), decompressed_data.size());
        } else {
            outfile.write((char *)&decrypted_data[0], sceseg.size);
        }
        outfile.close();
    }
};

static void join_files(const std::string &path, const std::string &filename, const std::string &output) {
    std::vector<std::string> files;

    for (auto &p : fs::directory_iterator(path)) {
        if (p.path().filename().string().substr(0, 4) == filename) {
            files.push_back(p.path().string());
        }
    }

    std::sort(files.begin(), files.end());

    std::ofstream fileout(output, std::ios::binary);
    for (const auto &file : files) {
        std::ifstream filein(file, std::ios::binary);
        std::vector<char> buffer(fs::file_size(file));
        filein.read(&buffer[0], fs::file_size(file));
        fileout.write(&buffer[0], fs::file_size(file));
        filein.close();
        fs::remove(file);
    }
    fileout.close();
}

static void decrypt_pup_packages(const std::string &src, const std::string &dest, KeyStore &SCE_KEYS) {
    std::vector<std::string> pkgfiles;

    for (const auto &p : fs::directory_iterator(src)) {
        if (p.path().filename().extension().string() == ".pkg")
            pkgfiles.push_back(p.path().filename().string());
    }

    for (const auto &filename : pkgfiles) {
        const std::string &filepath = fmt::format("{}/{}", src, filename);
        std::ifstream infile(filepath, std::ios::binary);
        decrypt_segments(infile, dest, filename, SCE_KEYS);
        infile.close();
    }

    join_files(dest, "os0-", dest + "/os0.img");
    join_files(dest, "vs0-", dest + "/vs0.img");
    join_files(dest, "sa0-", dest + "/sa0.img");
}

void install_pup(const std::string &pup, const std::string &pref_path) {
    if (fs::exists(pref_path + "/PUP_DEC")) {
        LOG_WARN("Path already exists, deleting it and reinstalling");
        fs::remove_all(pref_path + "/PUP_DEC");
    }

    LOG_INFO("Extracting {} to {}", pup, pref_path + "/PUP_DEC");

    fs::create_directory(pref_path + "/PUP_DEC");
    const auto pup_dest = fmt::format("{}/PUP", pref_path + "/PUP_DEC");
    fs::create_directory(pup_dest);

    extract_pup_files(pup, pup_dest);

    const auto pup_dec = fmt::format("{}/PUP_dec", pref_path + "/PUP_DEC");
    fs::create_directory(pup_dec);

    KeyStore SCE_KEYS;
    register_keys(SCE_KEYS);

    decrypt_pup_packages(pup_dest, pup_dec, SCE_KEYS);

    if (fs::file_size(pup_dec + "/os0.img") > 0) {
        extract_fat(pup_dec, "os0.img", pref_path);
    }
    if (fs::file_size(pup_dec + "/sa0.img") > 0) {
        extract_fat(pup_dec, "sa0.img", pref_path);
    }
    if (fs::file_size(pup_dec + "/vs0.img") > 0) {
        extract_fat(pup_dec, "vs0.img", pref_path);
        for (const auto &file : fs::directory_iterator(pref_path + "/vs0/sys/external")) {
            self2elf(file.path().string(), file.path().string() + "elf", SCE_KEYS, 0);
            fs::rename(file.path().string() + "elf", file.path().string());
            make_fself(file.path().string(), file.path().string() + "fself");
            fs::rename(file.path().string() + "fself", file.path().string());
        }
    }
}
