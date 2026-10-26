/* langgen: turn the readable language layer into compilable period code.
 *
 * Game sources carry the original English literal wrapped in
 *     TRANSLATE("English", "RU")
 * and tools/localization/data/ru.lang holds the Russian text, tab-separated:
 *     FILE.C <TAB> line <TAB> english-field <TAB> russian-field
 * A field is a readable C-string body: raw UTF-8 text plus the usual \n \t \"
 * \\ \xNN escape texts.  langgen resolves every TRANSLATE call (keyed by file,
 * line and the English bytes) and writes the same file with the call replaced
 * by the Russian string escaped to plain CP866 \xNN bytes.  The output is pure
 * ASCII inside literals, so MSVC and GCC build it without any charset option;
 * comments pass through untouched.  A file without TRANSLATE is copied byte for
 * byte; a TRANSLATE with no ru.lang entry keeps English and warns.  An empty
 * russian field is the empty string "" (the pack really blanks such slots);
 * keeping English is done by not wrapping the literal at all.
 *
 * Usage: langgen <ru.lang> <input.C> <output.C>
 */
#define _CRT_SECURE_NO_WARNINGS
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Unicode code point -> CP866 byte for the 128 non-ASCII CP866 positions. */
static const struct { unsigned cp; unsigned char b; } cp866[] = {
    { 0x0410, 0x80 }, { 0x0411, 0x81 }, { 0x0412, 0x82 }, { 0x0413, 0x83 },
    { 0x0414, 0x84 }, { 0x0415, 0x85 }, { 0x0416, 0x86 }, { 0x0417, 0x87 },
    { 0x0418, 0x88 }, { 0x0419, 0x89 }, { 0x041A, 0x8A }, { 0x041B, 0x8B },
    { 0x041C, 0x8C }, { 0x041D, 0x8D }, { 0x041E, 0x8E }, { 0x041F, 0x8F },
    { 0x0420, 0x90 }, { 0x0421, 0x91 }, { 0x0422, 0x92 }, { 0x0423, 0x93 },
    { 0x0424, 0x94 }, { 0x0425, 0x95 }, { 0x0426, 0x96 }, { 0x0427, 0x97 },
    { 0x0428, 0x98 }, { 0x0429, 0x99 }, { 0x042A, 0x9A }, { 0x042B, 0x9B },
    { 0x042C, 0x9C }, { 0x042D, 0x9D }, { 0x042E, 0x9E }, { 0x042F, 0x9F },
    { 0x0430, 0xA0 }, { 0x0431, 0xA1 }, { 0x0432, 0xA2 }, { 0x0433, 0xA3 },
    { 0x0434, 0xA4 }, { 0x0435, 0xA5 }, { 0x0436, 0xA6 }, { 0x0437, 0xA7 },
    { 0x0438, 0xA8 }, { 0x0439, 0xA9 }, { 0x043A, 0xAA }, { 0x043B, 0xAB },
    { 0x043C, 0xAC }, { 0x043D, 0xAD }, { 0x043E, 0xAE }, { 0x043F, 0xAF },
    { 0x2591, 0xB0 }, { 0x2592, 0xB1 }, { 0x2593, 0xB2 }, { 0x2502, 0xB3 },
    { 0x2524, 0xB4 }, { 0x2561, 0xB5 }, { 0x2562, 0xB6 }, { 0x2556, 0xB7 },
    { 0x2555, 0xB8 }, { 0x2563, 0xB9 }, { 0x2551, 0xBA }, { 0x2557, 0xBB },
    { 0x255D, 0xBC }, { 0x255C, 0xBD }, { 0x255B, 0xBE }, { 0x2510, 0xBF },
    { 0x2514, 0xC0 }, { 0x2534, 0xC1 }, { 0x252C, 0xC2 }, { 0x251C, 0xC3 },
    { 0x2500, 0xC4 }, { 0x253C, 0xC5 }, { 0x255E, 0xC6 }, { 0x255F, 0xC7 },
    { 0x255A, 0xC8 }, { 0x2554, 0xC9 }, { 0x2569, 0xCA }, { 0x2566, 0xCB },
    { 0x2560, 0xCC }, { 0x2550, 0xCD }, { 0x256C, 0xCE }, { 0x2567, 0xCF },
    { 0x2568, 0xD0 }, { 0x2564, 0xD1 }, { 0x2565, 0xD2 }, { 0x2559, 0xD3 },
    { 0x2558, 0xD4 }, { 0x2552, 0xD5 }, { 0x2553, 0xD6 }, { 0x256B, 0xD7 },
    { 0x256A, 0xD8 }, { 0x2518, 0xD9 }, { 0x250C, 0xDA }, { 0x2588, 0xDB },
    { 0x2584, 0xDC }, { 0x258C, 0xDD }, { 0x2590, 0xDE }, { 0x2580, 0xDF },
    { 0x0440, 0xE0 }, { 0x0441, 0xE1 }, { 0x0442, 0xE2 }, { 0x0443, 0xE3 },
    { 0x0444, 0xE4 }, { 0x0445, 0xE5 }, { 0x0446, 0xE6 }, { 0x0447, 0xE7 },
    { 0x0448, 0xE8 }, { 0x0449, 0xE9 }, { 0x044A, 0xEA }, { 0x044B, 0xEB },
    { 0x044C, 0xEC }, { 0x044D, 0xED }, { 0x044E, 0xEE }, { 0x044F, 0xEF },
    { 0x0401, 0xF0 }, { 0x0451, 0xF1 }, { 0x0404, 0xF2 }, { 0x0454, 0xF3 },
    { 0x0407, 0xF4 }, { 0x0457, 0xF5 }, { 0x040E, 0xF6 }, { 0x045E, 0xF7 },
    { 0x00B0, 0xF8 }, { 0x2219, 0xF9 }, { 0x00B7, 0xFA }, { 0x221A, 0xFB },
    { 0x2116, 0xFC }, { 0x00A4, 0xFD }, { 0x25A0, 0xFE }, { 0x00A0, 0xFF },
};

