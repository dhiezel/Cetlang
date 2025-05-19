#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define ENTRY       0x1000
#define CODE_OFFSET 0x1000
#define DATA_OFFSET 0x2000
#define PAGE_SIZE   0x1000

uint8_t code[] = {
    0x20, 0x00, 0x80, 0xd2, // mov x0, #1
    0x42, 0x00, 0x80, 0xd2, // mov x2, #len (placeholder)
    0x01, 0x00, 0x00, 0x58, // ldr x1, =data (placeholder)
    0x88, 0x08, 0x80, 0xd2, // mov x8, #64
    0x01, 0x00, 0x00, 0xd4, // svc #0
    0x00, 0x00, 0x80, 0xd2, // mov x0, #0
    0x88, 0x0b, 0x80, 0xd2, // mov x8, #93
    0x01, 0x00, 0x00, 0xd4  // svc #0
};

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[2], "-o") != 0) {
        fprintf(stderr, "Usage: %s input.cetlang -o output\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    if (!in) { perror("open input"); return 1; }

    char text[256] = {0};
    char line[256];
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "cetak(\"", 7) == 0) {
            char *start = line + 7;
            char *end = strchr(start, '"');
            if (end) *end = '\0';
            strcat(text, start);
            strcat(text, "\n");
        }
    }
    fclose(in);

    size_t text_len = strlen(text);
    if (text_len > 255) text_len = 255;
    code[4] = (uint8_t)text_len;

    // LDR literal patch
    int32_t ldr_off = DATA_OFFSET - (ENTRY + 12);
    *(uint32_t*)(code + 8) = 0x58000001 | (((ldr_off >> 2) & 0x7ffff) << 5);

    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror("open output"); return 1; }

    // === ELF Header ===
    uint8_t elf[64] = {0};
    elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2; // 64-bit
    elf[5] = 1; // little endian
    elf[6] = 1; // version
    elf[16] = 2; // ET_EXEC
    elf[18] = 0xb7; // EM_AARCH64
    elf[20] = 1; // EV_CURRENT
    *(uint64_t*)(elf + 24) = ENTRY; // entry point
    *(uint64_t*)(elf + 32) = 64;    // program header offset
    *(uint16_t*)(elf + 40) = 0;     // section header offset
    *(uint16_t*)(elf + 42) = 64;    // elf header size
    *(uint16_t*)(elf + 44) = 56;    // program header entry size
    *(uint16_t*)(elf + 46) = 1;     // program header num

    fwrite(elf, 1, 64, out);

    // === Program Header ===
    uint8_t ph[56] = {0};
    *(uint32_t*)(ph + 0) = 1;         // PT_LOAD
    *(uint32_t*)(ph + 4) = 5 | 1;     // PF_X | PF_R
    *(uint64_t*)(ph + 8)  = 0;        // offset
    *(uint64_t*)(ph + 16) = CODE_OFFSET;
    *(uint64_t*)(ph + 24) = CODE_OFFSET;
    *(uint64_t*)(ph + 32) = DATA_OFFSET + text_len;
    *(uint64_t*)(ph + 40) = DATA_OFFSET + text_len;
    *(uint64_t*)(ph + 48) = PAGE_SIZE;

    fwrite(ph, 1, 56, out);

    // === Pad sampai ke offset 0x1000 ===
    fseek(out, CODE_OFFSET, SEEK_SET);
    fwrite(code, 1, sizeof(code), out);

    // === Pad ke DATA offset ===
    fseek(out, DATA_OFFSET, SEEK_SET);
    fwrite(text, 1, text_len, out);

    fclose(out);
    chmod(argv[3], 0755);
    printf("Binary ELF ARM64 berhasil dibuat: %s\n", argv[3]);
    return 0;
}