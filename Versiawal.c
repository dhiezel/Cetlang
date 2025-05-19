#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

// ELF Header dan segment untuk Linux AArch64 (ARM64)
// Minimal ELF executable: 1 PT_LOAD, 1 section .text
// Syscall Linux ARM64: write = syscall 64

// Struktur ELF64 ARM64, langsung binary
// Offset text = 0x1000, entry point = 0x1000

uint8_t elf_header[] = {
    // ELF Header
    0x7f, 'E', 'L', 'F',      // Magic
    2, 1, 1, 0, 0,            // 64-bit, little-endian, v1
    [16] = 2, 0,              // Type: EXEC (Executable)
    0xb7, 0x00,              // Machine: AArch64
    1,0,0,0,                 // Version
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,   // Entry point = 0x1000
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,   // Program header table offset = 0x40
    0,0,0,0,0,0,0,0,         // Section header offset
    0,0,0,0,                 // Flags
    64,0,                   // ELF header size
    56,0, 1,0,              // Program header size, count = 1
    0,0, 0,0                // No section headers
};

// Program header
uint8_t program_header[] = {
    1,0,0,0,   // Type = PT_LOAD
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // Offset = 0
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00, // VAddr = 0x1000
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00, // PAddr = 0x1000
    // File size & mem size = placeholder (akan dihitung)
    0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
    5,0,0,0,   // Flags: RX
    0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00 // Align = 0x1000
};

// Syscall write untuk ARM64:
// mov x0, 1          // stdout
// mov x1, addr       // pointer ke pesan
// mov x2, len        // panjang pesan
// mov x8, 64         // syscall write
// svc #0

void write_syscall(FILE *f, const char *msg) {
    size_t len = strlen(msg);

    uint32_t insts[] = {
        0xd2800020, // mov x0, #1
        0x58000141, // ldr x1, =pesan (relative literal)
        0xd2800142, // mov x2, #len (max 255)
        0xd2800808, // mov x8, #64
        0xd4000001, // svc #0
    };
    insts[2] = 0xd2800000 | ((uint32_t)len << 5) | 0x02; // mov x2, #len

    fwrite(insts, sizeof(insts), 1, f);

    // tulis pesan sebagai literal (di belakang instruksi)
    fwrite(msg, len, 1, f);
    fputc('\n', f);
    fputc(0x00, f);

    // Padding agar 8-byte align
    while ((ftell(f) % 8) != 0)
        fputc(0x00, f);
}

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        printf("Usage: %s program.cetlang -o hasil\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    FILE *out = fopen(argv[3], "wb");

    if (!in || !out) {
        perror("Gagal membuka file");
        return 1;
    }

    // Placeholder: tulis header ELF & program header
    fwrite(elf_header, sizeof(elf_header), 1, out);
    long ph_offset = ftell(out);
    fwrite(program_header, sizeof(program_header), 1, out);

    // Catat offset awal segmen .text (0x1000)
    while (ftell(out) < 0x1000)
        fputc(0x00, out);

    // Baca tiap baris cetak("...")
    char line[512];
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "cetak(\"", 7) == 0 && strstr(line, "\");")) {
            char *start = strchr(line, '"') + 1;
            char *end = strrchr(line, '"');
            *end = '\0';
            write_syscall(out, start);
        }
    }

    long total_size = ftell(out);

    // Update ukuran file dan memory pada program_header
    fseek(out, ph_offset + 32, SEEK_SET);
    uint64_t size = total_size;
    fwrite(&size, sizeof(size), 1, out); // p_filesz
    fwrite(&size, sizeof(size), 1, out); // p_memsz

    fclose(in);
    fclose(out);
    chmod(argv[3], 0755);
    printf("Binary ELF ARM64 berhasil dibuat: %s\n", argv[3]);
    return 0;
}