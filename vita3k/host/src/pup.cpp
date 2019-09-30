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
#include <util/fs.h>
#include <util/string_utils.h>
#include <miniz.h>

#include <fstream>

std::map<int, std::string> pup_types = {
	{0x100, "version.txt"},
	{0x101, "license.xml"},
	{0x200, "psp2swu.self"},
	{0x204, "cui_setupper.self"},
	{0x400, "package_scewm.wm"},
	{0x401, "package_sceas.as"},
	{0x2005, "UpdaterES1.CpUp"},
	{0x2006, "UpdaterES2.CpUp"},
};

const std::string FSTYPE[] = {
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

void self2elf(std::string infile, std::string outfile, KeyStore &SCE_KEYS, int klictxt) {
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

	SceHeader sce = SceHeader(sceheaderbuffer);
    SelfHeader self_hdr = SelfHeader(selfheaderbuffer);

	filein.seekg(self_hdr.appinfo_offset);
    filein.read(appinfobuffer, AppInfoHeader::Size);

	AppInfoHeader appinfo_hdr = AppInfoHeader(appinfobuffer);

	filein.seekg(self_hdr.sceversion_offset);
    filein.read(verinfobuffer, SceVersionInfo::Size);

	SceVersionInfo verinfo_hdr = SceVersionInfo(verinfobuffer);

	filein.seekg(self_hdr.controlinfo_offset);
	filein.read(controlinfobuffer, SceControlInfo::Size);

	SceControlInfo controlinfo_hdr = SceControlInfo(controlinfobuffer);
	auto ci_off = SceControlInfo::Size;

	if (controlinfo_hdr.type == ControlType::DIGEST_SHA256) {
		filein.seekg(self_hdr.controlinfo_offset + ci_off);
		ci_off += SceControlInfoDigest256::Size;
		char controldigest256buffer[SceControlInfoDigest256::Size];
		filein.read(controldigest256buffer, SceControlInfoDigest256::Size);
		SceControlInfoDigest256 controldigest256 = SceControlInfoDigest256(controldigest256buffer);
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
		SceControlInfoDRM controlnpdrm = SceControlInfoDRM(controlnpdrmbuffer);
		npdrmtype = controlnpdrm.npdrm_type;
	}

	filein.seekg(self_hdr.elf_offset);
	char dat[ElfHeader::Size];
	filein.read(dat, ElfHeader::Size);
	fileout.write(dat, ElfHeader::Size);

	ElfHeader elf_hdr = ElfHeader(dat);
	std::vector<ElfPhdr> elf_phdrs;
	std::vector<SegmentInfo> segment_infos;
	bool encrypted = false;
	int at = ElfHeader::Size;

	for (int i = 0; i < elf_hdr.e_phnum; i++) {
		filein.seekg(self_hdr.phdr_offset + i * ElfPhdr::Size);
		char dat[ElfPhdr::Size];
		filein.read(dat, ElfPhdr::Size);
		ElfPhdr phdr = ElfPhdr(dat);
		elf_phdrs.push_back(phdr);
		fileout.write(dat, ElfPhdr::Size);
		at += ElfPhdr::Size;

		filein.seekg(self_hdr.segment_info_offset + i * SegmentInfo::Size);
		char segmentinfobuffer[SegmentInfo::Size];
		filein.read(segmentinfobuffer, SegmentInfo::Size);
		SegmentInfo segment_info = SegmentInfo(segmentinfobuffer);
		segment_infos.push_back(segment_info);

		if (segment_info.plaintext == SecureBool::NO)
			encrypted = true;
	}

	std::vector<SceSegment> scesegs;

	if (encrypted) {
		scesegs = get_segments(filein, sce, SCE_KEYS, appinfo_hdr.sys_version, appinfo_hdr.self_type, npdrmtype, klictxt);
	}

	for (int i = 0; i < elf_hdr.e_phnum; i++) {
		int idx;

		if (!scesegs.empty())
			idx = scesegs[i].idx;
		else
			idx = i;
		if (elf_phdrs[idx].p_filesz == 0)
			continue;

		int pad_len = elf_phdrs[idx].p_offset - at;
		if (pad_len < 0)
			LOG_ERROR("ELF p_offset Invalid");

		std::string padding;
		for (int i = 0; i < pad_len; i++) {
			padding += "00";
		}

		auto padding_array = string_utils::string_to_byte_array(padding);

        std::string result(padding_array.begin(), padding_array.end());
		fileout.write(result.c_str(), pad_len);

		at += pad_len;

		filein.seekg(segment_infos[idx].offset);
		unsigned char *dat = new unsigned char[segment_infos[idx].size];
		filein.read((char*)dat, segment_infos[idx].size);

        unsigned char *decrypted_data = new unsigned char[segment_infos[idx].size];
		if (segment_infos[idx].plaintext == SecureBool::NO) {
			aes_context aes_ctx;
			aes_setkey_enc(&aes_ctx, (unsigned char*)scesegs[i].key.c_str(), 128);
			size_t ctr_nc_off = 0;
		    unsigned char ctr_stream_block[0x10];
		    aes_crypt_ctr(&aes_ctx, segment_infos[idx].size, &ctr_nc_off, (unsigned char*)scesegs[i].iv.c_str(), ctr_stream_block, dat, decrypted_data);
		}

		if (segment_infos[idx].compressed == SecureBool::YES) {
			mz_stream stream;
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;
            stream.avail_in = 0;
            stream.next_in = Z_NULL;
            if (mz_inflateInit(&stream) != MZ_OK) {
                LOG_ERROR("inflateInit failed while decompressing");
                return;
            }

            std::string compressed_data((char*)decrypted_data, segment_infos[idx].size);
            stream.next_in = (Bytef *)compressed_data.data();
            stream.avail_in = compressed_data.size();

            int ret;
            char outbuffer[524288];
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
                return;
            }
            fileout.write(decompressed_data.c_str(), decompressed_data.length());
            at += decompressed_data.length();
		}
		else {
			fileout.write((char*)decrypted_data, segment_infos[idx].size);
		    at += segment_infos[idx].size;
		}
	    delete[] dat;
	    delete[] decrypted_data;
	}
	filein.close();
	fileout.close();
}

