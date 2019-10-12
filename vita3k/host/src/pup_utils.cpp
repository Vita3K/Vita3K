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
#include <fat16/fat16.h>
#include <host/pup_types.h>
#include <util/string_utils.h>

#include <self.h>

#include <fstream>

// Credits to TeamMolecule for their original work on this https://github.com/TeamMolecule/sceutils

void register_keys(KeyStore &SCE_KEYS) {
    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SPKG,
        0,
        "2E6F4751D15B06C51F572A9306E52DD7007EA56A31D459EC6D3681AB08625501",
        "B3D541A568751DF8F4833BAB4EFE0537",
        0x00000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::NONE);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SRVK,
        0,
        "4648164DB9E67009456C7CA6F2378835FD678539B36B3DE6F1C604B7D4258141",
        "6EC8AD67993DAE75675F0AFFDE5C41F3",
        0x10300000000,
        0x16920000000,
        SelfType::NONE);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SRVK,
        0,
        "DAE4B0F901E338DEFF3CCDBDEA1E2FDEA9926BB98CB182443CC0C0F7FAE428EE",
        "18D925FA885C7E28A9CFF458C24D8BED",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::NONE);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "9D4E4CE92EA1C4576EB9601EC43EC03AAE8EC324ECF6DE01E918E61D2223EE55",
        "CFEA3CCBA454D3279AD7CB0510431434",
        0x10300000000,
        0x16920000000,
        SelfType::SECURE);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "B1B6FEB39A8BD7A2AC584D435E150C624F560D3EFB03E745C575E0844569E2D0",
        "89B4E6BAB03B03D49BF0FC927FEA8659",
        0x18000000000,
        0x36100000000,
        SelfType::SECURE);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "59AC7F05E115D758201A3F3461BCA0D42BD186F00CFC24263973F622AD9ED30C",
        "A053B00BA4BF880799B4265C6BC064B5",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::SECURE);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "7A7FB1560DCD121CEA5E11B90124B13282752F2D5B95D75036AB3A29BB3BD2AB",
        "6C71642A042A041F1EE3094070B009BE",
        0x10300000000,
        0x16920000000,
        SelfType::BOOT);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "B1B936B512F9A16E51B948622B26F15C53680C77AC332EC25846B839520393EC",
        "90D527BAF7296B5B6A576CFA6B54D266",
        0x18000000000,
        0x36100000000,
        SelfType::BOOT);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "426FD1D33FEBBFAC560B7957B94F445AE5F1DED2AA70F74DB944645DC439122F",
        "995F1364BB9735FA448B18D886150C85",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::BOOT);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "B4AAF62D48FBD898C240308A9773AFE57B8A18D783F0B37932BB21B51386A9A0",
        "8CD162C5C613376F3E4BEA0B8FD5A3D0",
        0x10300000000,
        0x16920000000,
        SelfType::KERNEL);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "849AF7E8DE5B9C28C38CA74963FCF155E0F200FB08185E46CDA87790AAA10D72",
        "88710E219454A3CBF6D382D4BBD22BFC",
        0x18000000000,
        0x36100000000,
        SelfType::KERNEL);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "18E26DF712C362769D4F5E70460D28D88B7B991733DE692C2B9463B41FF4B925",
        "5B13077EEA801FC77D492050801FA507",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::KERNEL);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        0,
        "4769935C3B1CB248C3A88A406B1535D5DC2C0279D5901DE534DC4A11B8F60804",
        "0CE906F746D40105660456D827CEBD25",
        0x10300000000,
        0x16920000000,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        0,
        "4769935C3B1CB248C3A88A406B1535D5DC2C0279D5901DE534DC4A11B8F60804",
        "0CE906F746D40105660456D827CEBD25",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "613AD6EAC63D4E14F51A8C6AF18C66621968323B6F205B5E515C16D77BB06671",
        "ADBDAA5041B2094CF2B359301DE64171",
        0x10300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        2,
        "0F2041269B26D6B7EF143E35E83E914629A92F50F3A4CEE14CDFF63AEC641117",
        "07EF64437F0CB6995E6D785E42796C83",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        3,
        "3AFADA34660C6515B539EBBBC79C9C0ADA4337C32652CA03C6DD21A1D612D8F4",
        "7F98A137869B91B1EB9604F81FD74C50",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        4,
        "8FF491B36713E8AA38DE30B303689657F07AE70A8A8B1D7867441C52DB39C806",
        "D9CC7E26CE99053E48F9BEF1CB93C184",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        5,
        "4D71B2FB3D4359FB34445305C88A5E82FA12D34A8308F312AA34B58F6112253A",
        "04A27133FF0205C96B7F45A60D7D417B",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::USER);

    SCE_KEYS.register_keys(
        KeyType::NPDRM,
        SceType::SELF,
        0,
        "C10368BF3D2943BC6E5BD05E46A9A7B6",
        "00000000000000000000000000000000",
        0x00000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::NPDRM,
        SceType::SELF,
        1,
        "16419DD3BFBE8BDC596929B72CE237CD",
        "00000000000000000000000000000000",
        0x00000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        0,
        "AAA508FA5E85EAEE597ED2B27804D22287CFADF1DF32EDC7A7C58E8C9AA8BB36",
        "CD1BD3A59200CC67A3B804808DC2AE73",
        0x00000000000,
        0x16920000000,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        0,
        "5661E5FB20CFD1D1DFF50C1E59A6EA977D0AA5C5770F53B9CDD4E9451FFF55CB",
        "23D02FF79BF430E2D123869BF0CACAA0",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        1,
        "4181B2DF5F5D94D3C80B7D86EACF1928533A49BA58EDE2B43CDEE7E572568BD4",
        "B1678C0543B6C1997B63A6F4F3C8FD33",
        0x00000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        2,
        "5282582F17F068F89A260AAFB71C58928F45A8D08C681376B07FF9EAB1114226",
        "29672DF43E426F41AF46D42E8437D449",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        3,
        "270CBA370061B87077672ADB5142D18844AAED352A9CCEE63602B0D740594334",
        "1CF2454FBF47D76221B91AFC3B608C28",
        0x18000000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        4,
        "A782BC5A9EDDFC49A513FF3E592C4677A8C8920F23C9F11F2558FB9D99A43868",
        "559B5E658559EB65EBF892C274E098A9",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);

    SCE_KEYS.register_keys(
        KeyType::METADATA,
        SceType::SELF,
        5,
        "12D64D0172495226010A687DE245A73DE028B3561E25E69BABC325636F3CAE0A",
        "F149EED1757E5A915B24309795BFC380",
        0x36300000000,
        0xFFFFFFFFFFFFFFFF,
        SelfType::APP);
}