static int cp_to_cp866(unsigned cp)
{
    size_t i;
    for (i = 0; i < sizeof cp866 / sizeof *cp866; i++)
        if (cp866[i].cp == cp)
            return cp866[i].b;
    return -1;
}

/* ---------------- byte buffers ---------------- */
struct buf { unsigned char *d; size_t n, cap; };

static void buf_put(struct buf *b, int c)
{
    if (b->n == b->cap) {
        b->cap = b->cap ? b->cap * 2 : 256;
        b->d = realloc(b->d, b->cap);
        if (!b->d)
            exit(2);
    }
    b->d[b->n++] = (unsigned char) c;
}

static void buf_putn(struct buf *b, const void *p, size_t n)
{
    size_t i;
    const unsigned char *s = p;
    for (i = 0; i < n; i++)
        buf_put(b, s[i]);
}

/* ---------------- ru.lang entries ---------------- */
struct entry {
    char file[64];
    int line;
    struct buf en; /* decoded English bytes */
    struct buf ru; /* decoded CP866 bytes */
};

static struct entry *entries;
static int n_entries, cap_entries;

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a language-file field.  ru=1 maps raw UTF-8 text to CP866; escape
 * bytes pass through literally.  Returns 0 on success. */
static int decode_field(const unsigned char *s, size_t n, struct buf *out, int ru,
                        const char *what)
{
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        if (c == '\\') {
            if (i + 1 >= n) {
                fprintf(stderr, "langgen: dangling backslash in %s\n", what);
                return 1;
            }
            switch (s[i + 1]) {
            case 'n': buf_put(out, 0x0A); i += 2; break;
            case 't': buf_put(out, 0x09); i += 2; break;
            case 'r': buf_put(out, 0x0D); i += 2; break;
            case 'a': buf_put(out, 0x07); i += 2; break;
            case 'b': buf_put(out, 0x08); i += 2; break;
            case 'f': buf_put(out, 0x0C); i += 2; break;
            case 'v': buf_put(out, 0x0B); i += 2; break;
            case '"': buf_put(out, '"');  i += 2; break;
            case '\'': buf_put(out, '\''); i += 2; break;
            case '\\': buf_put(out, '\\'); i += 2; break;
            case 'x': {
                int hi = i + 2 < n ? hexval(s[i + 2]) : -1;
                int lo = i + 3 < n ? hexval(s[i + 3]) : -1;
                if (hi < 0 || lo < 0) {
                    fprintf(stderr, "langgen: bad \\x in %s\n", what);
                    return 1;
                }
                buf_put(out, (hi << 4) | lo);
                i += 4;
                break;
            }
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                /* octal, up to three digits - the same rule as in C literals */
                int v = 0, k = 1;
                while (k <= 3 && i + k < n && s[i + k] >= '0' && s[i + k] <= '7') {
                    v = v * 8 + (s[i + k] - '0');
                    k++;
                }
                buf_put(out, v & 0xFF);
                i += k;
                break;
            }
            default:
                fprintf(stderr, "langgen: bad escape \\%c in %s\n", s[i + 1], what);
                return 1;
            }
            continue;
        }
        if (c < 0x80) {
            buf_put(out, c);
            i++;
            continue;
        }
        if (!ru) {
            buf_put(out, c); /* English fields are ASCII; keep odd bytes as-is */
            i++;
            continue;
        }
        /* raw UTF-8 sequence -> code point -> CP866 byte */
        unsigned cp;
        size_t len;
        if (c >= 0xC0 && c < 0xE0 && i + 1 < n) { cp = (c & 0x1F) << 6; len = 2; }
        else if (c >= 0xE0 && c < 0xF0 && i + 2 < n) { cp = (c & 0x0F) << 12; len = 3; }
        else {
            fprintf(stderr, "langgen: non-UTF8 byte %02X in %s\n", c, what);
            return 1;
        }
        size_t k;
        for (k = 1; k < len; k++) {
            if ((s[i + k] & 0xC0) != 0x80) {
                fprintf(stderr, "langgen: bad UTF8 in %s\n", what);
                return 1;
            }
            cp |= (unsigned) (s[i + k] & 0x3F) << (6 * (len - 1 - k));
        }
        int b = cp_to_cp866(cp);
        if (b < 0) {
            fprintf(stderr, "langgen: U+%04X has no CP866 byte in %s\n", cp, what);
            return 1;
        }
        buf_put(out, b);
        i += len;
    }
    return 0;
}

