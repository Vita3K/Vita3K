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

#pragma once

#include <cstdint>

// Base 32-bit ELF types
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Word;
typedef int32_t Elf32_Sword;

// Typedef for compatibility
typedef Elf32_Half Elf_Half;
typedef Elf32_Off Elf_Off;
typedef Elf32_Addr Elf_Addr;
typedef Elf32_Word Elf_Word;
typedef Elf32_Sword Elf_Sword;

#define ELF_NIDENT 16
typedef struct {
    uint8_t e_ident[ELF_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} Elf32_Ehdr;

enum Elf_Ident {
    EI_MAG0, // 0x7F
    EI_MAG1, //'E'
    EI_MAG2, //'L'
    EI_MAG3, //'F'
    EI_CLASS, // Size class - 32 or 64-bit
    EI_DATA, // Byte ordering - little/big endian
    EI_VERSION, // ELF version
    EI_OSABI, // OS ABI
    EI_ABIVERSION, // OS ABI version
    EI_PAD // Padding
};

// Expected values for e_ident[EI_MAGx]
#define ELFMAG0 (0x7F)
#define ELFMAG1 ('E')
#define ELFMAG2 ('L')
#define ELFMAG3 ('F')

// Possible values for e_ident[EI_CLASS]
#define ELFCLASSNONE (0)
#define ELFCLASS32 (1)
#define ELFCLASS64 (2)

// Possible values for e_ident[EI_DATA]
#define ELFDATANONE (0)
#define ELFDATA2LSB (1)
#define ELFDATA2MSB (2)

// Expected value for e_ident[EI_VERSION] and e_version
#define EV_CURRENT (1)

// Expected value for e_ident[EI_OSABI]
#define ELFOSABI_NONE (0)

// Possible values for e_type
#define ET_SCE_EXEC (0xFE00) // SCE non-relocatable executable
#define ET_SCE_RELEXEC (0xFE04) // SCE relocatable executable
#define ET_SCE_PSP2RELEXEC (0xFFA5) // Old SCE relocatable format (unsupported)

// Expected value for e_machine
#define EM_ARM (0x28)

// Evaluates to true if the EI_MAGx fields in a Elf32_Ehdr are valid
#define EHDR_HAS_VALID_MAGIC(ehdr) ( \
    ((ehdr.e_ident[EI_MAG0]) == ELFMAG0) && ((ehdr.e_ident[EI_MAG1]) == ELFMAG1) && ((ehdr.e_ident[EI_MAG2]) == ELFMAG2) && ((ehdr.e_ident[EI_MAG3]) == ELFMAG3))

typedef struct {
    uint32_t p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} Elf32_Phdr;

// Possible values for p_type
#define PT_NULL (0x0U) // Unused entry - skip
#define PT_LOAD (0x1U) // Loadable segment
#define PT_SCE_RELA (0x60000000U) // Relocations
#define PT_SCE_COMMENT (0x6FFFFF00U) // Compiler signature?
#define PT_SCE_VERSION (0x6FFFFF01U) // SDK signature?
#define PT_ARM_EXIDX (0x70000001U)

#define PT_LOOS (0x60000000U) // Lowest OS-specific value
#define PT_HIOS (0x6FFFFFFFU) // Highest OS-specific value
#define PT_LOPROC (0x70000000U) // Lowest processor-specific value
#define PT_HIPROC (0x7FFFFFFFU) // Highest processor-specific value

// TODO: possible values for p_flags