static void extract_file(Fat16::Image &img, Fat16::Entry &entry, const std::string &path) {
    const std::u16string filename_16 = entry.get_filename();
    const std::string filename = path + std::string(filename_16.begin(), filename_16.end());

    FILE *f = fopen(filename.c_str(), "wb");

    static constexpr std::uint32_t CHUNK_SIZE = 0x10000;

    std::vector<std::uint8_t> temp_buf;
    temp_buf.resize(CHUNK_SIZE);

    std::uint32_t size_left = entry.entry.file_size;
    std::uint32_t offset = 0;

    while (size_left != 0) {
        std::uint32_t size_to_take = std::min<std::uint32_t>(CHUNK_SIZE, size_left);
        if (img.read_from_cluster(&temp_buf[0], offset, entry.entry.starting_cluster, size_to_take) != size_to_take) {
            break;
        }

        size_left -= size_to_take;
        offset += size_to_take;

        fwrite(&temp_buf[0], 1, size_to_take, f);
    }

    fclose(f);
}

static void traverse_directory(Fat16::Image &img, Fat16::Entry mee, std::string dir_path) {
    fs::create_directories(dir_path);

    while (img.get_next_entry(mee)) {
        if (mee.entry.file_attributes & (int)Fat16::EntryAttribute::DIRECTORY) {
            // Also check if it's not the back folder (. and ..)
            // This can be done by gathering the name
            if (mee.entry.get_entry_type_from_filename() != Fat16::EntryType::DIRECTORY) {
                Fat16::Entry baby;
                if (!img.get_first_entry_dir(mee, baby))
                    break;

                auto dir_name = mee.get_filename();

                traverse_directory(img, baby, dir_path + "/" + std::string(dir_name.begin(), dir_name.end()) + "/");
            }
        }

        if (mee.entry.file_attributes & (int)Fat16::EntryAttribute::ARCHIVE) {
            extract_file(img, mee, dir_path + "/");
        }
    }
}