static void load_lang(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "langgen: cannot open %s\n", path);
        exit(2);
    }
    int lineno = 0;
    unsigned char line[8192];
    while (fgets((char *) line, sizeof line, f)) {
        lineno++;
        size_t n = strlen((char *) line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            n--;
        line[n] = '\0';
        if (!n || line[0] == '#')
            continue;
        unsigned char *f1 = line, *f2 = strchr((char *) f1, '\t');
        if (!f2) { fprintf(stderr, "langgen: %s:%d: not tab-separated\n", path, lineno); exit(2); }
        *f2++ = 0;
        unsigned char *f3 = strchr((char *) f2, '\t');
        if (!f3) { fprintf(stderr, "langgen: %s:%d: missing english field\n", path, lineno); exit(2); }
        *f3++ = 0;
        unsigned char *f4 = strchr((char *) f3, '\t');
        if (!f4) { fprintf(stderr, "langgen: %s:%d: missing russian field\n", path, lineno); exit(2); }
        *f4++ = 0;
        if (n_entries == cap_entries) {
            cap_entries = cap_entries ? cap_entries * 2 : 256;
            entries = realloc(entries, cap_entries * sizeof *entries);
            if (!entries)
                exit(2);
        }
        struct entry *e = &entries[n_entries++];
        memset(e, 0, sizeof *e);
        snprintf(e->file, sizeof e->file, "%s", (char *) f1);
        e->line = atoi((char *) f2);
        if (decode_field(f3, strlen((char *) f3), &e->en, 0, path) ||
            decode_field(f4, strlen((char *) f4), &e->ru, 1, path))
            exit(2);
    }
    fclose(f);
}

/* ---------------- C literal helpers ---------------- */

/* Decode a C string body (source bytes between the quotes) to raw bytes. */
static int decode_c_body(const unsigned char *s, size_t n, struct buf *out,
                         const char *file, int line)
{
    size_t i = 0;
    while (i < n) {
        unsigned char c = s[i];
        if (c != '\\') {
            if (c >= 0x80) {
                fprintf(stderr, "langgen: %s:%d: raw non-ASCII byte in literal - "
                        "put such text in ru.lang\n", file, line);
                return 1;
            }
            buf_put(out, c);
            i++;
            continue;
        }
        if (i + 1 >= n) return 1;
        unsigned char e = s[i + 1];
        switch (e) {
        case 'n': buf_put(out, 0x0A); i += 2; break;
        case 't': buf_put(out, 0x09); i += 2; break;
        case 'r': buf_put(out, 0x0D); i += 2; break;
        case 'a': buf_put(out, 0x07); i += 2; break;
        case 'b': buf_put(out, 0x08); i += 2; break;
        case 'f': buf_put(out, 0x0C); i += 2; break;
        case 'v': buf_put(out, 0x0B); i += 2; break;
        case '"': buf_put(out, '"'); i += 2; break;
        case '\'': buf_put(out, '\''); i += 2; break;
        case '\\': buf_put(out, '\\'); i += 2; break;
        case '?': buf_put(out, '?'); i += 2; break;
        case 'x': {
            int hi = i + 2 < n ? hexval(s[i + 2]) : -1;
            int lo = i + 3 < n ? hexval(s[i + 3]) : -1;
            if (hi < 0 || lo < 0) return 1;
            buf_put(out, (hi << 4) | lo);
            i += 4;
            break;
        }
        default:
            if (e >= '0' && e <= '7') { /* octal, up to three digits */
                int v = 0, k = 1;
                while (k <= 3 && i + k < n && s[i + k] >= '0' && s[i + k] <= '7') {
                    v = v * 8 + (s[i + k] - '0');
                    k++;
                }
                buf_put(out, v & 0xFF);
                i += k;
            } else {
                return 1;
            }
        }
    }
    return 0;
}

