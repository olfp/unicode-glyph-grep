/* ugrep: grep-compatible Unicode mathematical-glyph search. */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_PROBE_LINES 100
#define DEFAULT_GREP "/usr/bin/grep"

typedef struct {
    int line_number, quiet, with_filename, no_filename, count, invert;
    int force_unicode, force_plain, recursive;
    long max_count, probe_lines;
    const char *pattern;
    const char *config;
} Options;

typedef struct { char **v; size_t n, cap; } StrVec;

static void die(const char *s) {
    fprintf(stderr, "ugrep: %s\n", s);
    exit(2);
}

static void vec_add(StrVec *v, char *s) {
    if (v->n == v->cap) {
        v->cap = v->cap ? v->cap * 2 : 16;
        v->v = realloc(v->v, v->cap * sizeof(*v->v));
        if (!v->v) die("out of memory");
    }
    v->v[v->n++] = s;
}

static unsigned long utf8(const unsigned char *s, size_t n, size_t *used) {
    if (!n) {
        *used = 0;
        return 0;
    }
    if (s[0] < 0x80) {
        *used = 1;
        return s[0];
    }
    if (n >= 2 && (s[0] & 0xe0) == 0xc0) {
        *used = 2;
        return ((s[0] & 31) << 6) | (s[1] & 63);
    }
    if (n >= 3 && (s[0] & 0xf0) == 0xe0) {
        *used = 3;
        return ((s[0] & 15) << 12) | ((s[1] & 63) << 6) | (s[2] & 63);
    }
    if (n >= 4 && (s[0] & 0xf8) == 0xf0) {
        *used = 4;
        return ((s[0] & 7) << 18) |
               ((s[1] & 63) << 12) |
               ((s[2] & 63) << 6) |
               (s[3] & 63);
    }
    *used = 1;
    return s[0];
}

static int is_math_glyph(unsigned long c) {
    return (c >= 0x1d400 && c <= 0x1d7ff) || c == 0x210e || c == 0x2113;
}

static int glyph_ascii(unsigned long c, char *out) {
    static const unsigned long starts[] = {
        0x1d400, 0x1d434, 0x1d468, 0x1d4d0, 0x1d56c,
        0x1d5a0, 0x1d5d4, 0x1d608, 0x1d63c, 0x1d670,
        0x1d6a8
    };

    for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); ++i) {
        unsigned long d = c - starts[i];
        if (d < 26) {
            *out = 'A' + d;
            return 1;
        }
        if (d >= 26 && d < 52) {
            *out = 'a' + (d - 26);
            return 1;
        }
    }

    if (c >= 0x1d7ce && c <= 0x1d7ff) {
        *out = '0' + (char)((c - 0x1d7ce) % 10);
        return 1;
    }

    if (c == 0x210e || c == 0x2113) {
        *out = 'h';
        return 1;
    }

    return 0;
}

static int normalize_text(const char *in, char **out) {
    size_t n = strlen(in), cap = n * 2 + 1, j = 0, used;
    char *p = malloc(cap);
    if (!p) return -1;

    for (size_t i = 0; i < n;) {
        unsigned long c = utf8((const unsigned char *)in + i, n - i, &used);
        char ch = 0;
        if (!used) break;
        if (glyph_ascii(c, &ch)) {
            if (j + 1 >= cap) {
                cap *= 2;
                p = realloc(p, cap);
                if (!p) return -1;
            }
            p[j++] = ch;
        } else {
            if (j + used >= cap) {
                cap *= 2;
                p = realloc(p, cap);
                if (!p) return -1;
            }
            memcpy(p + j, in + i, used);
            j += used;
        }
        i += used;
    }
    p[j] = '\0';
    *out = p;
    return 0;
}

static int has_math_glyph(const char *s) {
    size_t n = strlen(s), used;
    for (size_t i = 0; i < n;) {
        unsigned long c = utf8((const unsigned char *)s + i, n - i, &used);
        if (is_math_glyph(c)) return 1;
        if (!used) break;
        i += used;
    }
    return 0;
}

static const char *config_value(const char *file, const char *key, char *buf, size_t size) {
    FILE *f = fopen(file, "r");
    if (!f) return NULL;

    char line[PATH_MAX + 128];
    int in_ugrep = 0;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;

        if (*p == '[') {
            in_ugrep = strncmp(p, "[ugrep]", 7) == 0;
            continue;
        }

        if (!(in_ugrep || strchr(line, '[') == NULL)) continue;

        char k[64], v[PATH_MAX];
        if (sscanf(p, "%63[^=]=%1023[^\n]", k, v) == 2) {
            char *q = k;
            while (*q && isspace((unsigned char)*q)) q++;
            char *e = q + strlen(q);
            while (e > q && isspace((unsigned char)e[-1])) *--e = '\0';
            if (strcmp(q, key) == 0) {
                char *x = v;
                while (*x && isspace((unsigned char)*x)) x++;
                snprintf(buf, size, "%s", x);
                fclose(f);
                return buf;
            }
        }
    }

    fclose(f);
    return NULL;
}

