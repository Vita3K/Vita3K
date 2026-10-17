// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <pfsmgr/modes.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pfsmgr {

using Hash = std::array<uint8_t, 20>;
using AesKey = std::array<uint8_t, 16>;
using PortabilityKey = std::array<uint8_t, 32>;

std::vector<uint8_t> decrypt_portability_data(const PortabilityKey &key, const AesKey &iv, std::span<const uint8_t> data);
std::vector<uint8_t> getDecryptedPortabilityData(const AesKey &iv, std::span<const uint8_t> data, bool debug);
PortabilityKey getEbootparamKey();

struct KeyMaterial {
    AesKey dmac5_type0;
    Hash icv_hmac;
    Hash file_hmac;
    AesKey counter_iv;
};

struct SavedataFileKeys {
    AesKey data;
    AesKey tweak;
};

struct GeneratedSavedataKey {
    AesKey key;
    std::array<uint8_t, 0x50> sealedkey;
};

struct FileEntry {
    uint32_t parent;
    std::string name;
    uint32_t index;
    uint16_t attributes;
    uint64_t size;
    Hash hash;
};

struct VerifiedFile {
    std::vector<uint8_t> ciphertext;
    Hash seed;
    uint32_t block_size;
};

struct EncodedSavedataFile {
    std::vector<uint8_t> data;
    std::vector<uint8_t> icv;
    Hash entry_hash;
};

struct InitialSavedata {
    std::vector<uint8_t> files_db;
    std::array<uint8_t, 24> directory_icv;
    EncodedSavedataFile sealedkey;
};

using OpenPsId = std::array<uint8_t, 16>;
using AccountId = std::array<uint8_t, 8>;

struct SavedataIdentity {
    OpenPsId open_psid;
    AccountId account_id;
};

struct PendingSavedataIdentity {
    SavedataIdentity identity;
    bool remove_id_dat;
};

struct SavedataMetadata {
    std::string directory;
    std::string title_id;
    SavedataIdentity identity;
};

struct PreparedSavedataDirectory {
    FileEntry entry;
    std::array<uint8_t, 24> icv;
    std::vector<uint8_t> files_db;
};

KeyMaterial make_key_material(const Hash &icv_hmac, const Hash &file_hmac, const AesKey &counter_iv);
KeyMaterial make_key_material();
AesKey prepare_np_key(const AesKey &license_key, const KeyMaterial &material);
Hash derive_np_secret(const AesKey &np_key, const KeyMaterial &material, uint32_t key_param, uint32_t index);
Hash derive_savedata_secret(const AesKey &key, uint32_t index);
SavedataFileKeys derive_savedata_file_keys(const AesKey &key, uint32_t index);

// Generate a random savedata key and authenticated version-2 sealedkey in memory.
GeneratedSavedataKey generate_savedata_key();

// Prepare an initial savedata table with sce_sys (index 1) and sealedkey (index 2).
// This only builds bytes in memory; the caller must publish and validate the complete directory.
InitialSavedata prepare_initial_savedata();

// Persist an emulated console identity under the supplied ux0 directory.
std::optional<OpenPsId> generate_open_psid();
OpenPsId get_or_create_open_psid(const std::filesystem::path &ux0);
std::optional<PendingSavedataIdentity> pending_savedata_identity(const std::filesystem::path &ux0);
void stage_savedata_identity(const std::filesystem::path &ux0, const SavedataIdentity &identity, const SavedataIdentity &previous);
void commit_savedata_identity(const std::filesystem::path &ux0);

// Create and validate a new savedata in a sibling directory, without replacing an existing target.
void create_savedata(const std::filesystem::path &root, const SavedataMetadata &metadata);

// Create an initial mode-5 appmeta PFS without savedata metadata or an existing target.
void create_initial_appmeta(const std::filesystem::path &root);

// Authenticate and decrypt a version-2 sealedkey.
AesKey unseal_savedata_key(std::span<const uint8_t> sealedkey);

// Requires an exclusive savedata lock. With no supplied key, authenticate the on-disk sealedkey.
void recover_savedata_transaction(const std::filesystem::path &root, const std::optional<AesKey> &key = std::nullopt);

// Verify HMACs in supported version-3 and version-5 files.db trees, including savedata tables.
// Version 3 authenticates the chained entry hashes, not the entry names or metadata.
// The header's RSA signature is not verified by this reader.
// Throws on an unsupported, corrupt, or unreadable database.
std::vector<FileEntry> read_file_table(const std::filesystem::path &path, const Hash &key);

// Prepare an update to an existing small savedata file entry; does not write the database.
std::vector<uint8_t> update_savedata_file_table(std::span<const uint8_t> database, const FileEntry &expected,
    uint64_t size, const Hash &file_hash, const Hash &database_key);

