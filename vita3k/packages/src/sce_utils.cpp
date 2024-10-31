// Vita3K emulator project
// Copyright (C) 2025 Vita3K team
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

/**
 * @file sce_utils.cpp
 * @brief Utilities to handle SCE binaries
 */

#include <fat16/fat16.h>
#include <miniz.h>
#include <openssl/evp.h>
#include <packages/sce_types.h>
#include <util/string_utils.h>

#include <self.h>

#include <fstream>

// Credits to TeamMolecule for their original work on this https://github.com/TeamMolecule/sceutils

void register_keys(KeyStore &SCE_KEYS, int type) {
    // case 0 registers external(retail), 1 registers internal proto keys, proto_keys not added.
    switch (type) {
    case 0:
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
        break;
    case 1:
        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SPKG,
            0,
            "2E6F4751D15B06C51F572A9306E52DD7007EA56A31D459EC6D3681AB08625501",
            "B3D541A568751DF8F4833BAB4EFE0537",
            0x10400000000,
            0xFFF00000000,
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
            0xFFF00000000,
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
            0xFFF00000000,
            SelfType::SECURE);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            1,
            "7A7FB1560DCD121CEA5E11B90124B13282752F2D5B95D75036AB3A29BB3BD2AB",
            "6C71642A042A041F1EE3094070B009BE",
            0x10300000000,
            0x18000000000,
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
            0xFFF00000000,
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
            0xFFF00000000,
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
            0x00000000000,
            0xFFF00000000,
            SelfType::USER);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            2,
            "0F2041269B26D6B7EF143E35E83E914629A92F50F3A4CEE14CDFF63AEC641117",
            "07EF64437F0CB6995E6D785E42796C83",
            0x00000000000,
            0xFFF00000000,
            SelfType::USER);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            3,
            "3AFADA34660C6515B539EBBBC79C9C0ADA4337C32652CA03C6DD21A1D612D8F4",
            "7F98A137869B91B1EB9604F81FD74C50",
            0x00000000000,
            0xFFF00000000,
            SelfType::USER);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            4,
            "8FF491B36713E8AA38DE30B303689657F07AE70A8A8B1D7867441C52DB39C806",
            "D9CC7E26CE99053E48F9BEF1CB93C184",
            0x00000000000,
            0xFFF00000000,
            SelfType::USER);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            5,
            "4D71B2FB3D4359FB34445305C88A5E82FA12D34A8308F312AA34B58F6112253A",
            "04A27133FF0205C96B7F45A60D7D417B",
            0x00000000000,
            0xFFF00000000,
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
            0x00000000000,
            0xFFFFFFFFFFFFFFFF,
            SelfType::APP);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            3,
            "270CBA370061B87077672ADB5142D18844AAED352A9CCEE63602B0D740594334",
            "1CF2454FBF47D76221B91AFC3B608C28",
            0x00000000000,
            0xFFFFFFFFFFFFFFFF,
            SelfType::APP);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            4,
            "A782BC5A9EDDFC49A513FF3E592C4677A8C8920F23C9F11F2558FB9D99A43868",
            "559B5E658559EB65EBF892C274E098A9",
            0x00000000000,
            0xFFFFFFFFFFFFFFFF,
            SelfType::APP);

        SCE_KEYS.register_keys(
            KeyType::METADATA,
            SceType::SELF,
            5,
            "12D64D0172495226010A687DE245A73DE028B3561E25E69BABC325636F3CAE0A",
            "F149EED1757E5A915B24309795BFC380",
            0x00000000000,
            0xFFFFFFFFFFFFFFFF,
            SelfType::APP);
        break;
    }
}

static void extract_file(Fat16::Image &img, Fat16::Entry &entry, const fs::path &path) {
    const std::u16string filename_16 = entry.get_filename();
    const fs::path filename = path / std::wstring(filename_16.begin(), filename_16.end());
    FILE *f = FOPEN(filename.native().c_str(), "wb");

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

static void traverse_directory(Fat16::Image &img, Fat16::Entry mee, const fs::path &dir_path) {
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

                traverse_directory(img, baby, dir_path / std::wstring(dir_name.begin(), dir_name.end()) / "");
            }
        }

        if (mee.entry.file_attributes & (int)Fat16::EntryAttribute::ARCHIVE) {
            extract_file(img, mee, dir_path / "");
        }
    }
}