static const char *grep_path(const char *config) {
    static char value[PATH_MAX];
    if (config) {
        if (config_value(config, "grep", value, sizeof(value))) return value;
    }

    if (config_value(".ugreprc", "grep", value, sizeof(value))) return value;

    const char *home = getenv("HOME");
    if (home) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/.ugreprc", home);
        if (config_value(p, "grep", value, sizeof(value))) return value;
    }

    return DEFAULT_GREP;
}

static long probe_setting(const char *config) {
    char value[64];
    if (config) {
        if (config_value(config, "probe_lines", value, sizeof(value))) return strtol(value, NULL, 10);
    }
    if (config_value(".ugreprc", "probe_lines", value, sizeof(value))) return strtol(value, NULL, 10);

    const char *home = getenv("HOME");
    if (home) {
        char p[PATH_MAX];
        snprintf(p, sizeof(p), "%s/.ugreprc", home);
        if (config_value(p, "probe_lines", value, sizeof(value))) return strtol(value, NULL, 10);
    }

    return DEFAULT_PROBE_LINES;
}

static int delegate_to_grep(int argc, char **argv, const char *config) {
    const char *gp = grep_path(config);
    char **av = calloc((size_t)argc + 2, sizeof(*av));
    if (!av) die("out of memory");

    int n = 0;
    av[n++] = (char *)gp;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "-u") == 0 || strcmp(a, "--unicode") == 0 == 0) continue;
        if (strcmp(a, "-t") == 0 || strcmp(a, "--no-unicode") == 0) continue;
        if (strcmp(a, "--probe-lines") == 0 || strcmp(a, "--config") == 0) { ++i; continue; }
        if (strncmp(a, "--probe-lines=", 14) == 0 || strncmp(a, "--config=", 9) == 0) continue;
        av[n++] = argv[i];
    }
    av[n] = NULL;

    pid_t pid = fork();
    if (pid == -1) {
        free(av);
        die("fork failed");
    }
    if (pid == 0) {
        execv(gp, av);
        execvp(gp, av);
        perror(gp);
        _exit(127);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    free(av);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 2;
}

static int search_file(const char *path, const Options *o, regex_t *re, int multi) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 2;
    }

    char *line = NULL;
    size_t cap = 0;
    long lineno = 0;
    long count = 0;
    int any = 0;

    while (getline(&line, &cap, f) >= 0) {
        char *norm = NULL;
        ++lineno;
        if (normalize_text(line, &norm) != 0) {
            free(line);
            fclose(f);
            die("out of memory");
        }

        int match = regexec(re, norm, 0, NULL, 0) == 0;
        free(norm);
        if (o->invert) match = !match;
        if (!match) continue;

        count++;
        any = 1;
        if (o->max_count && count > o->max_count) break;
        if (o->quiet || o->count) continue;

        if (!o->no_filename && (o->with_filename || multi)) printf("%s:", path);
        if (o->line_number) printf("%ld:", lineno);
        fputs(line, stdout);
    }

    if (o->count) {
        if (!o->no_filename && (o->with_filename || multi)) printf("%s:", path);
        printf("%ld\n", count);
    }

    free(line);
    fclose(f);
    return any ? 0 : 1;
}

static void help(void) {
    puts("Usage: ugrep [OPTIONS] PATTERN [FILE ...]");
    puts("  -n, --line-number       print line numbers");
    puts("  -h, --no-filename       suppress filename prefixes");
    puts("  -H, --with-filename     print filenames even for a single file");
    puts("  -i, --ignore-case       ignore case");
    puts("  -c, --count             count matching lines");
    puts("  -m N, --max-count N     stop after N matches");
    puts("  -u, --unicode           force Unicode normalization mode");
    puts("  -t, --no-unicode       force the real system grep");
    puts("  -r, --recursive         search directories recursively");
    puts("      --probe-lines N     inspect N initial lines for glyph detection");
    puts("      --config FILE       use a config file");
    puts("");
    puts("Configuration file (.ugreprc):");
    puts("  [ugrep]");
    puts("  probe_lines = 100");
    puts("  grep = /usr/bin/grep");
}