// Prepare insertion of a small file; does not create its data or ICV files.
std::vector<uint8_t> insert_savedata_file_table(std::span<const uint8_t> database, const FileEntry &entry, const Hash &database_key);

// Prepare a directory entry and its ICV in memory; no filesystem changes are made.
PreparedSavedataDirectory prepare_savedata_directory(std::span<const uint8_t> database, uint32_t parent,
    uint32_t index, const std::string &name, const AesKey &savedata_key);

// Prepare deletion of an existing small file; does not remove its data or ICV files.
std::vector<uint8_t> remove_savedata_file_table(std::span<const uint8_t> database, const FileEntry &expected, const Hash &database_key);

// Read and authenticate a small savedata file and its version-2 per-file ICV.
// The supplied entry must come from an authenticated files.db table.
std::vector<uint8_t> read_savedata_file(const std::filesystem::path &file_path, const std::filesystem::path &icv_path,
    const FileEntry &entry, const AesKey &key);

// Prepare a savedata file of at most 64 MiB and its ICV in memory.
// Unsupported attributes or sizes throw; this does not modify files.db or the filesystem.
EncodedSavedataFile encode_savedata_file(std::span<const uint8_t> plaintext, uint32_t index, uint16_t attributes, const AesKey &key);

// Authenticate a version-2 unicv record and each stored file block.
VerifiedFile read_verified_file(const std::filesystem::path &unicv_path, const std::filesystem::path &file_path,
    const FileEntry &entry, const Hash &file_key);

// Only pass the result of read_verified_file; this operation does not verify it again.
std::vector<uint8_t> decrypt_cbc_file(const VerifiedFile &file, const AesKey &np_key, const KeyMaterial &material);

class File {
public:
    ~File();
    File(const File &) = delete;
    File &operator=(const File &) = delete;
    uint64_t size() const;
    size_t read(uint64_t offset, void *data, size_t length);

private:
    friend class Layer;
    struct Impl;
    explicit File(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class Layer {
public:
    Layer(const std::filesystem::path &root, const AesKey &license_key, const KeyMaterial &material, Mode mode);
    Layer(const std::filesystem::path &root, const AesKey &savedata_key);
    explicit Layer(const std::filesystem::path &root);
    Mode mode() const { return mode_; }
    std::filesystem::path savedata_host_path(const std::filesystem::path &relative_path) const;
    // Use a freshly reloaded layer under the creation lock; this lookup does not reserve the index.
    uint32_t find_available_savedata_file_index() const;
    // Requires an exclusive savedata lock and a freshly reloaded layer. Reload after publication.
    // If publication throws, recover the journal before accessing the savedata again.
    void replace_savedata_file(const std::filesystem::path &relative_path, std::span<const uint8_t> plaintext) const;
    // Same locking, recovery and reload requirements as replace_savedata_file.
    void create_savedata_directory(const std::filesystem::path &relative_path) const;
    // Same locking, recovery and reload requirements as replace_savedata_file. Directories must be empty.
    void remove_savedata_file(const std::filesystem::path &relative_path) const;
    const FileEntry *find(const std::filesystem::path &relative_path) const;
    std::vector<FileEntry> children(const std::filesystem::path &relative_path) const;
    std::optional<std::vector<uint8_t>> read(const std::filesystem::path &relative_path) const;
    std::unique_ptr<File> open_file(const std::filesystem::path &relative_path) const;

private:
    std::filesystem::path root_;
    Mode mode_;
    KeyMaterial material_;
    AesKey np_key_;
    uint32_t key_param_;
    std::vector<FileEntry> entries_;
};

// Exclusive writer for the supported sealedkey-based mode-5 format (version-3 files.db).
// A failed mutation with a journal poisons the session; reopen it for recovery.
class PfsSession {
public:
    explicit PfsSession(const std::filesystem::path &root, const std::optional<AesKey> &key = std::nullopt);
    ~PfsSession();
    PfsSession(const PfsSession &) = delete;
    PfsSession &operator=(const PfsSession &) = delete;

    bool is_usable() const;
    std::optional<std::vector<uint8_t>> read(const std::filesystem::path &path) const;
    uint64_t write(const std::filesystem::path &path, uint64_t offset, std::span<const uint8_t> data, bool create, bool append,
        std::optional<uint64_t> truncate_size = std::nullopt);
    void create_directories(const std::filesystem::path &path);
    void truncate(const std::filesystem::path &path, uint64_t size);
    void remove(const std::filesystem::path &path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Returns nullptr for a layer without PFS databases; otherwise authenticates a matching license.
std::unique_ptr<Layer> open_layer(const std::filesystem::path &root,
    const std::filesystem::path &license_directory, const std::string &title_id,
    Mode mode, const std::string &content_id = {});

} // namespace pfsmgr
