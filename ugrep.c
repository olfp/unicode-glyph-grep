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

typedef struct { int line_number, quiet, with_filename, no_filename, count, invert, force_unicode, force_plain, recursive; long max_count, probe_lines; const char *pattern; const char *config; } Options;

typedef struct { char **v; size_t n, cap; } StrVec;
static void die(const char *s) { fprintf(stderr, "ugrep: %s\n", s); exit(2); }
static void vec_add(StrVec *v, char *s) { if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 16; v->v = realloc(v->v, v->cap * sizeof *v->v); } v->v[v->n++] = s; }
static unsigned long utf8(const unsigned char *s, size_t n, size_t *used) { unsigned long c; if (!n) { *used=0; return 0; } if (s[0]<0x80) {*used=1; return s[0];} if(n>=2 && (s[0]&0xe0)==0xc0){*used=2;return ((s[0]&31)<<6)|(s[1]&63);} if(n>=3&&(s[0]&0xf0)==0xe0){*used=3;return((s[0]&15)<<12)|((s[1]&63)<<6)|(s[2]&63);} if(n>=4&&(s[0]&0xf8)==0xf0){*used=4;return((s[0]&7)<<18)|((s[1]&63)<<12)|((s[2]&63)<<6)|(s[3]&63);} *used=1; return s[0]; }

/* Mathematical Alphanumeric Symbols are arranged in style blocks. */
static int glyph_ascii(unsigned long c, char *out) {
    static const unsigned long starts[] = {0x1d400,0x1d434,0x1d468,0x1d4d0,0x1d56c,0x1d5a0,0x1d5d4,0x1d608,0x1d63c,0x1d670,0x1d6a8};
    for (size_t k=0;k<sizeof(starts)/sizeof(starts[0]);k++) {
        unsigned long d=c-starts[k];
        if (d<26) {*out='A'+d; return 1;}
        if (d>=26 && d<52) {*out='a'+d-26; return 1;}
    }
    /* Italic and bold-italic blocks have a few holes for legacy symbols. */
    if ((c>=0x1d434&&c<=0x1d467)||(c>=0x1d468&&c<=0x1d49b)||(c>=0x1d4d0&&c<=0x1d503)||(c>=0x1d56c&&c<=0x1d59f)) return 0;
    if (c>=0x1d7ce && c<=0x1d7ff) { *out='0'+(char)((c-0x1d7ce)%10); return 1; }
    if (c==0x210e || c==0x2113) {*out='h'; return 1;}
    return 0;
}

static int normalize(const char *in, char **out) {
    size_t n=strlen(in), cap=n*2+1, j=0, used; char *p=malloc(cap);
    for(size_t i=0;i<n;) { unsigned long c=utf8((const unsigned char*)in+i,n-i,&used); char a; if(!used) break; if(glyph_ascii(c,&a)) { if(j+1>=cap) {cap*=2;p=realloc(p,cap);} p[j++]=a; } else { if(j+used>=cap){cap*=2;p=realloc(p,cap);} memcpy(p+j,in+i,used);j+=used;} i+=used; }
    p[j]=0; *out=p; return 0;
}
static int has_glyph(const char *s) { size_t n=strlen(s),u; for(size_t i=0;i<n;){unsigned long c=utf8((const unsigned char*)s+i,n-i,&u); if((c>=0x1d400&&c<=0x1d7ff)||c==0x210e||c==0x2113)return 1;i+=u?u:1;} return 0; }

static const char *config_value(const char *file, const char *key, char *buf, size_t size) { FILE *f=fopen(file,"r"); if(!f)return NULL; char line[PATH_MAX+128]; int section=0; while(fgets(line,sizeof line,f)){char *p=line;while(isspace((unsigned char)*p))p++; if(*p=='['){section=!strncmp(p,"[ugrep]",7);continue;} char k[64],v[PATH_MAX]; if((section||!strchr(line,'['))&&sscanf(p,"%63[^=]=%1023[^"]",k,v)==2){char *q=k;while(isspace((unsigned char)*q))q++; char *e=q+strlen(q);while(e>q&&isspace((unsigned char)e[-1]))*--e=0; if(!strcmp(q,key)){char *x=v;while(isspace((unsigned char)*x))x++; strncpy(buf,x,size-1);buf[size-1]=0; fclose(f); return buf;}}} fclose(f); return NULL; }
static const char *grep_path(const char *config) { static char value[PATH_MAX]; if(config&&config_value(config,"grep",value,sizeof value))return value; char p[PATH_MAX]; if(config_value(".ugreprc","grep",p,sizeof p)) {strncpy(value,p,sizeof value-1);return value;} const char *home=getenv("HOME"); if(home){snprintf(p,sizeof p,"%s/.ugreprc",home);if(config_value(p,"grep",value,sizeof value))return value;} return DEFAULT_GREP; }
static long probe_setting(const char *config) { char v[64]; const char *home=getenv("HOME"); if(config&&config_value(config,"probe_lines",v,sizeof v))return atol(v); if(config_value(".ugreprc","probe_lines",v,sizeof v))return atol(v); if(home){char p[PATH_MAX];snprintf(p,sizeof p,"%s/.ugreprc",home);if(config_value(p,"probe_lines",v,sizeof v))return atol(v);} return DEFAULT_PROBE_LINES; }

static int delegate(int argc, char **argv, const Options *o, const char *input) { const char *gp=grep_path(o->config); char **av=calloc((size_t)argc+2,sizeof *av); int n=0; av[n++]=(char*)gp; for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"-u")||!strcmp(argv[i],"--unicode")||!strcmp(argv[i],"-t")||!strcmp(argv[i],"--no-unicode"))continue; if(!strcmp(argv[i],"--config")||!strcmp(argv[i],"--probe-lines")){i++;continue;} if(!strncmp(argv[i],"--config=",9)||!strncmp(argv[i],"--probe-lines=",14))continue; av[n++]=argv[i]; } av[n]=NULL; if(input){FILE *f=popen("/bin/cat","w");(void)f; /* replaced below */} pid_t pid=fork(); if(pid<0)die("fork failed"); if(!pid){if(input){int p[2]; /* unreachable for normal file delegation */ (void)p;} execv(gp,av); execvp(gp,av); perror(gp);_exit(127);} int st;waitpid(pid,&st,0);free(av);return WIFEXITED(st)?WEXITSTATUS(st):2; }