int main(int argc, char **argv) {
    Options o = {0};
    StrVec paths = {0};
    o.probe_lines = DEFAULT_PROBE_LINES;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "--help") == 0) {
            help();
            return 0;
        }
        if (strcmp(a, "-n") == 0 || strcmp(a, "--line-number") == 0) {
            o.line_number = 1;
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--no-filename") == 0) {
            o.no_filename = 1;
        } else if (strcmp(a, "-H") == 0 || strcmp(a, "--with-filename") == 0) {
            o.with_filename = 1;
        } else if (strcmp(a, "-i") == 0 || strcmp(a, "--ignore-case") == 0) {
            /* handled by REG_ICASE */
        } else if (strcmp(a, "-u") == 0 || strcmp(a, "--unicode") == 0) {
            o.force_unicode = 1;
        } else if (strcmp(a, "-t") == 0 || strcmp(a, "--no-unicode") == 0) {
            o.force_plain = 1;
        } else if (strcmp(a, "-c") == 0 || strcmp(a, "--count") == 0) {
            o.count = 1;
        } else if (strcmp(a, "-q") == 0 || strcmp(a, "--quiet") == 0) {
            o.quiet = 1;
        } else if (strcmp(a, "-v") == 0 || strcmp(a, "--invert-match") == 0) {
            o.invert = 1;
        } else if (strcmp(a, "-r") == 0 || strcmp(a, "--recursive") == 0) {
            o.recursive = 1;
        } else if (strcmp(a, "-m") == 0 || strcmp(a, "--max-count") == 0) {
            if (i + 1 >= argc) die("missing max count");
            o.max_count = strtol(argv[++i], NULL, 10);
        } else if (strcmp(a, "--probe-lines") == 0) {
            if (i + 1 >= argc) die("missing probe line count");
            o.probe_lines = strtol(argv[++i], NULL, 10);
        } else if (strncmp(a, "--probe-lines=", 14) == 0) {
            o.probe_lines = strtol(a + 14, NULL, 10);
        } else if (strcmp(a, "--config") == 0) {
            if (i + 1 >= argc) die("missing config path");
            o.config = argv[++i];
        } else if (strncmp(a, "--config=", 9) == 0) {
            o.config = a + 9;
        } else if (strcmp(a, "-e") == 0 || strcmp(a, "--regexp") == 0) {
            if (i + 1 >= argc) die("missing regex");
            o.pattern = argv[++i];
        } else if (a[0] == '-') {
            /* Drop unknown flags; they are used by system grep in pass-through mode. */
            continue;
        } else if (!o.pattern) {
            o.pattern = a;
        } else {
            vec_add(&paths, (char *)a);
        }
    }

    if (o.config) {
        long value = probe_setting(o.config);
        if (value > 0) o.probe_lines = value;
    }

    if (o.force_plain) {
        return delegate_to_grep(argc, argv, o.config);
    }

    if (!o.pattern) die("a pattern is required");

    int flags = REG_EXTENDED;
    if (o.count) flags |= REG_NOSUB;
    regex_t re;
    if (regcomp(&re, o.pattern, flags) != 0) die("invalid regular expression");

    int overall = 1;
    if (paths.n == 0) {
        char buf[8192];
        size_t len = 0;
        while (fgets(buf + len, sizeof(buf) - len, stdin)) {
            len += strlen(buf + len);
            if (len >= sizeof(buf) - 1) break;
        }
        char *line = buf;
        int use_unicode = o.force_unicode || has_math_glyph(line);
        if (!use_unicode) {
            regfree(&re);
            return delegate_to_grep(argc, argv, o.config);
        }
        /* Simplified stdin path: match over the buffered whole input. */
        for (char *p = strtok(line, "\n"); p; p = strtok(NULL, "\n")) {
            char *norm = NULL;
            if (normalize_text(p, &norm) != 0) die("out of memory");
            int match = regexec(&re, norm, 0, NULL, 0) == 0;
            free(norm);
            if (o.invert) match = !match;
            if (match) {
                if (!o.no_filename && o.with_filename) printf("stdin:");
                if (o.line_number) printf("%zu:", 1UL);
                printf("%s\n", p);
                overall = 0;
            }
        }
        regfree(&re);
        return overall;
    }

    int multi = paths.n > 1;
    for (size_t i = 0; i < paths.n; ++i) {
        const char *path = paths.v[i];
        if (strcmp(path, "-") == 0) continue;

        FILE *f = fopen(path, "r");
        if (!f) {
            perror(path);
            continue;
        }

        int glyph = 0;
        char *line = NULL;
        size_t cap = 0;
        for (long n = 0; n < o.probe_lines && getline(&line, &cap, f) >= 0; ++n) {
            if (has_math_glyph(line)) { glyph = 1; break; }
        }
        free(line);
        fclose(f);

        if (o.force_unicode || glyph) {
            if (search_file(path, &o, &re, multi) == 0) overall = 0;
        } else {
            int delegated = delegate_to_grep(argc, argv, o.config);
            if (delegated == 0) overall = 0;
        }
    }

    regfree(&re);
    free(paths.v);
    return overall;
}