/* Emit a byte string as a C literal: the same rules the translation pipeline
 * uses - printable ASCII stays readable, 0x0A / quote / backslash become the
 * two-character escape texts \n \" \\, everything else \xNN.  A literal is
 * split (" " between the parts) when a one-character plain byte is a hex
 * digit and the previous byte emitted an escape, so the digit cannot extend
 * it. */
static void emit_literal(struct buf *out, const struct buf *data)
{
    static const char hexd[] = "0123456789ABCDEF";
    static const char hexset[] = "0123456789abcdefABCDEF";
    int last_esc = 0;
    size_t i;
    buf_put(out, '"');
    for (i = 0; i < data->n; i++) {
        unsigned char c = data->d[i];
        char plain[3];
        int plen = 0;
        if (c >= 0x20 && c <= 0x7E) {
            plain[plen++] = (char) c;
        } else if (c == 0x0A) {
            plain[0] = '\\'; plain[1] = 'n'; plen = 2;
        } else if (c == '"' || c == '\\') {
            plain[0] = '\\'; plain[1] = (char) c; plen = 2;
        }
        if (plen) {
            if (plen == 1 && last_esc && strchr(hexset, plain[0])) {
                buf_put(out, '"');
                buf_put(out, ' ');
                buf_put(out, '"');
                last_esc = 0;
            }
            buf_putn(out, plain, (size_t) plen);
            last_esc = 0;
        } else {
            buf_put(out, '\\');
            buf_put(out, 'x');
            buf_put(out, hexd[(c >> 4) & 0xF]);
            buf_put(out, hexd[c & 0xF]);
            last_esc = 1;
        }
    }
    buf_put(out, '"');
}

/* ---------------- source scan ---------------- */

static const char *basename_of(const char *p)
{
    const char *s = p, *c;
    for (c = p; *c; c++)
        if (*c == '/' || *c == '\\')
            s = c + 1;
    return s;
}

static int same_bytes(const struct buf *a, const struct buf *b)
{
    if (a->n != b->n)
        return 0;
    return memcmp(a->d, b->d, a->n) == 0;
}

static size_t skip_ws(const struct buf *b, size_t j)
{
    while (j < b->n && (b->d[j] == ' ' || b->d[j] == '\t' ||
                        (b->d[j] >= 11 && b->d[j] <= 13)))
        j++;
    return j;
}

/* Match TRANSLATE("...", "RU") at i; on success set body0/body1 (the EN
 * literal's quotes) and end (past ')').  1 = matched, -1 = TRANSLATE( called
 * but malformed, 0 = not a call (plain TRANSLATE_some_identifier). */