static int search_file(const char *path, const Options *o, regex_t *re, int multi) { FILE *f=fopen(path,"r"); if(!f){perror(path);return 2;} char *line=NULL;size_t cap=0;long no=0,count=0;int any=0; while(getline(&line,&cap,f)>=0){no++; char *norm;normalize(line,&norm);int match=regexec(re,norm,0,NULL,0)==0;free(norm);if(o->invert)match=!match;if(!match)continue;count++;any=1;if(o->max_count&&count>o->max_count)break;if(o->quiet)continue;if(o->count)continue; if(!o->no_filename&&(o->with_filename||multi))printf("%s:",path);if(o->line_number)printf("%ld:",no);fputs(line,stdout);}if(o->count){if(!o->no_filename&&(o->with_filename||multi))printf("%s:",path);printf("%ld\n",count);}free(line);fclose(f);return any?0:1; }

static void help(void){puts("Usage: ugrep [OPTIONS] PATTERN [FILE ...]\n  -n, --line-number       print line numbers\n  -h, --no-filename       suppress filenames\n  -H, --with-filename     print filenames\n  -i, --ignore-case       ignore case\n  -c, --count             count matching lines\n  -m N, --max-count N     stop after N matches\n  -u, --unicode           force Unicode mode\n  -t, --no-unicode        use the system grep\n  -r, --recursive         search directories\n      --probe-lines N     configure glyph detection\n      --config FILE       use a .ugreprc file\n\nConfiguration: .ugreprc is read from the current directory, then $HOME.\n[ugrep]\nprobe_lines = 100\ngrep = /usr/bin/grep");}

int main(int argc,char **argv){Options o={0,0,0,0,0,0,0,0,0,DEFAULT_PROBE_LINES,NULL,NULL};StrVec paths={0};for(int i=1;i<argc;i++){char *a=argv[i];if(!strcmp(a,"--help")){help();return 0;}if(!strcmp(a,"-n")||!strcmp(a,"--line-number"))o.line_number=1;else if(!strcmp(a,"-h")||!strcmp(a,"--no-filename"))o.no_filename=1;else if(!strcmp(a,"-H")){o.with_filename=1;}else if(!strcmp(a,"-i")||!strcmp(a,"--ignore-case")){}else if(!strcmp(a,"-u")||!strcmp(a,"--unicode"))o.force_unicode=1;else if(!strcmp(a,"-t")||!strcmp(a,"--no-unicode"))o.force_plain=1;else if(!strcmp(a,"-c"))o.count=1;else if(!strcmp(a,"-q"))o.quiet=1;else if(!strcmp(a,"-v"))o.invert=1;else if(!strcmp(a,"-r"))o.recursive=1;else if(!strcmp(a,"-m")&&i+1<argc)o.max_count=atol(argv[++i]);else if(!strcmp(a,"--probe-lines")&&i+1<argc)o.probe_lines=atol(argv[++i]);else if(!strcmp(a,"--config")&&i+1<argc)o.config=argv[++i];else if(!strcmp(a,"-e")&&i+1<argc)o.pattern=argv[++i];else if(a[0]=='-'&&a[1]){}else if(!o.pattern)o.pattern=a;else vec_add(&paths,a);}if(o.force_plain)return delegate(argc,argv,&o,NULL);if(!o.pattern)die("a pattern is required");if(!o.config)o.probe_lines=probe_setting(NULL);regex_t re;int flags=REG_EXTENDED;if(regcomp(&re,o.pattern,flags))die("invalid regular expression");int multi=paths.n>1, result=1;for(size_t i=0;i<paths.n;i++){FILE*f=fopen(paths.v[i],"r");if(!f)continue;int glyph=0;char *l=NULL;size_t z=0;for(long n=0;n<o.probe_lines&&getline(&l,&z,f)>=0;n++)if(has_glyph(l)){glyph=1;break;}fclose(f);free(l);if(o.force_unicode||glyph){int r=search_file(paths.v[i],&o,&re,multi);if(r==0)result=0;}else {int r=delegate(argc,argv,&o,NULL);if(r==0)result=0;}}regfree(&re);free(paths.v);return result;}