void extract_fat(const std::string &partition_path, const std::string &partition, const std::string &pref_path) {
    FILE *f = fopen((partition_path + "/" + partition).c_str(), "rb");
    Fat16::Image img(f,
        // Read hook
        [](void *userdata, void *buffer, std::uint32_t size) -> std::uint32_t {
            return static_cast<std::uint32_t>(fread(buffer, 1, size, (FILE *)userdata));
        },
        // Seek hook
        [](void *userdata, std::uint32_t offset, int mode) -> std::uint32_t {
            fseek((FILE *)userdata, offset, (mode == Fat16::IMAGE_SEEK_MODE_BEG ? SEEK_SET : (mode == Fat16::IMAGE_SEEK_MODE_CUR ? SEEK_CUR : SEEK_END)));

            return ftell((FILE *)userdata);
        });

    Fat16::Entry first;
    traverse_directory(img, first, pref_path + partition.substr(0, 3));

    fclose(f);
}

//Credits to the vitasdk team/contributors for vita-make-fself https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-make-fself.c

void make_fself(const std::string &input_file, const std::string &output_file) {
    std::ifstream filein(input_file, std::ios::binary);
    std::ofstream fileout(output_file, std::ios::binary);

    uint64_t file_size = fs::file_size(input_file);

    std::vector<char> input(file_size);
    filein.read(&input[0], file_size);
    filein.close();

    ElfHeader ehdr = ElfHeader(&input[0]);

    SCE_header hdr = { 0 };
    hdr.magic = SCE_MAGIC;
    hdr.version = 3;
    hdr.sdk_type = 0xC0;
    hdr.header_type = 1;
    hdr.metadata_offset = 0x600;
    hdr.header_len = HEADER_LENGTH;
    hdr.elf_filesize = file_size;
    hdr.self_offset = 4;
    hdr.appinfo_offset = 0x80;
    hdr.elf_offset = sizeof(SCE_header) + sizeof(SCE_appinfo);
    hdr.phdr_offset = hdr.elf_offset + ElfHeader::Size;
    hdr.phdr_offset = (hdr.phdr_offset + 0xf) & ~0xf; // align
    hdr.section_info_offset = hdr.phdr_offset + ElfPhdr::Size * ehdr.e_phnum;
    hdr.sceversion_offset = hdr.section_info_offset + sizeof(segment_info) * ehdr.e_phnum;
    hdr.controlinfo_offset = hdr.sceversion_offset + sizeof(SCE_version);
    hdr.controlinfo_size = sizeof(SCE_controlinfo_5) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7);
    hdr.self_filesize = 0;

    uint32_t offset_to_real_elf = HEADER_LEN;

    SCE_appinfo appinfo = { 0 };
    appinfo.authid = 0x2F00000000000001ULL;
    appinfo.vendor_id = 0;
    appinfo.self_type = 8;
    appinfo.version = 0x1000000000000;
    appinfo.padding = 0;

    SCE_version ver = { 0 };
    ver.unk1 = 1;
    ver.unk2 = 0;
    ver.unk3 = 16;
    ver.unk4 = 0;

    SCE_controlinfo_5 control_5 = { 0 };
    control_5.common.type = 5;
    control_5.common.size = sizeof(control_5);
    control_5.common.unk = 1;
    SCE_controlinfo_6 control_6 = { 0 };
    control_6.common.type = 6;
    control_6.common.size = sizeof(control_6);
    control_6.common.unk = 1;
    control_6.unk1 = 1;
    SCE_controlinfo_7 control_7 = { 0 };
    control_7.common.type = 7;
    control_7.common.size = sizeof(control_7);

    char empty_buffer[ElfHeader::Size] = { 0 };
    ElfHeader myhdr = ElfHeader(empty_buffer);
    memcpy(&myhdr.e_ident_1, "\177ELF\1\1\1", 8);
    myhdr.e_type = ehdr.e_type;
    myhdr.e_machine = 0x28;
    myhdr.e_version = 1;
    myhdr.e_entry = ehdr.e_entry;
    myhdr.e_phoff = 0x34;
    myhdr.e_flags = 0x05000000U;
    myhdr.e_ehsize = 0x34;
    myhdr.e_phentsize = 0x20;
    myhdr.e_phnum = ehdr.e_phnum;

    fileout.seekp(hdr.appinfo_offset, std::ios_base::beg);
    fileout.write((char *)&appinfo, sizeof(appinfo));

    fileout.seekp(hdr.elf_offset, std::ios_base::beg);
    fileout.write((char *)&myhdr, ElfHeader::Size);

    fileout.seekp(hdr.phdr_offset, std::ios_base::beg);
    for (int i = 0; i < ehdr.e_phnum; ++i) {
        ElfPhdr phdr = ElfPhdr(&input[0] + ehdr.e_phoff + ehdr.e_phentsize * i);
        if (phdr.p_align > 0x1000)
            phdr.p_align = 0x1000;
        fileout.write((char *)&phdr, sizeof(phdr));
    }

    fileout.seekp(hdr.section_info_offset, std::ios_base::beg);
    for (int i = 0; i < ehdr.e_phnum; ++i) {
        ElfPhdr phdr = ElfPhdr(&input[0] + ehdr.e_phoff + ehdr.e_phentsize * i);
        segment_info segment_info = { 0 };
        segment_info.offset = offset_to_real_elf + phdr.p_offset;
        segment_info.length = phdr.p_filesz;
        segment_info.compression = 1;
        segment_info.encryption = 2;
        fileout.write((char *)&segment_info, sizeof(segment_info));
    }

    fileout.seekp(hdr.sceversion_offset, std::ios_base::beg);
    fileout.write((char *)&ver, sizeof(ver));

    fileout.seekp(hdr.controlinfo_offset, std::ios_base::beg);
    fileout.write((char *)&control_5, sizeof(control_5));
    fileout.write((char *)&control_6, sizeof(control_6));
    fileout.write((char *)&control_7, sizeof(control_7));

    fileout.seekp(HEADER_LEN, std::ios_base::beg);
    fileout.write((char *)&input[0], file_size);

    fileout.seekp(0, std::ios_base::end);
    hdr.self_filesize = fileout.tellp();
    fileout.seekp(0, std::ios_base::beg);

    fileout.write((char *)&hdr, sizeof(hdr));

    fileout.close();
}

