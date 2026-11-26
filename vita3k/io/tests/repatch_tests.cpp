// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include <io/functions.h>
#include <io/state.h>

#include <gtest/gtest.h>

#include <atomic>
#include <fstream>
#include <set>
#include <string>

namespace {

constexpr const char *title_id = "PCSE00000";

void write_bytes(const fs::path &path, const std::string &bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << bytes;
}

class RepatchTest : public testing::Test {
protected:
    fs::path pref;
    IOState io;

    void SetUp() override {
        static std::atomic<int> counter;
        pref = fs::temp_directory_path() / ("vita3k-repatch-test-" + std::to_string(counter++) + "-" + std::to_string(reinterpret_cast<uintptr_t>(this)));
        fs::remove_all(pref);

        write_bytes(app("a.txt"), "app-a");
        write_bytes(app("b.txt"), "app-b!");
        write_bytes(repatch("a.txt"), "repatch-a-longer");

        io.redirect_stdio = false;
        io.case_isens_find_enabled = true;
        io.app_path = title_id;
        io.title_id = title_id;
        init_device_paths(io);
    }

    void TearDown() override {
        fs::remove_all(pref);
    }

    fs::path app(const std::string &rel) const {
        return pref / "ux0" / "app" / title_id / fs::path(rel);
    }

    fs::path repatch(const std::string &rel) const {
        return pref / "ux0" / "rePatch" / title_id / fs::path(rel);
    }

    std::string read_all(const SceUID fd, const SceSize size) {
        std::string data(size, '\0');
        const int read = read_file(data.data(), io, fd, size, "test");
        data.resize(std::max(read, 0));
        return data;
    }

    std::string read_whole(const char *path) {
        const SceUID fd = open_file(io, path, SCE_O_RDONLY, pref, "test");
        if (fd < 0)
            return "<open failed>";
        const auto data = read_all(fd, 64);
        close_file(io, fd, "test");
        return data;
    }

    std::set<std::string> list(const char *path, int *count = nullptr) {
        std::set<std::string> names;
        int entries = 0;
        const SceUID fd = open_dir(io, path, pref, "test");
        if (fd < 0)
            return names;
        SceIoDirent dent{};
        while (read_dir(io, fd, &dent, pref, "test") > 0) {
            names.insert(dent.d_name);
            ++entries;
        }
        close_dir(io, fd, "test");
        if (count)
            *count = entries;
        return names;
    }
};

} // namespace

TEST_F(RepatchTest, ReadPrefersRepatch) {
    EXPECT_EQ(read_whole("app0:/a.txt"), "repatch-a-longer");
}

TEST_F(RepatchTest, UntouchedFileFromApp) {
    EXPECT_EQ(read_whole("app0:/b.txt"), "app-b!");
}

TEST_F(RepatchTest, WriteNeverRepatch) {
    const SceUID fd = open_file(io, "app0:/a.txt", SCE_O_RDWR, pref, "test");
    ASSERT_GE(fd, 0);
    EXPECT_EQ(io.std_files.at(fd)->get_system_location(), app("a.txt"));
    close_file(io, fd, "test");

    // the rePatch copy is never opened for writing and stays intact
    EXPECT_EQ(read_whole("app0:/a.txt"), "repatch-a-longer");
}

TEST_F(RepatchTest, StatMatchesOpen) {
    SceIoStat st;
    ASSERT_EQ(stat_file(io, "app0:/a.txt", &st, pref, "test"), 0);
    EXPECT_EQ(st.st_size, 16);

    ASSERT_EQ(stat_file(io, "app0:/b.txt", &st, pref, "test"), 0);
    EXPECT_EQ(st.st_size, 6);
}

TEST_F(RepatchTest, ListingMerges) {
    int count = 0;
    const auto names = list("app0:/", &count);
    EXPECT_EQ(names, (std::set<std::string>{ "a.txt", "b.txt" }));
    EXPECT_EQ(count, 2);
}

TEST_F(RepatchTest, ListingEntryStatSeesRepatchSize) {
    const SceUID fd = open_dir(io, "app0:/", pref, "test");
    ASSERT_GE(fd, 0);
    SceIoDirent dent{};
    bool seen = false;
    while (read_dir(io, fd, &dent, pref, "test") > 0) {
        if (std::string(dent.d_name) == "a.txt") {
            EXPECT_EQ(dent.d_stat.st_size, 16);
            seen = true;
        }
    }
    close_dir(io, fd, "test");
    EXPECT_TRUE(seen);
}

TEST_F(RepatchTest, RepatchOnlyFileAndDir) {
    write_bytes(repatch("new/c.txt"), "only-repatch");
    EXPECT_EQ(read_whole("app0:/new/c.txt"), "only-repatch");
    EXPECT_EQ(list("app0:/new"), (std::set<std::string>{ "c.txt" }));
    EXPECT_EQ(list("app0:/"), (std::set<std::string>{ "a.txt", "b.txt", "new" }));
}

