// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
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

#include <util/log.h>
#include <util/string_utils.h>

#ifdef WIN32
static const auto vita3k_exec = "Vita3K.exe";
static const auto updater_exec = "updater.exe";
static const auto updater_old_exec = "updater_old.exe";
#else
static const auto vita3k_exec = "./Vita3K";
static const auto updater_exec = "./updater";
static const auto updater_old_exec = "./updater_old";
#endif

static bool download_update(const std::string git_version) {
#ifdef WIN32
    const auto download_command = "powershell Invoke-WebRequest https://github.com/Vita3K/Vita3K/releases/download/continuous/windows-latest.zip -timeout 4 -OutFile vita3k-latest.zip";
#else
    const auto download_command = "curl -L https://github.com/Vita3K/Vita3K/releases/download/continuous/ubuntu-latest.zip -o ./vita3k-latest.zip";
#endif
    LOG_INFO("Attempting to download and extract the latest Vita3K version {} in progress...", git_version);

    return std::system(download_command) == 0;
}

static std::string get_version(const std::string cmd) {
    char tmp_ver[L_tmpnam + 1];
#ifdef WIN32
    tmpnam_s(tmp_ver);
#else
    tmpnam_r(tmp_ver);
#endif
    std::string ver_cmd = cmd + " >> " + tmp_ver;
    std::system(ver_cmd.c_str());
    std::ifstream ver(tmp_ver, std::ios::in);
    std::string version;
    std::getline(ver, version);
    ver.close();
    std::remove(tmp_ver);

    return version;
}

static int exit() {
    LOG_INFO("Press a key for continue");
    std::cin.get();

    return 0;
}

#ifdef WIN32
static const std::string version_cmd = R"(powershell ((Get-Item Vita3k.exe).VersionInfo.FileVersion) -replace \"0.1.5.\")";
#else
static const std::string version_cmd = R"(./Vita3K --version | cut -d" " -f3 | cut -d"-" -f1)";
#endif

static bool install_update(const std::string git_version) {
    if (!fs::exists("vita3k-latest.zip"))
        return false;

#ifdef WIN32
    const auto extract_cmd = R"(powershell Expand-Archive vita3k-latest.zip -DestinationPath '.' -Force)";
#else
    const auto extract_cmd = R"(unzip -q -o ./vita3k-latest.zip && rm -f ./vita3k-latest.zip && chmod +x ./Vita3K)";
#endif
    LOG_INFO("Installing latest version {}...", git_version);
    if (std::system(extract_cmd) != 0)
        return false;

#ifndef WIN32
    std::system("chmod +x ./Vita3K");
#endif

    fs::remove("vita3k-latest.zip");

    return get_version(version_cmd) == git_version;
}

static int exec(const std::string exec, const std::string argv) {
#ifdef _WIN32
    std::system(("start \"Updater\" " + exec + " " + argv).c_str());
#else
    std::system(("xdg-open " + exec + " & disown").c_str());
#endif

    return 0;
}

static int download_update_and_copy_updater(const std::string git_version, const std::string mode) {
    if (download_update(git_version)) {
        fs::copy_file(updater_exec, updater_old_exec, fs::copy_option::overwrite_if_exists);
#ifndef WIN32     
        std::system("chmod +x ./updater_old");
#endif
        return exec(updater_old_exec, "--" + mode);
    } else
        LOG_WARN("Could not complete the download on the latest version {}.", git_version);

    return exit();
}

int main(int argc, char *argv[]) {
    Root root_paths;
    if (logging::init(root_paths, true) != Success) {
        LOG_ERROR("Failed to init logging");
        return exit();
    }

    // Get Build number of latest release
    const auto latest_link = "https://api.github.com/repos/Vita3K/Vita3K/releases/latest";
#ifdef WIN32
    const auto powershell_version = get_version("powershell (Get-Host).Version.major");
    if (powershell_version.empty() || !std::isdigit(powershell_version[0]) || (std::stoi(powershell_version) < 3)) {
        LOG_ERROR("You PowerShell version {} is outdated and incompatible with Vita3K Update, consider to update it", powershell_version);
        return exit();
    }

    const auto github_version_cmd = fmt::format(R"(powershell ((Invoke-RestMethod {} -timeout 4).body.split([Environment]::NewLine) ^| Select-String -Pattern \"Vita3K Build: \") -replace \"Vita3K Build: \")", latest_link);
#else
    const auto github_version_cmd = fmt::format(R"(curl -sL {} | grep "Corresponding commit:" | cut -d " " -f 8 | grep -o '[[:digit:]]*')", latest_link);
#endif

    LOG_INFO("============================================================");
    LOG_INFO("====================== Vita3K Updater ======================");
    LOG_INFO("============================================================");

    const auto git_version = get_version(github_version_cmd);
    if (git_version.empty() || !std::isdigit(git_version[0])) {
        LOG_WARN("Failed to get current git version, try again later\n{}", git_version);
        
        return exit();
    }

    if (argc > 1) {
        if (strcmp(argv[1], "--install") == 0) {
            if (install_update(git_version))
                return exec(updater_exec, "--success --install");
            else
                LOG_WARN("Could not complete the install on the latest version {}.", git_version);
        } else if (strcmp(argv[1], "--update") == 0) {
            if (fs::exists(vita3k_exec)) {
                const auto version = get_version(version_cmd);
                if (install_update(git_version)) {
                    if ((argc == 3) && strcmp(argv[2], "--start") == 0)
                        return exec(updater_exec, "--success --update " + version + " --start");
                    else
                        return exec(updater_exec, "--success --update " + version);
                } else
                    LOG_WARN("Could not complete the update on the latest version {}.", git_version);
            }
        } else if (strcmp(argv[1], "--success") == 0) {
            fs::remove(updater_old_exec);
            if (argc > 2) {
                if (strcmp(argv[2], "--install") == 0) {
                    LOG_INFO("Vita3K installed with success on version {}!", git_version);
                    LOG_INFO("============================================================");
                    LOG_INFO("=================== Install complete! ======================");
                    LOG_INFO("============================================================");
                } else if (strcmp(argv[2], "--update") == 0) {
                    const auto version = argv[3];
                    LOG_INFO("Successfully updated your Vita3K version from {} to {}!", version, git_version);
                    LOG_INFO("============================================================");
                    LOG_INFO("===================== Update complete! =====================");
                    LOG_INFO("============================================================");
                }

                if ((argc == 5) && strcmp(argv[4], "--start") == 0) {
                    LOG_INFO("Starting Vita3K...");
                    return std::system(vita3k_exec);
                }
            }
        } else
            LOG_ERROR("Argument invalide: {}", argv[1]);

        return exit();
    } else {
        if (fs::exists(vita3k_exec)) {
            const auto version = get_version(version_cmd);
            if (std::stoi(version) > std::stoi(git_version)) {
                LOG_INFO("The later version {} of Vita3K is already installed.", git_version);
            } else if (std::stoi(version) == std::stoi(git_version)) {
                LOG_INFO("The latest version {} of Vita3K is already instaled.", git_version);
            } else {
                LOG_INFO("A new version {} of Vita3K is available.", git_version);
                return download_update_and_copy_updater(git_version, "update");
            }
        } else {
            LOG_INFO("Vita3K not found, do you want installing latest version {}? [y/n]", git_version);
            char ask;
            std::cin >> ask;
            if (ask == 'y')
                return download_update_and_copy_updater(git_version, "install");
            else {
                LOG_WARN("Installation canceled!");
                std::cin.get();
            }
        }
    }

    return exit();
}
