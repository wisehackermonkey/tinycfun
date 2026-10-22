/* chime.c — Chime-SMS-2M reference inference + tokenizer, C11 port.
 *
 * Port of:
 *   localtype_tokenizer.py  (word-level tokenizer)
 *   chime_numpy_reference.py (NumPy float64 reference inference, int8 file)
 *
 * Build: gcc -O2 -std=c11 -o chime chime.c -lm
 * Run:   ./chime "text before cursor" [-k N] [--model path/to/cifg_int8.bin]
 *
 * License: Apache-2.0. Original Python: Copyright 2026 Luke Steuber.
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

/* ------------------------------------------------------------------ */
/* small utils                                                         */
/* ------------------------------------------------------------------ */

static void die(const char *msg) {
    fprintf(stderr, "chime: %s\n", msg);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

static void *xcalloc(size_t n, size_t s) {
    void *p = calloc(n ? n : 1, s ? s : 1);
    if (!p) die("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) die("out of memory");
    return q;
}

static char *xstrdup(const char *s) {
    size_t n = strlen(s);
    char *p = xmalloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

/* growable byte buffer */
typedef struct { char *data; size_t len, cap; } Buf;

static void buf_reserve(Buf *b, size_t extra) {
    if (b->len + extra + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 64;
        while (nc < b->len + extra + 1) nc *= 2;
        b->data = xrealloc(b->data, nc);
        b->cap = nc;
    }
}

static void buf_putc(Buf *b, char c) {
    buf_reserve(b, 1);
    b->data[b->len++] = c;
    b->data[b->len] = 0;
}

static void buf_puts(Buf *b, const char *s) {
    size_t n = strlen(s);
    buf_reserve(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = 0;
}

static void buf_putcps(Buf *b, uint32_t cp) {
    if (cp < 0x80) {
        buf_putc(b, (char)cp);
    } else if (cp < 0x800) {
        buf_putc(b, (char)(0xC0 | (cp >> 6)));
        buf_putc(b, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        buf_putc(b, (char)(0xE0 | (cp >> 12)));
        buf_putc(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        buf_putc(b, (char)(0x80 | (cp & 0x3F)));
    } else {
        buf_putc(b, (char)(0xF0 | (cp >> 18)));
        buf_putc(b, (char)(0x80 | ((cp >> 12) & 0x3F)));
        buf_putc(b, (char)(0x80 | ((cp >> 6) & 0x3F)));
        buf_putc(b, (char)(0x80 | (cp & 0x3F)));
    }
}

static void buf_free(Buf *b) {
    free(b->data);
    b->data = NULL;
    b->len = b->cap = 0;
}

/* UTF-8 decode one codepoint. Returns bytes consumed, sets *cp. 0 on error. */
static int utf8_decode(const unsigned char *s, size_t n, uint32_t *cp) {
    if (n == 0) return 0;
    unsigned char c = s[0];
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) {
        if (n < 2 || (s[1] & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c & 0x1F) << 6) | (s[1] & 0x3F);
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        if (n < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        if (n < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
              ((uint32_t)(s[2] & 0x3F) << 6) | (s[3] & 0x3F);
        return 4;
    }
    return 0;
}

/* Decode UTF-8 string to codepoint array. Returns count. */
static size_t utf8_to_cps(const char *s, uint32_t **out) {
    size_t n = strlen(s);
    size_t cap = n + 1;
    uint32_t *cps = xmalloc(cap * sizeof(uint32_t));
    size_t i = 0, k = 0;
    while (i < n) {
        uint32_t cp;
        int used = utf8_decode((const unsigned char *)s + i, n - i, &cp);
        if (!used) { cps[k++] = 0xFFFD; i++; continue; }
        cps[k++] = cp;
        i += used;
    }
    *out = cps;
    return k;
}

/* UTF-16 code-unit length of a UTF-8 string. */
static size_t utf16_len(const char *s) {
    size_t n = strlen(s), i = 0, units = 0;
    while (i < n) {
        uint32_t cp;
        int used = utf8_decode((const unsigned char *)s + i, n - i, &cp);
        if (!used) { i++; units++; continue; }
        units += (cp >= 0x10000) ? 2 : 1;
        i += used;
    }
    return units;
}

/* ------------------------------------------------------------------ */
/* CRC-32 (zlib polynomial)                                           */
/* ------------------------------------------------------------------ */

static uint32_t crc32_buf(const uint8_t *data, size_t n) {
    static uint32_t table[256];
    static int inited = 0;
    if (!inited) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        inited = 1;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
        c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/* Unicode general-category helpers (compact)                         */
/* ------------------------------------------------------------------ */

/* True for codepoints in general category L (letters) and Nl/No excluded
 * from \p{L}. We accept L categories (Lu, Ll, Lt, Lm, Lo). The Python code
 * uses str.isalpha(); this approximates that for the scripts the vocab uses.
 * Extend ranges here if your vocab has other scripts. */
static int is_letter_cp(uint32_t cp) {
    if (cp < 0x80)
        return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
    /* Latin-1 supplement letters */
    if (cp >= 0x00C0 && cp <= 0x00D6) return 1;
    if (cp >= 0x00D8 && cp <= 0x00F6) return 1;
    if (cp >= 0x00F8 && cp <= 0x02AF) return 1; /* Latin ext, IPA */
    if (cp >= 0x0370 && cp <= 0x03FF) return 1; /* Greek */
    if (cp >= 0x0400 && cp <= 0x052F) return 1; /* Cyrillic */
    if (cp >= 0x0531 && cp <= 0x0587) return 1; /* Armenian */
    if (cp >= 0x05D0 && cp <= 0x05EA) return 1; /* Hebrew */
    if (cp >= 0x0620 && cp <= 0x064A) return 1; /* Arabic */
    if (cp >= 0x0900 && cp <= 0x097F) return 1; /* Devanagari */
    if (cp >= 0x0980 && cp <= 0x09FF) return 1; /* Bengali */
    if (cp >= 0x0A00 && cp <= 0x0A7F) return 1; /* Gurmukhi */
    if (cp >= 0x0A80 && cp <= 0x0AFF) return 1; /* Gujarati */
    if (cp >= 0x0B80 && cp <= 0x0BFF) return 1; /* Tamil */
    if (cp >= 0x0C00 && cp <= 0x0C7F) return 1; /* Telugu */
    if (cp >= 0x0E00 && cp <= 0x0E7F) return 1; /* Thai */
    if (cp >= 0x3040 && cp <= 0x309F) return 1; /* Hiragana */
    if (cp >= 0x30A0 && cp <= 0x30FF) return 1; /* Katakana */
    if (cp >= 0x4E00 && cp <= 0x9FFF) return 1; /* CJK Unified */
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 1; /* Hangul syllables */
    if (cp >= 0xFF21 && cp <= 0xFF3A) return 1; /* Fullwidth A-Z */
    if (cp >= 0xFF41 && cp <= 0xFF5A) return 1; /* Fullwidth a-z */
    if (cp >= 0x1F100 && cp <= 0x1F1FF) return 0; /* enclosed alnum, flags */
    if (cp >= 0x20000 && cp <= 0x2A6DF) return 1; /* CJK ext B */
    return 0;
}

/* Apostrophe-like / hyphen-like characters allowed inside a word. */
static int is_word_inner_cp(uint32_t cp) {
    return cp == '\'' || cp == 0x2019 || /* ’ */
           cp == '-' || cp == 0x2010 || cp == 0x2011 || cp == 0x2013 ||
           cp == 0x2014 || cp == 0x2212;
}

/* Normalize: fold ’ to ', lowercase (ASCII + Latin-1 + simple BMP fold). */
static void norm_cp_append(Buf *out, uint32_t cp) {
    if (cp == 0x2019) cp = '\'';
    if (cp >= 'A' && cp <= 'Z') cp += 32;
    else if (cp >= 0x00C0 && cp <= 0x00DE && cp != 0x00D7) cp += 32;
    buf_putcps(out, cp);
}

static char *normalize_word(const char *s) {
    uint32_t *cps;
    size_t n = utf8_to_cps(s, &cps);
    Buf b = {0};
    for (size_t i = 0; i < n; i++) norm_cp_append(&b, cps[i]);
    free(cps);
    if (!b.data) return xstrdup("");
    return b.data;
}

/* ------------------------------------------------------------------ */
/* Punctuation + canonical folding                                    */
/* ------------------------------------------------------------------ */

/* Punctuation tokens (single codepoint each). We match the Python PUNCT tuple. */
static const uint32_t PUNCT_CPS[] = {
    '.', ',', '?', '!', 0x2026, '\'', '"', '-', ':', ';', '(', ')', '/', '&',
    '$', '%', '*', '+', '=', '@', '#', '~', '_', '[', ']', '<', '>', '^',
    '\\', '|', 0x00B0,
};
static const size_t PUNCT_N = sizeof(PUNCT_CPS) / sizeof(PUNCT_CPS[0]);

static int is_punct_cp(uint32_t cp) {
    for (size_t i = 0; i < PUNCT_N; i++)
        if (PUNCT_CPS[i] == cp) return 1;
    return 0;
}

/* Folding map: look-alike -> canonical. Returns canonical cp or the input. */
static uint32_t canon_cp(uint32_t cp) {
    switch (cp) {
        case 0x2019: case 0x2018: case 0x201A: case '`': case 0x00B4: return '\'';
        case 0x201C: case 0x201D: case 0x201E: case 0x00AB: case 0x00BB: return '"';
        case 0x2013: case 0x2014: case 0x2010: case 0x2011: case 0x2212: case 0xFE63: case 0xFF0D: return '-';
        case 0x3002: case 0xFF0E: return '.';
        case 0xFF0C: case 0x3001: return ',';
        case 0xFF1A: return ':';
        case 0xFF1B: return ';';
        case 0xFF1F: return '?';
        case 0xFF01: return '!';
        case 0xFF08: return '(';
        case 0xFF09: return ')';
        case 0x00BF: return '?';
        case 0x00A1: return '!';
        case 0x2025: return 0x2026;
        default: return cp;
    }
}

/* ------------------------------------------------------------------ */
/* Emoji detection (same ranges as the Python EMOJI_RE, simplified)   */
/* ------------------------------------------------------------------ */

static int is_emoji_base_cp(uint32_t cp) {
    /* flags handled separately (two regional indicators) */
    if (cp == 0x3030 || cp == 0x303D || cp == 0x3297 || cp == 0x3299) return 1;
    if (cp == 0x00A9 || cp == 0x00AE || cp == 0x2122) return 1;
    if (cp >= 0x2194 && cp <= 0x2199) return 1;
    if (cp == 0x21A9 || cp == 0x21AA) return 1;
    if (cp == 0x24C2) return 1;
    if (cp >= 0x25AA && cp <= 0x25FE) return 1;
    if (cp == 0x2934 || cp == 0x2935) return 1;
    if (cp >= 0x1F000 && cp <= 0x1FAFF) return 1;
    if (cp >= 0x2600 && cp <= 0x27BF) return 1;
    if (cp >= 0x2300 && cp <= 0x23FF) return 1;
    if (cp >= 0x2B00 && cp <= 0x2BFF) return 1;
    return 0;
}

static int is_regional_indicator(uint32_t cp) {
    return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

static int is_emoji_mod_cp(uint32_t cp) {
    return cp == 0xFE0E || cp == 0xFE0F ||
           (cp >= 0x1F3FB && cp <= 0x1F3FF) ||
           (cp >= 0xE0020 && cp <= 0xE007F);
}

static int is_keycap_base(uint32_t cp) {
    return (cp >= '0' && cp <= '9') || cp == '#' || cp == '*';
}

/* Strip variation selectors (☀️ -> ☀) for matching. */
static char *strip_presentation(const char *s) {
    uint32_t *cps;
    size_t n = utf8_to_cps(s, &cps);
    Buf b = {0};
    for (size_t i = 0; i < n; i++) {
        if (cps[i] == 0xFE0F || cps[i] == 0xFE0E) continue;
        buf_putcps(&b, cps[i]);
    }
    free(cps);
    if (!b.data) return xstrdup("");
    return b.data;
}

/* Full-match emoji check on a codepoint array. */
static int is_emoji_cps(const uint32_t *cps, size_t n) {
    if (n == 0) return 0;
    size_t i = 0;
    while (i < n) {
        int matched = 0;
        /* flag: two regional indicators */
        if (i + 1 < n && is_regional_indicator(cps[i]) && is_regional_indicator(cps[i + 1])) {
            i += 2;
            matched = 1;
        } else if (is_keycap_base(cps[i]) && i + 1 < n) {
            size_t j = i + 1;
            if (j < n && cps[j] == 0xFE0F) j++;
            if (j < n && cps[j] == 0x20E3) { i = j + 1; matched = 1; }
        } else if (is_emoji_base_cp(cps[i])) {
            i++;
            while (i < n && is_emoji_mod_cp(cps[i])) i++;
            matched = 1;
        }
        if (!matched) return 0;
        /* ZWJ sequences */
        while (i < n && cps[i] == 0x200D) {
            i++;
            if (i >= n) return 0;
            if (is_regional_indicator(cps[i]) && i + 1 < n && is_regional_indicator(cps[i + 1])) {
                i += 2;
            } else if (is_keycap_base(cps[i]) && i + 1 < n) {
                size_t j = i + 1;
                if (j < n && cps[j] == 0xFE0F) j++;
                if (j < n && cps[j] == 0x20E3) i = j + 1;
                else return 0;
            } else if (is_emoji_base_cp(cps[i])) {
                i++;
                while (i < n && is_emoji_mod_cp(cps[i])) i++;
            } else {
                return 0;
            }
        }
    }
    return 1;
}

static int is_emoji_str(const char *s) {
    uint32_t *cps;
    size_t n = utf8_to_cps(s, &cps);
    int r = is_emoji_cps(cps, n);
    free(cps);
    return r;
}

/* ------------------------------------------------------------------ */
/* Token kind checks                                                  */
/* ------------------------------------------------------------------ */

static const char *SPECIALS[6] = { "<pad>", "<s>", "</s>", "<unk>", "<num>", "<sym>" };

static int str_in(const char *s, const char *const *arr, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, arr[i]) == 0) return 1;
    return 0;
}

static int is_word_token(const char *tok) {
    if (!tok || !*tok) return 0;
    uint32_t *cps;
    size_t n = utf8_to_cps(tok, &cps);
    if (n == 0) { free(cps); return 0; }
    int ok = is_letter_cp(cps[0]) && !is_emoji_cps(cps, n) &&
             !str_in(tok, SPECIALS, 6);
    if (ok) {
        for (size_t i = 0; i < n; i++) {
            if (!is_letter_cp(cps[i]) && !is_word_inner_cp(cps[i])) { ok = 0; break; }
        }
    }
    /* punctuation tokens are single punct cps; exclude */
    if (ok && n == 1 && is_punct_cp(cps[0])) ok = 0;
    free(cps);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Tokenizer                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    char **tokens;
    size_t n;
    int *word_ids;   size_t n_word;
    int *emoji_ids;  size_t n_emoji;
    int *punct_ids;  size_t n_punct;
    /* hash map token -> id */
    int *map;        /* open addressing: -1 empty, else id */
    size_t map_cap;
    /* emoji key (stripped presentation) -> token index */
    char **emoji_key; int *emoji_val; size_t n_emoji_key;
} Tokenizer;

static uint64_t fnv1a(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ULL; }
    return h;
}

static void tok_map_insert(Tokenizer *tk, const char *tok, int id) {
    uint64_t h = fnv1a(tok);
    size_t mask = tk->map_cap - 1;
    size_t i = h & mask;
    while (tk->map[i] != -1) {
        if (strcmp(tk->tokens[tk->map[i]], tok) == 0) return;
        i = (i + 1) & mask;
    }
    tk->map[i] = id;
}

static void tok_map_build(Tokenizer *tk) {
    size_t cap = 16;
    while (cap < tk->n * 2) cap *= 2;
    tk->map_cap = cap;
    tk->map = xmalloc(cap * sizeof(int));
    for (size_t i = 0; i < cap; i++) tk->map[i] = -1;
    for (size_t i = 0; i < tk->n; i++) tok_map_insert(tk, tk->tokens[i], (int)i);
}

static int tok_lookup(const Tokenizer *tk, const char *tok) {
    uint64_t h = fnv1a(tok);
    size_t mask = tk->map_cap - 1;
    size_t i = h & mask;
    while (tk->map[i] != -1) {
        if (strcmp(tk->tokens[tk->map[i]], tok) == 0) return tk->map[i];
        i = (i + 1) & mask;
    }
    return -1;
}

static void tokenizer_init(Tokenizer *tk, char **tokens, size_t n) {
    memset(tk, 0, sizeof(*tk));
    tk->tokens = tokens;
    tk->n = n;
    tk->word_ids = xmalloc(n * sizeof(int));
    tk->emoji_ids = xmalloc(n * sizeof(int));
    tk->punct_ids = xmalloc(n * sizeof(int));
    for (size_t i = 0; i < n; i++) {
        const char *t = tokens[i];
        if (is_word_token(t)) tk->word_ids[tk->n_word++] = (int)i;
        if (is_emoji_str(t))  tk->emoji_ids[tk->n_emoji++] = (int)i;
        if (strlen(t) == 1 && is_punct_cp((unsigned char)t[0])) {
            /* handle BMP punct that may be multi-byte too */
        }
        uint32_t *cps; size_t nc = utf8_to_cps(t, &cps);
        if (nc == 1 && is_punct_cp(cps[0])) tk->punct_ids[tk->n_punct++] = (int)i;
        free(cps);
    }
    tok_map_build(tk);
}

static void tokenizer_free(Tokenizer *tk) {
    for (size_t i = 0; i < tk->n; i++) free(tk->tokens[i]);
    free(tk->tokens);
    free(tk->word_ids);
    free(tk->emoji_ids);
    free(tk->punct_ids);
    free(tk->map);
    for (size_t i = 0; i < tk->n_emoji_key; i++) free(tk->emoji_key[i]);
    free(tk->emoji_key);
    free(tk->emoji_val);
}

/* Canonicalize an emoji token by stripping variation selectors. */
static const char *tok_canonical(const Tokenizer *tk, const char *token) {
    if (tok_lookup(tk, token) >= 0) return token;
    if (!is_emoji_str(token)) return token;
    char *key = strip_presentation(token);
    for (size_t i = 0; i < tk->n_emoji_key; i++) {
        if (strcmp(tk->emoji_key[i], key) == 0) {
            free(key);
            return tk->tokens[tk->emoji_val[i]];
        }
    }
    free(key);
    return token;
}

static int tok_id_of(const Tokenizer *tk, const char *token) {
    const char *canon = tok_canonical(tk, token);
    int id = tok_lookup(tk, canon);
    if (id < 0) return 3; /* <unk> */
    return id;
}

/* ------------------------------------------------------------------ */
/* Word-level splitting (mirrors WORD_RE / _MASTER in Python)          */
/* ------------------------------------------------------------------ */

/* A word = letter followed by letters, apostrophes, or hyphens. */
static int word_char_at(const uint32_t *cps, size_t n, size_t i, int *is_letter) {
    if (i >= n) return 0;
    if (is_letter_cp(cps[i])) { if (is_letter) *is_letter = 1; return 1; }
    if (is_word_inner_cp(cps[i])) { if (is_letter) *is_letter = 0; return 1; }
    return 0;
}

/* Find the raw active prefix at the end of the string (raw word being typed). */
static char *active_prefix(const char *s) {
    uint32_t *cps; size_t n = utf8_to_cps(s, &cps);
    size_t i = n;
    while (i > 0) {
        if (is_letter_cp(cps[i - 1]) || is_word_inner_cp(cps[i - 1])) i--;
        else break;
    }
    /* The prefix must start with a letter. */
    size_t start = i;
    while (start < n && !is_letter_cp(cps[start])) start++;
    Buf b = {0};
    for (size_t k = start; k < n; k++) buf_putcps(&b, cps[k]);
    free(cps);
    if (!b.data) return xstrdup("");
    return b.data;
}

/* Tokenize the given text into tokens (words, punct, emoji, <num>, <sym>).
 * Appends to `out`; each string must be freed by caller or stored. */
static void tokenize_into(const char *text, char ***out, size_t *n_out, size_t *cap) {
    uint32_t *cps; size_t n = utf8_to_cps(text, &cps);
    size_t i = 0;
    while (i < n) {
        uint32_t c = cps[i];
        /* word */
        if (is_letter_cp(c)) {
            size_t j = i + 1;
            while (j < n && (is_letter_cp(cps[j]) || is_word_inner_cp(cps[j]))) j++;
            Buf w = {0};
            for (size_t k = i; k < j; k++) buf_putcps(&w, cps[k]);
            char *norm = normalize_word(w.data);
            buf_free(&w);
            if (*n_out == *cap) { *cap = *cap ? *cap * 2 : 32; *out = xrealloc(*out, *cap * sizeof(char *)); }
            (*out)[(*n_out)++] = norm;
            i = j;
            continue;
        }
        /* emoji */
        {
            /* try to match emoji at i */
            size_t j = i;
            int ok = 0;
            if (is_regional_indicator(cps[i]) && i + 1 < n && is_regional_indicator(cps[i + 1])) {
                j = i + 2; ok = 1;
            } else if (is_keycap_base(cps[i])) {
                size_t k = i + 1;
                if (k < n && cps[k] == 0xFE0F) k++;
                if (k < n && cps[k] == 0x20E3) { j = k + 1; ok = 1; }
            } else if (is_emoji_base_cp(cps[i])) {
                j = i + 1;
                while (j < n && is_emoji_mod_cp(cps[j])) j++;
                ok = 1;
            }
            while (ok && j < n && cps[j] == 0x200D) {
                size_t k = j + 1;
                if (k < n && is_regional_indicator(cps[k]) && k + 1 < n && is_regional_indicator(cps[k + 1])) {
                    j = k + 2;
                } else if (k < n && is_keycap_base(cps[k])) {
                    size_t m = k + 1;
                    if (m < n && cps[m] == 0xFE0F) m++;
                    if (m < n && cps[m] == 0x20E3) j = m + 1;
                    else { ok = 0; break; }
                } else if (k < n && is_emoji_base_cp(cps[k])) {
                    j = k + 1;
                    while (j < n && is_emoji_mod_cp(cps[j])) j++;
                } else { ok = 0; break; }
            }
            if (ok) {
                Buf e = {0};
                for (size_t k = i; k < j; k++) buf_putcps(&e, cps[k]);
                if (*n_out == *cap) { *cap = *cap ? *cap * 2 : 32; *out = xrealloc(*out, *cap * sizeof(char *)); }
                (*out)[(*n_out)++] = e.data;
                i = j;
                continue;
            }
        }
        /* number: digits with . , : / inside */
        if (c >= '0' && c <= '9') {
            size_t j = i + 1;
            for (;;) {
                if (j < n && cps[j] >= '0' && cps[j] <= '9') { j++; continue; }
                if (j + 1 < n && (cps[j] == '.' || cps[j] == ',' || cps[j] == ':' || cps[j] == '/') &&
                    cps[j + 1] >= '0' && cps[j + 1] <= '9') { j += 2; continue; }
                break;
            }
            if (*n_out == *cap) { *cap = *cap ? *cap * 2 : 32; *out = xrealloc(*out, *cap * sizeof(char *)); }
            (*out)[(*n_out)++] = xstrdup("<num>");
            i = j;
            continue;
        }
        /* whitespace */
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f' ||
            (c >= 0x00A0 && c <= 0x2000) || c == 0x200B || c == 0x3000) {
            i++;
            continue;
        }
        /* other: punct or <sym>; fold canon; handle runs */
        uint32_t cc = canon_cp(c);
        i++;
        if (cc == '.') {
            if (*n_out > 0 && strcmp((*out)[*n_out - 1], ".") == 0) {
                free((*out)[*n_out - 1]);
                (*out)[*n_out - 1] = xstrdup("…");
                continue;
            }
            if (*n_out > 0 && strcmp((*out)[*n_out - 1], "…") == 0) continue;
        } else if (is_punct_cp(cc) && *n_out > 0) {
            /* collapse runs of same punct */
            char tmp[8]; int tl = 0;
            if (cc < 0x80) tmp[tl++] = (char)cc;
            else { Buf t = {0}; buf_putcps(&t, cc); memcpy(tmp, t.data, t.len); tl = (int)t.len; tmp[tl] = 0; buf_free(&t); }
            if (strcmp((*out)[*n_out - 1], tmp) == 0) continue;
        }
        if (*n_out == *cap) { *cap = *cap ? *cap * 2 : 32; *out = xrealloc(*out, *cap * sizeof(char *)); }
        if (is_punct_cp(cc)) {
            if (cc < 0x80) { char s[2] = {(char)cc, 0}; (*out)[(*n_out)++] = xstrdup(s); }
            else { Buf t = {0}; buf_putcps(&t, cc); (*out)[(*n_out)++] = t.data; }
        } else {
            (*out)[(*n_out)++] = xstrdup("<sym>");
        }
    }
    free(cps);
}

/* Public tokenize. Returns NULL-terminated token array plus count. */
static char **tokenize(const char *text, size_t *n_out) {
    char **out = NULL; size_t n = 0, cap = 0;
    tokenize_into(text, &out, &n, &cap);
    *n_out = n;
    return out;
}

/* ------------------------------------------------------------------ */
/* Context extraction (context_tokens in Python)                      */
/* ------------------------------------------------------------------ */

#define CONTEXT_TOKENS 64
#define CONTEXT_HOP 32

static int is_sentence_end(const char *t) {
    return strcmp(t, ".") == 0 || strcmp(t, "!") == 0 ||
           strcmp(t, "?") == 0 || strcmp(t, "…") == 0;
}

static int window_start(int length, int limit, int hop) {
    if (length <= limit) return 0;
    return ((length - limit + hop - 1) / hop) * hop;
}

/* Returns token array of context (excluding active prefix) and sets *prefix (malloc). */
static char **context_tokens(const char *text, size_t *n_out, char **prefix_out) {
    char *prefix = active_prefix(text);
    size_t plen = strlen(prefix);
    /* text before prefix */
    size_t tlen = strlen(text);
    char *before = xmalloc(tlen - plen + 1);
    memcpy(before, text, tlen - plen);
    before[tlen - plen] = 0;

    /* take substring after last newline */
    char *line = before;
    for (char *p = before; *p; p++) if (*p == '\n') line = p + 1;

    size_t tn = 0;
    char **toks = tokenize(line, &tn);

    /* drop up to last sentence-end token */
    size_t start = 0;
    for (size_t i = tn; i-- > 0;) {
        if (is_sentence_end(toks[i])) { start = i + 1; break; }
    }
    if (start > 0) {
        for (size_t i = 0; i < start; i++) free(toks[i]);
        memmove(toks, toks + start, (tn - start) * sizeof(char *));
        tn -= start;
    }

    int ws = window_start((int)tn, CONTEXT_TOKENS - 1, CONTEXT_HOP);
    if (ws > 0 && (size_t)ws <= tn) {
        for (int i = 0; i < ws; i++) free(toks[i]);
        memmove(toks, toks + ws, (tn - ws) * sizeof(char *));
        tn -= ws;
    }

    free(before);
    *prefix_out = prefix;
    *n_out = tn;
    return toks;
}

/* Build ids: [bos] + ids of context tokens. */
static int *context_ids(const Tokenizer *tk, const char *text, size_t *n_ids, char **prefix_out) {
    size_t tn;
    char **toks = context_tokens(text, &tn, prefix_out);
    int *ids = xmalloc((tn + 1) * sizeof(int));
    ids[0] = 1; /* bos */
    for (size_t i = 0; i < tn; i++) {
        ids[i + 1] = tok_id_of(tk, toks[i]);
        free(toks[i]);
    }
    free(toks);
    *n_ids = tn + 1;
    return ids;
}

/* ------------------------------------------------------------------ */
/* Model: read int8 bin, dequantize to float64, run reference forward */
/* ------------------------------------------------------------------ */

typedef struct {
    int V, E, H, n_vocab;
    /* dequantized weights (float64) */
    double *embedding; /* V x E */
    double *wx;        /* E x 3H */
    double *wr;        /* E x 3H */
    double *proj;      /* H x E */
    double *b;         /* 3H */
    char **tokens;     /* V tokens */
    int *offer_ids;
    size_t n_offer;
    Tokenizer tk;
} Model;

static int read_u32le(const uint8_t *p) {
    return (int)(p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t)(p[3]) << 24);
}

static float read_f32le(const uint8_t *p) {
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static uint8_t *read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "chime: cannot open %s: %s\n", path, strerror(errno)); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = xmalloc((size_t)sz);
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); die("short read"); }
    fclose(f);
    *n = (size_t)sz;
    return buf;
}

static void dequantize(const uint8_t *bin, size_t n, int V, int E, int H, size_t *pos, double *out, size_t rows, size_t cols) {
    const float *scales = (const float *)(bin + *pos);
    *pos += 4 * rows;
    const int8_t *q = (const int8_t *)(bin + *pos);
    size_t bytes = rows * cols;
    for (size_t r = 0; r < rows; r++) {
        double s = (double)read_f32le((const uint8_t *)&scales[r]);
        for (size_t c = 0; c < cols; c++)
            out[r * cols + c] = (double)q[r * cols + c] * s;
    }
    *pos += bytes;
    size_t pad = (4 - (bytes % 4)) % 4;
    *pos += pad;
}

static void model_load(Model *m, const char *path) {
    size_t n;
    uint8_t *bin = read_file(path, &n);
    if (n < 24 || memcmp(bin, "LTCF", 4) != 0) die("not a LTCF file");
    int version = read_u32le(bin + 4);
    if (version != 1) die("unsupported LTCF version");
    int V = read_u32le(bin + 8);
    int E = read_u32le(bin + 12);
    int H = read_u32le(bin + 16);
    int n_vocab = read_u32le(bin + 20);
    /* CRC */
    uint32_t crc_stored = (uint32_t)read_u32le(bin + n - 4);
    uint32_t crc_calc = crc32_buf(bin, n - 4);
    if (crc_stored != crc_calc) die("CRC mismatch");

    m->V = V; m->E = E; m->H = H; m->n_vocab = n_vocab;
    m->embedding = xmalloc((size_t)V * E * sizeof(double));
    m->wx = xmalloc((size_t)E * 3 * H * sizeof(double));
    m->wr = xmalloc((size_t)E * 3 * H * sizeof(double));
    m->proj = xmalloc((size_t)H * E * sizeof(double));
    m->b = xmalloc((size_t)3 * H * sizeof(double));

    size_t pos = 24;
    dequantize(bin, n, V, E, H, &pos, m->embedding, V, E);
    dequantize(bin, n, V, E, H, &pos, m->wx, E, 3 * H);
    dequantize(bin, n, V, E, H, &pos, m->wr, E, 3 * H);
    dequantize(bin, n, V, E, H, &pos, m->proj, H, E);
    for (int i = 0; i < 3 * H; i++)
        m->b[i] = (double)read_f32le(bin + pos + 4 * i);
    pos += 12 * (size_t)H;
    /* vocabulary: n_vocab bytes of UTF-8, \n-separated */
    const char *vocab = (const char *)(bin + pos);
    if (pos + n_vocab + 4 != n) die("vocabulary section does not match header");
    /* split on \n into V tokens */
    m->tokens = xmalloc(V * sizeof(char *));
    int count = 0;
    size_t i = 0;
    while (count < V) {
        size_t j = i;
        while (j < (size_t)n_vocab && vocab[j] != '\n') j++;
        if (j >= (size_t)n_vocab && count < V - 1) die("vocabulary short");
        size_t len = j - i;
        char *t = xmalloc(len + 1);
        memcpy(t, vocab + i, len);
        t[len] = 0;
        m->tokens[count++] = t;
        i = j + 1;
    }
    free(bin);

    tokenizer_init(&m->tk, m->tokens, (size_t)V);

    /* offer_ids: offerable tokens */
    m->offer_ids = xmalloc(V * sizeof(int));
    for (int i2 = 0; i2 < V; i2++) {
        const char *t = m->tokens[i2];
        if (!t[0]) continue;
        uint32_t *cps; size_t nc = utf8_to_cps(t, &cps);
        if (nc == 0) { free(cps); continue; }
        if (!is_letter_cp(cps[0])) { free(cps); continue; }
        int ok = 1;
        for (size_t k = 0; k < nc; k++) {
            if (!is_letter_cp(cps[k]) && !is_word_inner_cp(cps[k])) { ok = 0; break; }
        }
        free(cps);
        if (!ok) continue;
        size_t ulen = utf16_len(t);
        if (ulen >= 2 || strcmp(t, "a") == 0 || strcmp(t, "i") == 0)
            m->offer_ids[m->n_offer++] = i2;
    }
}

static void model_free(Model *m) {
    free(m->embedding); free(m->wx); free(m->wr); free(m->proj); free(m->b);
    tokenizer_free(&m->tk);
    free(m->tokens);
    free(m->offer_ids);
}

static double sigmoid(double x) { return 1.0 / (1.0 + exp(-x)); }

/* reference_logits: run the model over ids and return logits[V]. */
static double *reference_logits(const Model *m, const int *ids, size_t n_ids) {
    int E = m->E, H = m->H, V = m->V;
    double *c = xcalloc((size_t)H, sizeof(double));
    double *r = xcalloc((size_t)E, sizeof(double));
    double *g = xmalloc((size_t)3 * H * sizeof(double));
    double *tmp3 = xmalloc((size_t)3 * H * sizeof(double));
    double *tmpE = xmalloc((size_t)E * sizeof(double));
    double *logits = xcalloc((size_t)V, sizeof(double));

    for (size_t ti = 0; ti < n_ids; ti++) {
        int id = ids[ti];
        if (id < 0 || id >= V) id = 3;
        /* g = embedding[id] @ wx + r @ wr + b */
        const double *emb = m->embedding + (size_t)id * E;
        /* tmp3 = emb @ wx */
        for (int k = 0; k < 3 * H; k++) tmp3[k] = 0.0;
        for (int e = 0; e < E; e++) {
            double ev = emb[e];
            if (ev == 0.0) continue;
            const double *row = m->wx + (size_t)e * (3 * H);
            for (int k = 0; k < 3 * H; k++) tmp3[k] += ev * row[k];
        }
        /* tmp3 += r @ wr */
        for (int e = 0; e < E; e++) {
            double rv = r[e];
            if (rv == 0.0) continue;
            const double *row = m->wr + (size_t)e * (3 * H);
            for (int k = 0; k < 3 * H; k++) tmp3[k] += rv * row[k];
        }
        for (int k = 0; k < 3 * H; k++) g[k] = tmp3[k] + m->b[k];

        /* update c, r */
        for (int k = 0; k < H; k++) {
            double f = sigmoid(g[k]);
            double o = sigmoid(g[H + k]);
            double u = tanh(g[2 * H + k]);
            c[k] = f * c[k] + (1.0 - f) * u;
            /* r contribution: (o * tanh(c)) @ proj, accumulate into tmpE */
        }
        /* tmpE = (o * tanh(c)) @ proj */
        for (int e = 0; e < E; e++) tmpE[e] = 0.0;
        for (int k = 0; k < H; k++) {
            double o = sigmoid(g[H + k]);
            double oc = o * tanh(c[k]);
            const double *row = m->proj + (size_t)k * E;
            for (int e = 0; e < E; e++) tmpE[e] += oc * row[e];
        }
        memcpy(r, tmpE, (size_t)E * sizeof(double));
    }

    /* logits = embedding @ r */
    for (int v = 0; v < V; v++) {
        double s = 0.0;
        const double *emb = m->embedding + (size_t)v * E;
        for (int e = 0; e < E; e++) s += emb[e] * r[e];
        logits[v] = s;
    }

    free(c); free(r); free(g); free(tmp3); free(tmpE);
    return logits;
}

/* top_words: k best offerable tokens starting with prefix, sorted by (-logit, id). */
typedef struct { int id; double logit; } Cand;
static int cand_cmp(const void *a, const void *b) {
    const Cand *x = a, *y = b;
    if (x->logit > y->logit) return -1;
    if (x->logit < y->logit) return 1;
    return (x->id < y->id) ? -1 : (x->id > y->id);
}

static char **top_words(const Model *m, const double *logits, const char *prefix, int k, size_t *n_out) {
    Cand *c = xmalloc(m->n_offer * sizeof(Cand));
    size_t nc = 0;
    size_t plen = strlen(prefix);
    for (size_t i = 0; i < m->n_offer; i++) {
        int id = m->offer_ids[i];
        const char *t = m->tokens[id];
        if (strncmp(t, prefix, plen) != 0) continue;
        c[nc].id = id;
        c[nc].logit = logits[id];
        nc++;
    }
    qsort(c, nc, sizeof(Cand), cand_cmp);
    if ((size_t)k > nc) k = (int)nc;
    char **out = xmalloc((size_t)k * sizeof(char *));
    for (int i = 0; i < k; i++) out[i] = xstrdup(m->tokens[c[i].id]);
    *n_out = (size_t)k;
    free(c);
    return out;
}

/* ------------------------------------------------------------------ */
/* CLI                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    const char *text = NULL;
    const char *model_path = "cifg_int8.bin";
    int k = 6;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) model_path = argv[++i];
        else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) k = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s TEXT [-k N] [--model PATH]\n", argv[0]);
            return 0;
        } else if (!text) text = argv[i];
        else { fprintf(stderr, "chime: unexpected argument %s\n", argv[i]); return 1; }
    }
    if (!text) { fprintf(stderr, "Usage: %s TEXT [-k N] [--model PATH]\n", argv[0]); return 1; }
    if (k < 1 || k > 64) { fprintf(stderr, "chime: k must be in 1..64\n"); return 1; }

    Model m;
    memset(&m, 0, sizeof(m));
    model_load(&m, model_path);

    size_t n_ids;
    char *prefix;
    int *ids = context_ids(&m.tk, text, &n_ids, &prefix);
    double *logits = reference_logits(&m, ids, n_ids);
    size_t n_sug;
    char **sug = top_words(&m, logits, prefix, k, &n_sug);

    /* print JSON like the Python CLI */
    printf("{\"text\": \"");
    for (const char *p = text; *p; p++) {
        if (*p == '"' || *p == '\\') putchar('\\');
        putchar(*p);
    }
    printf("\", \"suggestions\": [");
    for (size_t i = 0; i < n_sug; i++) {
        if (i) printf(", ");
        printf("\"");
        for (const char *p = sug[i]; *p; p++) {
            if (*p == '"' || *p == '\\') putchar('\\');
            putchar(*p);
        }
        printf("\"");
    }
    printf("]}\n");

    for (size_t i = 0; i < n_sug; i++) free(sug[i]);
    free(sug);
    free(logits);
    free(ids);
    free(prefix);
    model_free(&m);
    return 0;
}