std::string make_filename(unsigned char* hdr, int64_t filetype) {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t moffs;
    uint64_t metaoffs;
    memcpy(&magic, &hdr[0], 4);
    memcpy(&version, &hdr[4], 4);
    memcpy(&flags, &hdr[8], 4);
    memcpy(&moffs, &hdr[12], 4);
    memcpy(&metaoffs, &hdr[16], 8);

	if (magic == 0x454353 && version == 3 && flags == 0x30040) {
		auto meta = std::vector<unsigned char>(&hdr[0] + metaoffs, &hdr[0] + 0x1000 - metaoffs);
		unsigned char* array = &meta[0];
		char t;
		memcpy(&t, &array[4], 1);

		static int typecount = 0;

		if (t < 0x1C) {
			std::string name = fmt::format("{}-{:0>2}.pkg", FSTYPE[t], typecount);
			typecount++;
			return name;
		}
	}
	return fmt::format("unknown-0x{:X}.pkg", filetype);
}

void extract_pup_files(std::string pup, std::string output) {
    const int SCEUF_HEADER_SIZE = 0x80;
    const int SCEUF_FILEREC_SIZE = 0x20;
	std::ifstream infile(pup, std::ios::binary);
	char header[SCEUF_HEADER_SIZE];
	infile.read(header, SCEUF_HEADER_SIZE);

	for (int i = 0; i < 5; i++) {
		if (header[i] != "SCEUF"[i]) {
			LOG_ERROR("Invalid PUP");
			return;
		}
	}

	uint32_t cnt;
    uint32_t pup_version;
    uint32_t firmware_version;
    uint32_t build_number;
	memcpy(&cnt, &header[0x18], 4);
	memcpy(&pup_version, &header[8], 4);
	memcpy(&firmware_version, &header[0x10], 4);
	memcpy(&build_number, &header[0x14], 4);

	LOG_INFO("PUP Version: 0x{:0}", pup_version);
	LOG_INFO("Firmware Version: 0x{:X}", firmware_version);
	LOG_INFO("Build Number: {:0}", build_number);
	LOG_INFO("Number Of Files: {}", cnt);

	for (auto x = 0; x < cnt; x++) {
		infile.seekg(SCEUF_HEADER_SIZE + x * SCEUF_FILEREC_SIZE);
		char rec[SCEUF_FILEREC_SIZE];
		infile.read(rec, SCEUF_FILEREC_SIZE);

		uint64_t filetype;
		uint64_t offset;
		uint64_t length;
		uint64_t flags;

		memcpy(&filetype, &rec[0], 8);
		memcpy(&offset, &rec[8], 8);
		memcpy(&length, &rec[16], 8);
		memcpy(&flags, &rec[24], 8);

		std::string filename = "";
		if (pup_types.count(filetype)) {
			filename = pup_types.at(filetype);
		} else {
			infile.seekg(offset);
			char hdr[0x1000];
			infile.read(hdr, 0x1000);
			filename = make_filename((unsigned char*)hdr, filetype);
		}

		std::ofstream outfile(fmt::format("{}/{}", output, filename), std::ios::binary);
		infile.seekg(offset);
		char* buffer = new char[length];
		infile.read(buffer, length);
		outfile.write(buffer, length);

		delete[] buffer;
		outfile.close();
	}
	infile.close();
}