std::vector<SceSegment> get_segments(std::ifstream &file, const SceHeader &sce_hdr, KeyStore &SCE_KEYS, const uint64_t sysver, const SelfType self_type, int keytype, const int klictxt) {
    file.seekg(sce_hdr.metadata_offset + 48);
    std::vector<char> dat(sce_hdr.header_length - sce_hdr.metadata_offset - 48);
    file.read(&dat[0], sce_hdr.header_length - sce_hdr.metadata_offset - 48);

    const std::string key = SCE_KEYS.get(KeyType::METADATA, sce_hdr.sce_type, sysver, sce_hdr.key_revision, self_type).key;
    const std::string iv = SCE_KEYS.get(KeyType::METADATA, sce_hdr.sce_type, sysver, sce_hdr.key_revision, self_type).iv;
    aes_context aes_ctx;
    unsigned char dec_in[MetadataInfo::Size];

    if (self_type == SelfType::APP) {
        keytype = 0;
        if (sce_hdr.key_revision >= 2)
            keytype = 1;
        const std::string np_key = SCE_KEYS.get(KeyType::NPDRM, sce_hdr.sce_type, sysver, keytype, self_type).key;
        const std::string np_iv = SCE_KEYS.get(KeyType::NPDRM, sce_hdr.sce_type, sysver, keytype, self_type).iv;
        unsigned char predec[32];
        aes_setkey_dec(&aes_ctx, (unsigned char *)np_key.c_str(), 256);
        aes_crypt_cbc(&aes_ctx, AES_DECRYPT, sizeof(klictxt), (unsigned char *)np_iv.c_str(), (unsigned char *)klictxt, predec);

        unsigned char input_data[MetadataInfo::Size];
        std::copy(&dat[0], &dat[64], input_data);
        aes_setkey_dec(&aes_ctx, predec, 256);
        aes_crypt_cbc(&aes_ctx, AES_DECRYPT, MetadataInfo::Size, (unsigned char *)np_iv.c_str(), input_data, dec_in);

    } else {
        std::copy(&dat[0], &dat[64], dec_in);
    }
    unsigned char dec[64];

    const auto key_vec = string_utils::string_to_byte_array(key);
    auto iv_vec = string_utils::string_to_byte_array(iv);
    auto key_bytes = &key_vec[0];
    auto iv_bytes = &iv_vec[0];
    aes_setkey_dec(&aes_ctx, key_bytes, 256);
    aes_crypt_cbc(&aes_ctx, AES_DECRYPT, 64, iv_bytes, dec_in, dec);

    MetadataInfo metadata_info = MetadataInfo((char *)dec);

    std::vector<unsigned char> dec1(sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    std::vector<unsigned char> input_data(sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    memcpy(&input_data[0], &dat[64], sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    aes_setkey_dec(&aes_ctx, metadata_info.key, 128);
    aes_crypt_cbc(&aes_ctx, AES_DECRYPT, sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size, metadata_info.iv, &input_data[0], &dec1[0]);

    unsigned char dec2[MetadataHeader::Size];
    std::copy(&dec1[0], &dec1[MetadataHeader::Size], dec2);
    MetadataHeader metadata_hdr = MetadataHeader((char *)dec2);

    std::vector<SceSegment> segs;
    const auto start = MetadataHeader::Size + metadata_hdr.section_count * MetadataSection::Size;
    std::vector<std::string> vault;

    for (uint32_t i = 0; i < metadata_hdr.key_count; i++) {
        std::string key(&dec1[0] + (start + (16 * i)), &dec1[0] + (start + (16 * (i + 1))));
        vault.push_back(key);
    }

    for (uint32_t i = 0; i < metadata_hdr.section_count; i++) {
        std::vector<unsigned char> dec3((MetadataHeader::Size + i * MetadataSection::Size + MetadataSection::Size) - (MetadataHeader::Size + i * MetadataSection::Size));
        memcpy(&dec3[0], &dec1[0] + (MetadataHeader::Size + i * MetadataSection::Size), (MetadataHeader::Size + i * MetadataSection::Size + MetadataSection::Size) - (MetadataHeader::Size + i * MetadataSection::Size));
        MetadataSection metsec = MetadataSection((char *)&dec3[0]);

        if (metsec.encryption == EncryptionType::AES128CTR) {
            segs.push_back({ metsec.offset, metsec.seg_idx, metsec.size, metsec.compression == CompressionType::DEFLATE, vault[metsec.key_idx], vault[metsec.iv_idx] });
        }
    }
    return segs;
}

std::tuple<uint64_t, SelfType> get_key_type(std::ifstream &file, const SceHeader &sce_hdr) {
    if (sce_hdr.sce_type == SceType::SELF) {
        file.seekg(32);
        char selfheaderbuffer[SelfHeader::Size];
        file.read(selfheaderbuffer, SelfHeader::Size);
        SelfHeader self_hdr = SelfHeader(selfheaderbuffer);

        file.seekg(self_hdr.appinfo_offset);
        char appinfobuffer[AppInfoHeader::Size];
        file.read(appinfobuffer, AppInfoHeader::Size);
        AppInfoHeader appinfo_hdr = AppInfoHeader(appinfobuffer);

        return { appinfo_hdr.sys_version, appinfo_hdr.self_type };
    } else if (sce_hdr.sce_type == SceType::SRVK) {
        file.seekg(sce_hdr.header_length);
        char srvkheaderbuffer[SrvkHeader::Size];
        file.read(srvkheaderbuffer, SrvkHeader::Size);
        SrvkHeader srvk = SrvkHeader(srvkheaderbuffer);

        return { srvk.sys_version, SelfType::NONE };
    } else if (sce_hdr.sce_type == SceType::SPKG) {
        file.seekg(sce_hdr.header_length);
        char spkgheaderbuffer[SpkgHeader::Size];
        file.read(spkgheaderbuffer, SpkgHeader::Size);
        SpkgHeader spkg = SpkgHeader(spkgheaderbuffer);
        return { spkg.update_version << 16, SelfType::NONE };
    } else {
        LOG_ERROR("Unknown system version for type {}", static_cast<int>(sce_hdr.sce_type));
        return {};
    }
}