TEST_F(RepatchTest, SubdirectoryMerges) {
    write_bytes(app("d/x.txt"), "app-x");
    write_bytes(app("d/y.txt"), "app-y");
    write_bytes(repatch("d/x.txt"), "repatch-x");
    EXPECT_EQ(read_whole("app0:/d/x.txt"), "repatch-x");
    EXPECT_EQ(read_whole("app0:/d/y.txt"), "app-y");
    int count = 0;
    EXPECT_EQ(list("app0:/d", &count), (std::set<std::string>{ "x.txt", "y.txt" }));
    EXPECT_EQ(count, 2);
}

TEST_F(RepatchTest, CaseInsensitiveLookup) {
    EXPECT_EQ(read_whole("app0:/A.TXT"), "repatch-a-longer");
}

TEST_F(RepatchTest, JapanesePath) {
    write_bytes(repatch("\xE3\x83\x87\xE3\x83\xBC\xE3\x82\xBF.bin"), "nihongo");
    EXPECT_EQ(read_whole("app0:/\xE3\x83\x87\xE3\x83\xBC\xE3\x82\xBF.bin"), "nihongo");
}

TEST_F(RepatchTest, OtherDevicesUnaffected) {
    // only app0: is overlaid, a direct ux0:app path still reads the game's own file
    EXPECT_EQ(read_whole("ux0:/app/PCSE00000/a.txt"), "app-a");
}

TEST_F(RepatchTest, RootedPathStaysInRepatch) {
    write_bytes(pref / "host-secret.txt", "secret");
    const std::string secret_path = (pref / "host-secret.txt").string();

    // translate_path collapses "//", the lookup itself must refuse a rooted path as well
    EXPECT_TRUE(find_repatch_path(io, pref, "app/PCSE00000/" + secret_path).empty());
    EXPECT_TRUE(find_repatch_path(io, pref, "app/PCSE00000//etc/passwd").empty());
    EXPECT_NE(read_whole(("app0:/" + secret_path).c_str()), "secret");
    EXPECT_NE(read_whole("app0://etc/passwd").substr(0, 5), "root:");
    EXPECT_EQ(read_whole("app0:\\\\etc\\passwd"), "<open failed>");

    SceIoStat st;
    EXPECT_LT(stat_file(io, ("app0:/" + secret_path).c_str(), &st, pref, "test"), 0);
    EXPECT_LT(stat_file(io, "app0://etc", &st, pref, "test"), 0);
    EXPECT_TRUE(list("app0://etc").empty());
}

TEST_F(RepatchTest, DotDotStaysInRepatch) {
    write_bytes(pref / "outside.txt", "outside");
    write_bytes(app("inner.txt"), "app-inner");
    EXPECT_TRUE(find_repatch_path(io, pref, "app/PCSE00000/../../../../outside.txt").empty());
    EXPECT_TRUE(find_repatch_path(io, pref, "app/PCSE00000/d/../../../../../outside.txt").empty());
    EXPECT_NE(read_whole("app0:/../../../../outside.txt"), "outside");
    EXPECT_NE(read_whole("app0:/..\\..\\..\\..\\outside.txt"), "outside");
}

TEST_F(RepatchTest, SymlinkOutOfRepatchIsIgnored) {
    write_bytes(pref / "outside" / "s.txt", "outside-s");
    write_bytes(app("link.txt"), "app-link");
    fs::create_directories(repatch(""));
    std::error_code ec;
    fs::create_symlink(pref / "outside" / "s.txt", repatch("link.txt"), ec);
    fs::create_directory_symlink(pref / "outside", repatch("dirlink"), ec);
    ASSERT_FALSE(ec);

    // the file symlink falls back to the game's file, the directory symlink finds nothing
    EXPECT_EQ(read_whole("app0:/link.txt"), "app-link");
    EXPECT_NE(read_whole("app0:/dirlink/s.txt"), "outside-s");
    EXPECT_TRUE(find_repatch_path(io, pref, "app/PCSE00000/dirlink/s.txt").empty());
    EXPECT_TRUE(find_repatch_path(io, pref, "app/PCSE00000/link.txt").empty());
}

TEST_F(RepatchTest, SymlinkInsideRepatchWorks) {
    write_bytes(repatch("real/t.txt"), "real-t");
    std::error_code ec;
    fs::create_directory_symlink(repatch("real"), repatch("alias"), ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(read_whole("app0:/alias/t.txt"), "real-t");
}

TEST_F(RepatchTest, EbootAndModuleOnlyInRepatchResolve) {
    // what load_module and sceAppMgrLoadExec probe: the translated app0 path
    write_bytes(repatch("eboot.bin"), "new-eboot");
    write_bytes(repatch("sce_module/x.suprx"), "new-module");
    write_bytes(app("eboot.bin"), "old-eboot");

    EXPECT_EQ(find_repatch_path(io, pref, io.device_paths.app0 + "/eboot.bin"), repatch("eboot.bin"));
    EXPECT_EQ(find_repatch_path(io, pref, io.device_paths.app0 + "/sce_module/x.suprx"), repatch("sce_module/x.suprx"));
    EXPECT_TRUE(find_repatch_path(io, pref, io.device_paths.app0 + "/sce_module/y.suprx").empty());
    EXPECT_FALSE(fs::exists(app("sce_module/x.suprx")));
}