static int parse_translate(const struct buf *src, size_t i,
                           size_t *body0, size_t *body1, size_t *end)
{
    size_t j = skip_ws(src, i + 9);
    if (j >= src->n || src->d[j] != '(')
        return 0;
    j = skip_ws(src, j + 1);
    if (j >= src->n || src->d[j] != '"')
        return -1;
    *body0 = ++j;
    while (j < src->n) {
        if (src->d[j] == '\\') { j += 2; continue; }
        if (src->d[j] == '"')
            break;
        j++;
    }
    if (j >= src->n)
        return -1;
    *body1 = j;
    j = skip_ws(src, j + 1);
    if (j >= src->n || src->d[j] != ',')
        return -1;
    j = skip_ws(src, j + 1);
    if (j + 3 >= src->n || src->d[j] != '"' || src->d[j + 1] != 'R' ||
        src->d[j + 2] != 'U' || src->d[j + 3] != '"')
        return -1;
    j = skip_ws(src, j + 4);
    if (j >= src->n || src->d[j] != ')')
        return -1;
    *end = j + 1;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: langgen <ru.lang> <input.C> <output.C>\n");
        return 2;
    }
    load_lang(argv[1]);

    const char *src_path = argv[2];
    FILE *f = fopen(src_path, "rb");
    if (!f) {
        fprintf(stderr, "langgen: cannot open %s\n", src_path);
        return 2;
    }
    struct buf src = { 0 };
    int c;
    while ((c = fgetc(f)) != EOF)
        buf_put(&src, c);
    fclose(f);

    const char *base = basename_of(src_path);
    struct buf out = { 0 };
    size_t i = 0;
    int line = 1;

    while (i < src.n) {
        unsigned char c = src.d[i];

        if (c == '/' && i + 1 < src.n && src.d[i + 1] == '/') {
            while (i < src.n && src.d[i] != '\n')
                buf_put(&out, src.d[i++]);
            continue;
        }
        if (c == '/' && i + 1 < src.n && src.d[i + 1] == '*') {
            buf_put(&out, src.d[i++]); buf_put(&out, src.d[i++]);
            while (i < src.n) {
                if (src.d[i] == '*' && i + 1 < src.n && src.d[i + 1] == '/') {
                    buf_put(&out, src.d[i++]); buf_put(&out, src.d[i++]);
                    break;
                }
                if (src.d[i] == '\n')
                    line++;
                buf_put(&out, src.d[i++]);
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            unsigned char q = c;
            buf_put(&out, src.d[i++]);
            while (i < src.n) {
                if (src.d[i] == '\\') {
                    buf_put(&out, src.d[i++]);
                    if (i < src.n)
                        buf_put(&out, src.d[i++]);
                    continue;
                }
                if (src.d[i] >= 0x80) {
                    fprintf(stderr, "langgen: %s:%d: raw non-ASCII byte in a literal\n",
                            base, line);
                    return 1;
                }
                buf_put(&out, src.d[i++]);
                if (src.d[i - 1] == q)
                    break;
            }
            continue;
        }

        if (c == 'T' && i + 9 <= src.n && memcmp(src.d + i, "TRANSLATE", 9) == 0 &&
            (i == 0 || !(isalnum((unsigned char) src.d[i - 1]) || src.d[i - 1] == '_'))) {
            size_t body0, body1, end;
            int k, r = parse_translate(&src, i, &body0, &body1, &end);
            if (r < 0) {
                fprintf(stderr, "langgen: %s:%d: malformed TRANSLATE call\n", base, line);
                return 1;
            }
            if (r > 0) {         /* r == 0: a plain TRANSLATE_some_identifier */
                struct buf en = { 0 };
                if (decode_c_body(src.d + body0, body1 - body0, &en, base, line)) {
                    fprintf(stderr, "langgen: %s:%d: bad TRANSLATE literal\n", base, line);
                    return 1;
                }
                struct entry *e;
                for (k = 0; k < n_entries; k++) {
                    e = &entries[k];
                    if (e->line == line && strcmp(e->file, base) == 0 &&
                        same_bytes(&e->en, &en)) {
                        emit_literal(&out, &e->ru);
                        break;
                    }
                }
                if (k == n_entries) {
                    fprintf(stderr, "langgen: %s:%d: no ru.lang entry for TRANSLATE, "
                            "keeping English\n", base, line);
                    buf_putn(&out, src.d + body0 - 1, body1 - body0 + 2);
                }
                free(en.d);
                i = end;
                continue;
            }
        }

        if (c == '\n')
            line++;
        buf_put(&out, src.d[i++]);
    }

    /* Compare with the existing output so unchanged inputs do not churn
     * timestamps (a rebuild with an untouched ru.lang stays a no-op). */
    unsigned char *old = NULL;
    size_t old_n = 0;
    f = fopen(argv[3], "rb");
    if (f) {
        old = malloc(out.n ? out.n : 1);
        if (old)
            old_n = fread(old, 1, out.n, f);
        fclose(f);
    }
    if (old && old_n == out.n && memcmp(old, out.d, out.n) == 0) {
        free(old);
        return 0;
    }
    free(old);
    f = fopen(argv[3], "wb");
    if (!f) {
        fprintf(stderr, "langgen: cannot write %s\n", argv[3]);
        return 2;
    }
    fwrite(out.d, 1, out.n, f);
    fclose(f);
    return 0;
}
