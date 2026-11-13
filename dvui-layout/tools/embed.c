// Malý nástroj pro build: převede binární soubor na C pole.
// Náhrada za C23 #embed, které zatím neumí všechny cílové překladače.
//
// Použití: embed <vstup> <výstup.c> <jméno_symbolu>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <input> <output.c> <symbol>\n", argv[0]);
        return 1;
    }
    FILE* in = fopen(argv[1], "rb");
    if (!in) {
        fprintf(stderr, "embed: cannot open %s\n", argv[1]);
        return 1;
    }
    FILE* out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "embed: cannot create %s\n", argv[2]);
        fclose(in);
        return 1;
    }
    fprintf(out, "// Vygenerováno nástrojem tools/embed.c z %s – neupravovat.\n", argv[1]);
    fprintf(out, "#include <stdalign.h>\n#include <stddef.h>\n\n");
    // zarovnání kvůli SPIR-V (slova po 4 bajtech)
    fprintf(out, "alignas(8) const unsigned char %s[] = {", argv[3]);
    size_t n = 0;
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (n % 16 == 0) fprintf(out, "\n   ");
        fprintf(out, " 0x%02x,", c);
        n++;
    }
    // prázdný soubor by dal prázdné pole, které C nedovoluje
    if (n == 0) fprintf(out, " 0x00");
    fprintf(out, "\n};\nconst size_t %s_size = %zu;\n", argv[3], n);
    fclose(in);
    if (fclose(out) != 0) return 1;
    return 0;
}