void extract_fat(const fs::path &partition_path, const std::string &partition, const fs::path &pref_path) {
    FILE *f = FOPEN((partition_path / partition).native().c_str(), "rb");
    Fat16::Image img(
        f,
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
    traverse_directory(img, first, pref_path / partition.substr(0, 3));

    fclose(f);
}

std::string decompress_segments(const std::vector<uint8_t> &decrypted_data, const uint64_t &size) {
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

    const std::string compressed_data(reinterpret_cast<const char *>(decrypted_data.data()), size);
    stream.next_in = reinterpret_cast<const Bytef *>(compressed_data.data());
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

std::vector<SceSegment> get_segments(const uint8_t *input, const SceHeader &sce_hdr, KeyStore &SCE_KEYS, const uint64_t sysver, const SelfType self_type, int keytype, const uint8_t *klic) {
    std::vector<char> dat(sce_hdr.header_length - sce_hdr.metadata_offset - 48);
    memcpy(dat.data(), &input[sce_hdr.metadata_offset + 48], sce_hdr.header_length - sce_hdr.metadata_offset - 48);

    const std::string key = SCE_KEYS.get(KeyType::METADATA, sce_hdr.sce_type, sysver, sce_hdr.key_revision, self_type).key;
    const std::string iv = SCE_KEYS.get(KeyType::METADATA, sce_hdr.sce_type, sysver, sce_hdr.key_revision, self_type).iv;

    EVP_CIPHER_CTX *cipher_ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER *cipher128 = EVP_CIPHER_fetch(nullptr, "AES-128-CBC", nullptr);
    EVP_CIPHER *cipher256 = EVP_CIPHER_fetch(nullptr, "AES-256-CBC", nullptr);
    int dec_len = 0;

    unsigned char dec_in[MetadataInfo::Size];

    if (self_type == SelfType::APP) {
        keytype = 0;
        if (sce_hdr.key_revision >= 2)
            keytype = 1;
        const std::string np_key = SCE_KEYS.get(KeyType::NPDRM, sce_hdr.sce_type, sysver, keytype, self_type).key;
        const std::string np_iv = SCE_KEYS.get(KeyType::NPDRM, sce_hdr.sce_type, sysver, keytype, self_type).iv;
        const auto np_key_vec = string_utils::string_to_byte_array(np_key);
        auto np_iv_vec = string_utils::string_to_byte_array(np_iv);
        unsigned char predec[16];

        EVP_DecryptInit_ex(cipher_ctx, cipher128, nullptr, np_key_vec.data(), np_iv_vec.data());
        EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
        EVP_DecryptUpdate(cipher_ctx, predec, &dec_len, klic, 16);
        EVP_DecryptFinal_ex(cipher_ctx, predec + dec_len, &dec_len);

        unsigned char input_data[MetadataInfo::Size];
        std::copy(dat.data(), &dat[MetadataInfo::Size], input_data);
        EVP_DecryptInit_ex(cipher_ctx, cipher128, nullptr, predec, np_iv_vec.data());
        EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
        EVP_DecryptUpdate(cipher_ctx, dec_in, &dec_len, input_data, MetadataInfo::Size);
        EVP_DecryptFinal_ex(cipher_ctx, predec + dec_len, &dec_len);

    } else {
        std::copy(dat.data(), &dat[MetadataInfo::Size], dec_in);
    }
    unsigned char dec[64];

    const auto key_vec = string_utils::string_to_byte_array(key);
    auto iv_vec = string_utils::string_to_byte_array(iv);
    EVP_DecryptInit_ex(cipher_ctx, cipher256, nullptr, key_vec.data(), iv_vec.data());
    EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
    EVP_DecryptUpdate(cipher_ctx, dec, &dec_len, dec_in, MetadataInfo::Size);
    EVP_DecryptFinal_ex(cipher_ctx, dec + dec_len, &dec_len);

    MetadataInfo metadata_info = MetadataInfo((char *)dec);

    std::vector<unsigned char> dec1(sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    std::vector<unsigned char> input_data(sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    memcpy(input_data.data(), &dat[MetadataInfo::Size], sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    EVP_DecryptInit_ex(cipher_ctx, cipher128, nullptr, metadata_info.key, metadata_info.iv);
    EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
    EVP_DecryptUpdate(cipher_ctx, dec1.data(), &dec_len, input_data.data(), sce_hdr.header_length - sce_hdr.metadata_offset - 48 - MetadataInfo::Size);
    EVP_DecryptFinal_ex(cipher_ctx, dec1.data() + dec_len, &dec_len);

    unsigned char dec2[MetadataHeader::Size];
    std::copy(dec1.data(), &dec1[MetadataHeader::Size], dec2);
    MetadataHeader metadata_hdr = MetadataHeader((char *)dec2);

    std::vector<SceSegment> segs;
    const auto start = MetadataHeader::Size + metadata_hdr.section_count * MetadataSection::Size;
    std::vector<std::string> vault;

    for (uint32_t i = 0; i < metadata_hdr.key_count; i++) {
        std::string key(&dec1[start + (16 * i)], &dec1[start + (16 * (i + 1))]);
        vault.push_back(key);
    }

    for (uint32_t i = 0; i < metadata_hdr.section_count; i++) {
        std::vector<unsigned char> dec3((MetadataHeader::Size + i * MetadataSection::Size + MetadataSection::Size) - (MetadataHeader::Size + i * MetadataSection::Size));
        memcpy(dec3.data(), &dec1[MetadataHeader::Size + i * MetadataSection::Size], (MetadataHeader::Size + i * MetadataSection::Size + MetadataSection::Size) - (MetadataHeader::Size + i * MetadataSection::Size));
        MetadataSection metsec = MetadataSection((char *)dec3.data());

        if (metsec.encryption == EncryptionType::AES128CTR) {
            segs.push_back({ metsec.offset, metsec.seg_idx, metsec.size, metsec.compression == CompressionType::DEFLATE, vault[metsec.key_idx], vault[metsec.iv_idx] });
        }
    }

    EVP_CIPHER_CTX_free(cipher_ctx);
    EVP_CIPHER_free(cipher128);
    EVP_CIPHER_free(cipher256);

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

std::vector<uint8_t> decrypt_fself(const std::vector<uint8_t> &fself, const uint8_t *klic) {
    const SCE_header &self_header = *reinterpret_cast<const SCE_header *>(fself.data());

    // Check if a valid SELF or is still in encrypted layer
    if (self_header.magic != SCE_MAGIC) {
        LOG_ERROR("Invalid SELF: file is either not a SELF or is still encrypted (unsupported).");
        return {};
    }

    const segment_info *const seg_infos = reinterpret_cast<const segment_info *>(&fself[self_header.section_info_offset]);
    const AppInfoHeader app_info_hdr = AppInfoHeader(reinterpret_cast<const char *>(&fself[self_header.appinfo_offset]));

    // Check the encryption self type
    if (seg_infos->encryption == 2)
        return fself; // Self is not encrypted, return the original self

    // Check if the self is an app and if a klic have all 0 inside it
    const auto is_app = app_info_hdr.self_type == SelfType::APP;
    if (is_app && std::all_of(klic, klic + 16, [](uint8_t i) { return i == 0; })) {
        LOG_ERROR("No klic provided for encrypted App");
        return {};
    }

    // Get the keyset for the self
    KeyStore SCE_KEYS;
    register_keys(SCE_KEYS, 1);
    uint64_t authid = 0x2F00000000000001ULL;

    // Extract the elf from the self
    std::vector<uint8_t> elf;
    int npdrmtype = 0;

    const SceHeader sce_hdr = SceHeader(reinterpret_cast<const char *>(fself.data()));
    const SelfHeader self_hdr = SelfHeader(reinterpret_cast<const char *>(&fself[SceHeader::Size]));
    authid = app_info_hdr.auth_id;

    // Get the control info
    SceControlInfo control_info = SceControlInfo(reinterpret_cast<const char *>(&fself[self_hdr.controlinfo_offset]));
    auto ci_off = SceControlInfo::Size;

    // Check if the control info is a digest
    if (control_info.type == ControlType::DIGEST_SHA256)
        ci_off += SceControlInfoDigest256::Size;

    control_info = SceControlInfo(reinterpret_cast<const char *>(&fself[self_hdr.controlinfo_offset + ci_off]));

    ci_off += SceControlInfo::Size;

    // Check if the control info is a npdrm type
    if (control_info.type == ControlType::NPDRM_VITA) {
        const SceControlInfoDRM controlnpdrm = SceControlInfoDRM(reinterpret_cast<const char *>(&fself[self_hdr.controlinfo_offset + ci_off]));
        npdrmtype = controlnpdrm.npdrm_type;
    }

    // Extract the elf from the encrypted self
    const ElfHeader elf_hdr = ElfHeader(reinterpret_cast<const char *>(&fself[self_hdr.elf_offset]));
    elf.insert(elf.end(), (const uint8_t *)&elf_hdr, (const uint8_t *)&elf_hdr + ElfHeader::Size);

    // Extract the phdrs from the encrypted self
    std::vector<ElfPhdr> elf_phdrs;
    std::vector<SegmentInfo> segment_infos;
    bool encrypted = false;
    uint64_t at = ElfHeader::Size;

    for (uint16_t i = 0; i < elf_hdr.e_phnum; i++) {
        const ElfPhdr phdr = ElfPhdr(reinterpret_cast<const char *>(&fself[self_hdr.phdr_offset + (i * ElfPhdr::Size)]));
        elf_phdrs.push_back(phdr);
        elf.insert(elf.end(), (const uint8_t *)&phdr, (const uint8_t *)&phdr + ElfPhdr::Size);
        at += ElfPhdr::Size;

        const SegmentInfo segment_info = SegmentInfo(reinterpret_cast<const char *>(&fself[self_hdr.segment_info_offset + (i * SegmentInfo::Size)]));
        segment_infos.push_back(segment_info);

        if (segment_info.plaintext == SecureBool::NO)
            encrypted = true;
    }

    std::vector<SceSegment> scesegs;

    if (encrypted)
        scesegs = get_segments(fself.data(), sce_hdr, SCE_KEYS, app_info_hdr.sys_version, app_info_hdr.self_type, npdrmtype, klic);

    EVP_CIPHER_CTX *cipher_ctx = EVP_CIPHER_CTX_new();
    EVP_CIPHER *cipher = EVP_CIPHER_fetch(nullptr, "AES-128-CTR", nullptr);
    int dec_len = 0;

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

        elf.resize(elf.size() + pad_len);

        at += pad_len;

        std::vector<unsigned char> decrypted_data(segment_infos[idx].size);
        if (segment_infos[idx].plaintext == SecureBool::NO) {
            EVP_DecryptInit_ex(cipher_ctx, cipher, nullptr, reinterpret_cast<const unsigned char *>(scesegs[i].key.c_str()), reinterpret_cast<const unsigned char *>(scesegs[i].iv.c_str()));
            EVP_CIPHER_CTX_set_padding(cipher_ctx, 0);
            EVP_DecryptUpdate(cipher_ctx, decrypted_data.data(), &dec_len, &fself[segment_infos[idx].offset], segment_infos[idx].size);
            EVP_DecryptFinal_ex(cipher_ctx, decrypted_data.data() + dec_len, &dec_len);
        }

        if (segment_infos[idx].compressed == SecureBool::YES) {
            const std::string decompressed_data = decompress_segments(decrypted_data, segment_infos[idx].size);
            segment_infos[idx].compressed = SecureBool::NO;
            elf.insert(elf.end(), decompressed_data.begin(), decompressed_data.end());
            at += decompressed_data.length();
        } else {
            elf.insert(elf.end(), decrypted_data.data(), &decrypted_data[segment_infos[idx].size]);
            at += segment_infos[idx].size;
        }
    }

    EVP_CIPHER_CTX_free(cipher_ctx);
    EVP_CIPHER_free(cipher);

    // Credits to the vitasdk team/contributors for vita-make-fself https://github.com/vitasdk/vita-toolchain/blob/master/src/vita-make-fself.c

    // Create a new self with the extracted elf
    std::vector<uint8_t> decrypted_self(HEADER_LEN + elf.size());
    ElfHeader ehdr = ElfHeader((char *)elf.data());

    // Create a new header
    SCE_header hdr{
        .magic = SCE_MAGIC,
        .version = 3,
        .sdk_type = 0xC0,
        .header_type = 1,
        .metadata_offset = 0x600,
        .header_len = HEADER_LENGTH,
        .elf_filesize = elf.size(),
        .self_filesize = HEADER_LEN + elf.size(),
        .self_offset = 4,
        .appinfo_offset = 0x80,
        .elf_offset = sizeof(SCE_header) + sizeof(SCE_appinfo),
        .phdr_offset = (hdr.elf_offset + ElfHeader::Size + 0xf) & ~0xf,
        .section_info_offset = (hdr.phdr_offset + ElfPhdr::Size) * ehdr.e_phnum,
        .sceversion_offset = hdr.section_info_offset + sizeof(segment_info) * ehdr.e_phnum,
        .controlinfo_offset = hdr.sceversion_offset + sizeof(SCE_version),
        .controlinfo_size = sizeof(SCE_controlinfo_5) + sizeof(SCE_controlinfo_6) + sizeof(SCE_controlinfo_7),
    };

    // Copy the header to the new self
    memcpy(decrypted_self.data(), &hdr, sizeof(hdr));

    uint32_t offset_to_real_elf = HEADER_LEN;

    // Create a new app info
    SCE_appinfo appinfo{
        .authid = authid,
        .vendor_id = 0,
        .self_type = 8,
        .version = 0x1000000000000,
        .padding = 0
    };

    // Create a new version info
    SCE_version ver{
        .unk1 = 1,
        .unk2 = 0,
        .unk3 = 16,
        .unk4 = 0
    };

    // Create a new control info
    SCE_controlinfo_5 control_5{
        .common = {
            .type = 5,
            .size = sizeof(control_5),
            .unk = 1 }
    };
    SCE_controlinfo_6 control_6{
        .common = {
            .type = 6,
            .size = sizeof(control_6),
            .unk = 1 },
        .is_used = 1
    };
    SCE_controlinfo_7 control_7{
        .common = {
            .type = 7,
            .size = sizeof(control_7) }
    };

    // Create a new elf header
    char empty_buffer[ElfHeader::Size]{};
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

    // Copy the app info to the decrypted self
    memcpy(&decrypted_self[hdr.appinfo_offset], &appinfo, sizeof(appinfo));

    // Copy the elf header to the decrypted self
    memcpy(&decrypted_self[hdr.elf_offset], &myhdr, ElfHeader::Size);

    // Copy the phdrs to the decrypted self
    for (int i = 0; i < ehdr.e_phnum; ++i) {
        ElfPhdr phdr = ElfPhdr(reinterpret_cast<const char *>(&elf[ehdr.e_phoff + (ehdr.e_phentsize * i)]));
        if (phdr.p_align > 0x1000)
            phdr.p_align = 0x1000;
        memcpy(&decrypted_self[hdr.phdr_offset + (ElfPhdr::Size * i)], &phdr, ElfPhdr::Size);

        // Copy the segment info to the decrypted self
        segment_info segment_info{
            .offset = offset_to_real_elf + phdr.p_offset,
            .length = phdr.p_filesz,
            .compression = 1,
            .encryption = 2,
        };
        memcpy(&decrypted_self[hdr.section_info_offset + (SegmentInfo::Size * i)], &segment_info, SegmentInfo::Size);
    }

    auto current_offset = hdr.sceversion_offset;
    const auto copy_data = [&current_offset, &decrypted_self](const void *data, const size_t size) {
        memcpy(&decrypted_self[current_offset], data, size);
        current_offset += size;
    };

    // Copy the version and control info to the decrypted self
    copy_data(&ver, sizeof(ver));
    copy_data(&control_5, sizeof(control_5));
    copy_data(&control_6, sizeof(control_6));
    copy_data(&control_7, sizeof(control_7));

    // Copy the elf to the decrypted self
    memcpy(&decrypted_self[HEADER_LEN], elf.data(), elf.size());

    // Return the decrypted self
    return decrypted_self;
}

const unsigned char HMACKey[] = {
    0xE5, 0xE2, 0x78, 0xAA, 0x1E, 0xE3, 0x40, 0x82,
    0xA0, 0x88, 0x27, 0x9C, 0x83, 0xF9, 0xBB, 0xC8,
    0x06, 0x82, 0x1C, 0x52, 0xF2, 0xAB, 0x5D, 0x2B,
    0x4A, 0xBD, 0x99, 0x54, 0x50, 0x35, 0x51, 0x14
};

std::string resolve_ver_xml_url(const std::string &title_id) {
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    if (!mac) {
        LOG_ERROR("Failed to fetch HMAC MAC");
        return {};
    }

    // Create a new HMAC context
    EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);
    if (!ctx) {
        EVP_MAC_free(mac);
        LOG_ERROR("Failed to create HMAC context");
        return {};
    }

    OSSL_PARAM params[2] = {
        OSSL_PARAM_construct_utf8_string("digest", (char *)"SHA256", 0),
        OSSL_PARAM_construct_end()
    };

    EVP_MAC_init(ctx, HMACKey, sizeof(HMACKey), params);

    const auto np_title_id = fmt::format("np_{}", title_id);
    EVP_MAC_update(ctx, (const unsigned char *)np_title_id.c_str(), np_title_id.length());

    unsigned char hash[EVP_MAX_MD_SIZE];
    size_t outlen;
    EVP_MAC_final(ctx, hash, &outlen, sizeof(hash));

    // Free the MAC context and the MAC object
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);

    std::string hashStr(outlen * 2, '\0');
    for (size_t i = 0; i < outlen; i++)
        fmt::format_to(&hashStr[i * 2], "{:02x}", hash[i]);

    const auto updateXmlUrl = "http://gs-sec.ww.np.dl.playstation.net/pl/np/" + title_id + "/" + hashStr + "/" + title_id + "-ver.xml";

    LOG_DEBUG("updateXmlUrl: {}", updateXmlUrl);

    return updateXmlUrl;
}