void decrypt_segments(std::ifstream &infile, std::string outdir, std::string filename, KeyStore &SCE_KEYS) {
    char sceheaderbuffer[SceHeader::Size];
    infile.read(sceheaderbuffer, SceHeader::Size);
    SceHeader sce = SceHeader(sceheaderbuffer);

    auto sysver = std::get<0>(get_key_type(infile, sce));
    SelfType selftype = std::get<1>(get_key_type(infile, sce));

	auto scesegs = get_segments(infile, sce, SCE_KEYS, sysver, selftype);
	for (auto sceseg : scesegs) {
		std::ofstream outfile(fmt::format("{}/{}.seg02", outdir, filename), std::ios::binary);
		infile.seekg(sceseg.offset);
		unsigned char *encrypted_data = new unsigned char[sceseg.size];
		infile.read((char*)encrypted_data, sceseg.size); //maybe rewrite the casts to static_cast<>() ??
		aes_context aes_ctx;
		aes_setkey_enc(&aes_ctx, (unsigned char*)sceseg.key.c_str(), 128);
		size_t ctr_nc_off = 0;
		unsigned char ctr_stream_block[0x10];
		unsigned char *decrypted_data = new unsigned char[sceseg.size];
		aes_crypt_ctr(&aes_ctx, sceseg.size, &ctr_nc_off, (unsigned char*)sceseg.iv.c_str(), ctr_stream_block, encrypted_data, decrypted_data);
		if (sceseg.compressed) {
            mz_stream stream;
            stream.zalloc = Z_NULL;
            stream.zfree = Z_NULL;
            stream.opaque = Z_NULL;
            stream.avail_in = 0;
            stream.next_in = Z_NULL;
            if (mz_inflateInit(&stream) != MZ_OK) {
                LOG_ERROR("inflateInit failed while decompressing");
                return;
            }

            std::string compressed_data((char*)decrypted_data, sceseg.size);
            stream.next_in = (Bytef *)compressed_data.data();
            stream.avail_in = compressed_data.size();

            int ret;
            char outbuffer[524288];
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
                return;
            }
            outfile.write(decompressed_data.c_str(), decompressed_data.size());
		}
		else {
		    outfile.write((char*)decrypted_data, sceseg.size);
		}
        outfile.close();
		delete[] encrypted_data;
        delete[] decrypted_data;
	}
};

void join_files(std::string path, std::string filename, std::string output) {
    std::vector<std::string> files;

	for (auto &p : fs::directory_iterator(path)) {
        if (p.path().filename().string().substr(0, 4) == filename) {
		    files.push_back(p.path().string());
		}
	}

	std::ofstream fileout(output, std::ios::binary);
	for (auto file : files) {
        std::ifstream filein(file, std::ios::binary);
		char *buffer = new char[fs::file_size(file)];
        filein.read(buffer, fs::file_size(file));
		fileout.write(buffer, fs::file_size(file));
        filein.close();
		fs::remove(file);
	}
	fileout.close();
}

void decrypt_pup_packages(std::string src, std::string dest) {
    std::vector<std::string> pkgfiles;

    KeyStore SCE_KEYS = KeyStore();
    register_keys(SCE_KEYS);

	for (auto &p : fs::directory_iterator(src)) {
        if (p.path().filename().extension().string() == ".pkg")
            pkgfiles.push_back(p.path().filename().string());
    }

	for (auto filename : pkgfiles) {
        auto filepath = fmt::format("{}/{}", src, filename);
		std::ifstream infile(filepath, std::ios::binary);
        try {
            decrypt_segments(infile, dest, filename, SCE_KEYS);
        }
		catch (std::string message) {
            LOG_ERROR("Error: {}", message);
        }
        infile.close();
    }

	join_files(dest, "os0-", dest + "/os0.bin");
	join_files(dest, "vs0-", dest + "/vs0.bin");
	join_files(dest, "sa0-", dest + "/sa0.bin");
}

void install_pup(std::string pup, std::string output) {
	if (fs::exists(output)) {
        LOG_WARN("Path already exists, deleting it and reinstalling");
        fs::remove_all(output);
    }

    LOG_INFO("Extracting {} to {}", pup, output);

	fs::create_directory(output);
    auto pup_dest = fmt::format("{}/PUP", output);
	fs::create_directory(pup_dest);

	extract_pup_files(pup, pup_dest);

	auto pup_dec = fmt::format("{}/PUP_dec", output);
	fs::create_directory(pup_dec);

	decrypt_pup_packages(pup_dest, pup_dec);
}
