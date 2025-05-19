#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>

// ELF Header (64-bit, ARM64)
uint8_t elf_header[] = {
    0x7f, 'E', 'L', 'F',     // Magic
    2, 1, 1, 0,              // 64-bit, little endian
    0, 0, 0, 0, 0, 0, 0, 0,  // Padding
    2, 0,                   // e_type = EXEC (2)
    0xb7, 0x00,             // e_machine = AARCH64 (0x00B7) — INI YANG KRUSIAL
    1, 0, 0, 0,             // e_version
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // e_entry
    0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // e_phoff
    0, 0, 0, 0, 0, 0, 0, 0,                         // e_shoff
    0, 0, 0, 0,                                     // e_flags
    64, 0,                                          // e_ehsize
    56, 0,                                          // e_phentsize
    1, 0,                                           // e_phnum
    0, 0,                                           // e_shentsize
    0, 0                                            // e_shnum
};

// Program Header
uint8_t program_header[56] = {
    1, 0, 0, 0,              // p_type = LOAD
    0, 0, 0, 0, 0, 0, 0, 0,  // p_offset
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // p_vaddr
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // p_paddr
    0, 0, 0, 0, 0, 0, 0, 0,  // p_filesz (set nanti)
    0, 0, 0, 0, 0, 0, 0, 0,  // p_memsz (set nanti)
    5, 0, 0, 0,              // Flags: RX
    0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // Align
};

// Tulis instruksi syscall write untuk mencetak teks
void write_syscall(FILE *f, const char *msg, uint64_t base_addr) {
    size_t len = strlen(msg);
    long pos = ftell(f);
    uint64_t addr_msg = base_addr + pos + 20;

    uint32_t inst0 = 0xd2800020; // mov x0, #1
    int32_t offset = (addr_msg - (base_addr + pos + 4)) >> 2;
    uint32_t inst1 = 0x10000000 | ((offset & 0x7FFFF) << 5) | 1; // adr x1, msg
    uint32_t inst2 = 0xd2800000 | ((uint32_t)len << 5) | 2; // mov x2, #len
    uint32_t inst3 = 0xd2800808; // mov x8, #64
    uint32_t inst4 = 0xd4000001; // svc #0

    fwrite(&inst0, 4, 1, f);
    fwrite(&inst1, 4, 1, f);
    fwrite(&inst2, 4, 1, f);
    fwrite(&inst3, 4, 1, f);
    fwrite(&inst4, 4, 1, f);
    fwrite(msg, len, 1, f);
    fputc('\n', f);
    fputc(0x00, f);

    while (ftell(f) % 8 != 0)
        fputc(0x00, f);
}

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        printf("Usage: %s input.cetlang -o output\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[3], "wb");

    if (!in || !out) {
        perror("Gagal membuka file");
        return 1;
    }

    fwrite(elf_header, sizeof(elf_header), 1, out);
    long ph_offset = ftell(out);
    fwrite(program_header, sizeof(program_header), 1, out);

    while (ftell(out) < 0x1000)
        fputc(0x00, out);

    char line[512];
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "cetak(\"", 7) == 0 && strstr(line, "\");")) {
            char *start = strchr(line, '"') + 1;
            char *end = strrchr(line, '"');
            if (start && end && end > start)
                *end = '\0';
            write_syscall(out, start, 0x1000);
        }
    }

    long total = ftell(out);
    fseek(out, ph_offset + 32, SEEK_SET);
    fwrite(&total, 8, 1, out); // filesz
    fwrite(&total, 8, 1, out); // memsz

    fclose(in);
    fclose(out);
    chmod(argv[3], 0755);
    printf("Binary ELF ARM64 berhasil dibuat: %s\n", argv[3]);
    return 0;
}