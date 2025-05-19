#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Konstanta ELF
#define ELF_MAGIC_BYTE0 0x7f
#define ELF_MAGIC_BYTE1 'E'
#define ELF_MAGIC_BYTE2 'L'
#define ELF_MAGIC_BYTE3 'F'
#define ELFCLASS64      2
#define ELFDATA2LSB     1
#define EV_CURRENT      1
#define EM_AARCH64      183
#define ET_EXEC         2
#define PH_OFFSET_OFFSET 32
#define PHENTSIZE_OFFSET 44
#define PHNUM_OFFSET    46
#define PT_LOAD         1
#define PF_X            1
#define PF_R            4

// Konstanta Aplikasi
#define ENTRY       0x1000
#define CODE_OFFSET 0x1000
#define DATA_OFFSET 0x2000
#define PAGE_SIZE   0x1000
#define MAX_TEXT_LEN 255

// Struktur untuk ELF Header
typedef struct {
    uint8_t     e_ident[16];
    uint16_t    e_type;
    uint16_t    e_machine;
    uint32_t    e_version;
    uint64_t    e_entry;
    uint64_t    e_phoff;
    uint64_t    e_shoff;
    uint32_t    e_flags;
    uint16_t    e_ehsize;
    uint16_t    e_phentsize;
    uint16_t    e_phnum;
    uint16_t    e_shentsize;
    uint16_t    e_shnum;
    uint16_t    e_shstrndx;
} Elf64_Ehdr;

// Struktur untuk Program Header
typedef struct {
    uint32_t    p_type;
    uint32_t    p_flags;
    uint64_t    p_offset;
    uint64_t    p_vaddr;
    uint64_t    p_paddr;
    uint64_t    p_filesz;
    uint64_t    p_memsz;
    uint64_t    p_align;
} Elf64_Phdr;

uint8_t code[] = {
    0x20, 0x00, 0x80, 0xd2, // mov x0, #1
    0x42, 0x00, 0x80, 0xd2, // mov x2, #len (placeholder)
    0x01, 0x00, 0x00, 0x58, // ldr x1, =data (placeholder)
    0x88, 0x08, 0x80, 0xd2, // mov x8, #64
    0x01, 0x00, 0x00, 0xd4, // svc #0 (write)
    0x00, 0x00, 0x80, 0xd2, // mov x0, #0
    0x88, 0x0b, 0x80, 0xd2, // mov x8, #93
    0x01, 0x00, 0x00, 0xd4  // svc #0 (exit)
};

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        fprintf(stderr, "Usage: %s input.cetlang -o output\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    if (!in) {
        perror("fopen input");
        return 1;
    }

    char text[MAX_TEXT_LEN + 1] = {0}; // +1 untuk null terminator
    char line[256];
    size_t current_text_len = 0;
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "cetak(\"", 7) == 0) {
            char *start = line + 7;
            char *end = strchr(start, '"');
            if (end) {
                size_t len = end - start;
                if (current_text_len + len + 1 < MAX_TEXT_LEN + 1) {
                    strncpy(text + current_text_len, start, len);
                    current_text_len += len;
                    if (current_text_len < MAX_TEXT_LEN) {
                        text[current_text_len++] = '\n';
                    }
                } else {
                    fprintf(stderr, "Peringatan: Teks melebihi batas maksimum (%d karakter), akan dipotong.\n", MAX_TEXT_LEN);
                    break;
                }
            } else {
                fprintf(stderr, "Error: Petik penutup tidak ditemukan pada perintah cetak.\n");
                fclose(in);
                return 1;
            }
        }
    }
    fclose(in);

    size_t text_len = strlen(text);
    code[4] = (uint8_t)text_len;

    // LDR literal patch
    int32_t ldr_off = DATA_OFFSET - (ENTRY + 12);
    *(uint32_t*)(code + 8) = 0x58000001 | (((ldr_off >> 2) & 0x7ffff) << 5);

    FILE *out = fopen(argv[3], "wb");
    if (!out) {
        perror("fopen output");
        return 1;
    }

    // === ELF Header ===
    Elf64_Ehdr elf_hdr = {
        .e_ident = {ELF_MAGIC_BYTE0, ELF_MAGIC_BYTE1, ELF_MAGIC_BYTE2, ELF_MAGIC_BYTE3,
                    ELFCLASS64, ELFDATA2LSB, EV_CURRENT, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .e_type = ET_EXEC,
        .e_machine = EM_AARCH64,
        .e_version = EV_CURRENT,
        .e_entry = ENTRY,
        .e_phoff = 64,
        .e_shoff = 0,
        .e_flags = 0,
        .e_ehsize = sizeof(Elf64_Ehdr),
        .e_phentsize = sizeof(Elf64_Phdr),
        .e_phnum = 1,
        .e_shentsize = 0,
        .e_shnum = 0,
        .e_shstrndx = 0
    };
    fwrite(&elf_hdr, 1, sizeof(elf_hdr), out);

    // === Program Header ===
    Elf64_Phdr prog_hdr = {
        .p_type = PT_LOAD,
        .p_flags = PF_X | PF_R,
        .p_offset = 0,
        .p_vaddr = CODE_OFFSET,
        .p_paddr = CODE_OFFSET,
        .p_filesz = DATA_OFFSET + text_len,
        .p_memsz = DATA_OFFSET + text_len,
        .p_align = PAGE_SIZE
    };
    fwrite(&prog_hdr, 1, sizeof(prog_hdr), out);

    // === Pad sampai ke offset 0x1000 ===
    fseek(out, CODE_OFFSET, SEEK_SET);
    fwrite(code, 1, sizeof(code), out);

    // === Pad ke DATA offset ===
    fseek(out, DATA_OFFSET, SEEK_SET);
    fwrite(text, 1, text_len, out);

    fclose(out);
    if (chmod(argv[3], 0755) != 0) {
        perror("chmod");
        return 1;
    }
    printf("Binary ELF ARM64 berhasil dibuat: %s\n", argv[3]);
    return 0;
}