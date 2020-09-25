#include <alloca.h>
#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <unistd.h>
#include <libgen.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

void fail(const char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(1);
}

void error(const char* what)
{
    perror(what);
    exit(1);
}

/* --- bvalue ------------------------------------------------------------- */

enum bvalue_type
{
    BVALUE_INTEGER,
    BVALUE_BYTESTRING,
    BVALUE_LIST,
    BVALUE_DICTIONARY
};

struct bvalue
{
    enum bvalue_type type;
    union {
        int ivalue;
        struct {
            size_t size;
            size_t allocated;
            char data[1];
        } bsvalue;
        struct {
            struct bvalue *item;
            struct bvalue *tail;
        } lvalue;
        struct {
            struct bvalue *key;
            struct bvalue *value;
            struct bvalue *tail;
        } dvalue;
    } value;
};

struct bvalue* make_bvalue_integer(int n)
{
    struct bvalue* result = (struct bvalue*)malloc(sizeof(struct bvalue));
    result->type = BVALUE_INTEGER;
    result->value.ivalue = n;
    return result;
}

struct bvalue* allocate_bvalue_bytestring(size_t allocated)
{
    struct bvalue* result = (struct bvalue*)malloc(sizeof(struct bvalue) + allocated);
    result->type = BVALUE_BYTESTRING;
    result->value.bsvalue.size = 0;
    result->value.bsvalue.allocated = allocated;
    result->value.bsvalue.data[0] = '\0';
    return result;
}

struct bvalue* make_bvalue_bytestring(char* bytes, size_t size)
{
    struct bvalue* result = allocate_bvalue_bytestring(size);
    result->value.bsvalue.size = size;
    memcpy(result->value.bsvalue.data, bytes, size);
    result->value.bsvalue.data[size] = '\0';
    return result;
}

struct bvalue* make_bvalue_list(struct bvalue* item, struct bvalue* tail)
{
    struct bvalue* result = (struct bvalue*)malloc(sizeof(struct bvalue));
    result->type = BVALUE_LIST;
    result->value.lvalue.item = item;
    result->value.lvalue.tail = tail;
    return result;
}

struct bvalue* make_bvalue_dictionary(struct bvalue* key, struct bvalue* value, struct bvalue* tail)
{
    struct bvalue* result = (struct bvalue*)malloc(sizeof(struct bvalue));
    result->type = BVALUE_DICTIONARY;
    result->value.dvalue.key = key;
    result->value.dvalue.value = value;
    result->value.dvalue.tail = tail;
    return result;
}

void free_bvalue(struct bvalue* value)
{
    while (value)
    {
        struct bvalue* next = NULL;
        switch (value->type)
        {
        case BVALUE_INTEGER:
        case BVALUE_BYTESTRING:
            break;
        case BVALUE_LIST:
            free_bvalue(value->value.lvalue.item);
            next = value->value.lvalue.tail;
            break;
        case BVALUE_DICTIONARY:
            free_bvalue(value->value.dvalue.key);
            free_bvalue(value->value.dvalue.value);
            next = value->value.dvalue.tail;
            break;
        }
        free(value);
        value = next;
    }
}

_Bool bvalue_equals_string(struct bvalue* value, const char* s)
{
    if (!value)
        return false;
    if (BVALUE_BYTESTRING != value->type)
        return false;
    size_t length = strlen(s);
    if (length != value->value.bsvalue.size)
        return false;
    return !memcmp(value->value.bsvalue.data, s, length);
}

struct bvalue* bvalue_dictionary_get(struct bvalue* dictionary, const char* key)
{
    size_t key_length = strlen(key);
    for ( ; dictionary; dictionary = dictionary->value.dvalue.tail)
    {
        if (BVALUE_DICTIONARY != dictionary->type)
            continue;
        if (!dictionary->value.dvalue.key)
            continue;
        if (!bvalue_equals_string(dictionary->value.dvalue.key, key))
            continue;
        return dictionary->value.dvalue.value;
    }
    return NULL;
}

_Bool bvalue_list_contains_string(struct bvalue* list, const char* s)
{
    if (!list)
        return false;
    if (BVALUE_LIST != list->type)
        return false;
    for (; list; list = list->value.lvalue.tail)
        if (bvalue_equals_string(list->value.lvalue.item, s))
            return true;
    return false;
}

void bvalue_append(struct bvalue** value, const char* bytes, size_t length)
{
    if (0 == length)
        return;
    if (BVALUE_BYTESTRING != (*value)->type)
        fail("not a bytestring");
    size_t new_length = (*value)->value.bsvalue.size + length;
    size_t new_allocated = (*value)->value.bsvalue.allocated;
    while (new_allocated < new_length)
        new_allocated <<= 1;
    if (new_allocated == (*value)->value.bsvalue.allocated)
    {
        memcpy((*value)->value.bsvalue.data + (*value)->value.bsvalue.size, bytes, length);
        (*value)->value.bsvalue.size = new_length;
    }
    else
    {
        struct bvalue* old = *value;
        *value = allocate_bvalue_bytestring(new_allocated);
        (*value)->value.bsvalue.size = new_length;
        memcpy((*value)->value.bsvalue.data, old->value.bsvalue.data, old->value.bsvalue.size);
        memcpy((*value)->value.bsvalue.data + old->value.bsvalue.size, bytes, length);
        free_bvalue(old);
    }
}

const char PERCENT = '%';
const char NEWLINE = '\n';

struct bvalue* bvalue_format(struct bvalue* value, const char* format)
{
    struct bvalue* result = allocate_bvalue_bytestring(128);
    for (const char* p = format; *p; p++)
    {
        if (*p != '%')
        {
            bvalue_append(&result, p, 1);
            continue;
        }
        switch (p[1])
        {
        case '%':
            p++;
            bvalue_append(&result, &PERCENT, 1);
            break;
        case 'n':
            p++;
            bvalue_append(&result, &NEWLINE, 1);
            break;
        case '{':
            p += 2;
            const char* end = strchr(p, '}');
            if (!end)
                fail("no closing brace in format");
            char *key_name = (char*)malloc(end - p + 1);
            memcpy(key_name, p, end - p);
            key_name[end - p] = '\0';
            struct bvalue* embed = bvalue_dictionary_get(value, key_name);
            free(key_name);
            if (embed && BVALUE_BYTESTRING == embed->type)
                bvalue_append(&result, embed->value.bsvalue.data, embed->value.bsvalue.size);
            p = end;
            break;
        default:
            fail("invalid character in format");
        }
    }
    return result;
}

/* --- breader ------------------------------------------------------------ */

struct breader
{
    int fd;
    int peeked_char;
};

struct bvalue* breader_read(struct breader* reader);

struct breader* make_breader(int fd)
{
    struct breader* reader = (struct breader*)malloc(sizeof(struct breader));
    reader->fd = fd;
    reader->peeked_char = EOF;
    return reader;
}

void free_breader(struct breader* reader)
{
    free(reader);
}

int bread_next_char(struct breader* reader)
{
    if (EOF != reader->peeked_char)
    {
        int result = reader->peeked_char;
        reader->peeked_char = EOF;
        return result;
    }
    char ch = 0;
    int count = recv(reader->fd, &ch, 1, 0);
    if (count < 0)
        error("recv");
    if (0 == count)
        return EOF;
    return ch;
}

int bread_peek_char(struct breader* reader)
{
    if (EOF == reader->peeked_char)
        reader->peeked_char = bread_next_char(reader);
    return reader->peeked_char;
}

struct bvalue* bread_dictionary(struct breader* reader)
{
    struct bvalue* result = NULL;
    struct bvalue** iterator = &result;

    bread_next_char(reader);
    while('e' != bread_peek_char(reader))
    {
        struct bvalue* key = breader_read(reader);
        struct bvalue* value = breader_read(reader);
        *iterator = make_bvalue_dictionary(key, value, NULL);
        iterator = &(*iterator)->value.dvalue.tail;
    }
    bread_next_char(reader);
    return result;
}

struct bvalue* bread_list(struct breader* reader)
{
    struct bvalue* result = NULL;
    struct bvalue** iterator = &result;
    bread_next_char(reader);
    while('e' != bread_peek_char(reader))
    {
        struct bvalue* item = breader_read(reader);
        *iterator = make_bvalue_list(item, NULL);
        iterator = &(*iterator)->value.lvalue.tail;
    }
    bread_next_char(reader);
    return result;
}

struct bvalue* bread_integer(struct breader* reader)
{
    int value = 0;
    _Bool negative = false;
    bread_next_char(reader);
    while('e' != bread_peek_char(reader))
    {
        int ch = bread_next_char(reader);
        if (ch == '-')
            negative = true;
        else
            value = value * 10 + (ch - '0');
    }
    bread_next_char(reader);
    if (negative)
        value = -value;
    return make_bvalue_integer(value);
}

struct bvalue* bread_bytestring(struct breader* reader)
{
    size_t length = 0;
    char *bytes = NULL;
    while (':' != bread_peek_char(reader))
        length = length*10 + (bread_next_char(reader) - '0');
    bread_next_char(reader);
    
    bytes = (char*)malloc(length + 1);
    if (NULL == bytes)
        error("malloc");
    for (size_t i = 0; i < length; i++)
        bytes[i] = bread_next_char(reader);
    bytes[length] = '\0';
    struct bvalue* result = make_bvalue_bytestring(bytes, length);
    free(bytes);
    return result;
}

struct bvalue* breader_read(struct breader* reader)
{
    switch (bread_peek_char(reader))
    {
    case 'd':
        return bread_dictionary(reader);
    case 'l':
        return bread_list(reader);
    case 'i':
        return bread_integer(reader);
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return bread_bytestring(reader);
    case EOF:
        fail("Unexpected EOF");
    default:
        fail("bad character in nREPL stream");
    }
    return NULL;
}

/* -- print option -------------------------------------------------------- */

struct print_option
{
    struct print_option* next;
    char* key;
    int fd;
    char* format;
};

struct print_option* make_print_option(const char* optarg)
{
    struct print_option* print = (struct print_option*)malloc(sizeof(struct print_option));
    memset(print, 0, sizeof(struct print_option));
    print->next = NULL;
    print->fd = 1;
    if (!strchr(optarg, ','))
    {
        print->key = strdup(optarg);
        print->format = (char*)malloc(strlen(optarg) + 4);
        sprintf(print->format, "%%{%s}", optarg);
    }
    else
    {
        print->key = strdup(optarg);
        *strchr(print->key, ',') = '\0';

        const char* p = strchr(optarg, ',') + 1;
        print->fd = atoi(p);
        p = strchr(p, ',');
        if (NULL == p)
            fail("--print option is either KEY or KEY,FD,FORMAT");
        ++p;
        print->format = strdup(p);
    }
    return print;
}

void free_print_options(struct print_option* print)
{
    while (print)
    {
        if (print->key)
            free(print->key);
        if (print->format)
            free(print->format);
        void* p = print;
        print = print->next;
        free(p);
    }
}

void append_print_option(struct print_option** options, struct print_option* new)
{
    while (*options)
        options = &(*options)->next;
    *options = new;
}

struct print_option* make_default_print_options(void)
{
    struct print_option* result = make_print_option("out,1,%{out}");
    append_print_option(&result, make_print_option("err,2,%{err}"));
    append_print_option(&result, make_print_option("value,1,%{value}%n"));
    return result;
}

/* -- options ------------------------------------------------------------- */

struct options
{
    char* port;
    char* namespace;
    char* op;
    char* code;
    char* filename;
    int line;
    int column;
    char* send;
    _Bool help;
    struct print_option* print;
};

struct sockaddr_in options_address(struct options* options, const char* port);

char *read_file(const char* filename, char* buffer, size_t buffer_size)
{
    FILE *portfile = fopen(filename, "r");
    if (NULL == portfile)
        return NULL;
    if (NULL == fgets(buffer, buffer_size, portfile))
    {
        fclose(portfile);
        return NULL;
    }
    fclose(portfile);
    return buffer;
}

struct sockaddr_in options_address_from_file(struct options* options, const char* filename)
{
    char linebuffer[256];
    if (!read_file(filename, linebuffer, sizeof(linebuffer)))
        error(filename);
    return options_address(options, linebuffer);
}

struct sockaddr_in options_address_from_relative_file(struct options* options, const char* directory_in, const char* filename)
{
    char *directory = strdup(directory_in);
    for (;;)
    {
        struct stat statb;
        char* path_to_check = (char*)malloc(strlen(directory) + strlen(filename) + 2);
        sprintf(path_to_check, "%s/%s", directory, filename);
        if (0 == stat(path_to_check, &statb))
        {
            struct sockaddr_in result = options_address_from_file(options, path_to_check);
            free(path_to_check);
            free(directory);
            return result;
        }
        free(path_to_check);

        char* old_directory = strdup(directory);
        char* parent_directory = dirname(directory);
        if (!strcmp(old_directory, parent_directory))
        {
            char error_message[PATH_MAX + 128];
            sprintf(error_message, "rep: No ancestor of %s contains %s", directory_in, filename);
            fail(error_message);
        }
        free(old_directory);
        char* new_directory = strdup(parent_directory);
        free(directory);
        directory = new_directory;
    }
}

char* make_path_absolute(const char* path)
{
    if (*path == '/')
        return strdup(path);
    char* absolute_directory = (char*)malloc(strlen(path) + PATH_MAX + 2);
    getcwd(absolute_directory, PATH_MAX);
    strcat(absolute_directory, "/");
    strcat(absolute_directory, path);
    return absolute_directory;
}

struct sockaddr_in options_address(struct options* options, const char* port)
{
    if (*port == '@' && !strchr(port + 1, '@'))
        return options_address_from_file(options, port + 1);
    else if (*port == '@')
    {
        const char* relative_directory = strchr(port + 1, '@') + 1;
        char* absolute_directory = make_path_absolute(relative_directory);
        char* filename = strdup(port + 1);
        *strchr(filename, '@') = '\0';
        struct sockaddr_in result = options_address_from_relative_file(options, absolute_directory, filename);
        free(absolute_directory);
        free(filename);
        return result;
    }
    struct sockaddr_in address =
    {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    if (strchr(port, ':'))
    {
        char *host_part = alloca(strlen(port));
        strcpy(host_part, port);
        *strchr(host_part, ':') = '\0';
        if (!inet_aton(host_part, &address.sin_addr))
        {
            struct hostent *ent = gethostbyname(host_part);
            if (NULL == ent)
                error(host_part);
            if (NULL == ent->h_addr)
            {
                fprintf(stderr, "%s has no addresses\n", host_part);
                exit(1);
            }
            address.sin_addr.s_addr = *(u_long *)ent->h_addr_list[0];
        }
        port = strchr(port, ':') + 1;
    }
    address.sin_port = htons(atoi(port));
    return address;
}

char* collect_code(int argc, char *argv[], int start)
{
    size_t code_size = 0;
    for (int i = optind; i < argc; ++i)
        code_size += strlen(argv[i]) + 1;
    char *code = (char *)malloc(code_size);
    code[0] = '\0';
    for (int i = optind; i < argc; ++i)
    {
        if (code[0])
            strcat(code, " ");
        strcat(code, argv[i]);
    }
    return code;
}

enum {
    OPT_OP = 127,
    OPT_PRINT,
    OPT_SEND,
};


const char SHORT_OPTIONS[] = "hl:n:p:";
const struct option LONG_OPTIONS[] =
{
    { "help",      0, NULL, 'h' },
    { "line",      1, NULL, 'l' },
    { "namespace", 1, NULL, 'n' },
    { "op",        1, NULL, OPT_OP },
    { "port",      1, NULL, 'p' },
    { "print",     1, NULL, OPT_PRINT },
    { "send",      1, NULL, OPT_SEND },
    { NULL,        0, NULL, 0 }
};

struct options* new_options(void)
{
    struct options* options = (struct options*)malloc(sizeof(struct options));
    options->port = strdup("@.nrepl-port");
    options->op = strdup("eval");
    options->namespace = strdup("user");
    options->code = NULL;
    options->help = false;
    options->line = -1;
    options->column = -1;
    options->filename = NULL;
    options->send = strdup("");
    options->print = make_default_print_options();
    return options;
}

void options_parse_line(struct options* options, const char* line)
{
    int colons = 0;
    int nondigits = 0;
    for (const char* p = line; *p; ++p)
    {
        if (*p == ':')
            ++colons;
        else if (!isdigit(*p))
            ++nondigits;
    }
    if (2 == colons)
    {
        options->filename = strdup(line);
        *strchr(options->filename, ':') = '\0';
        const char* p = strchr(line, ':') + 1;
        options->line = atoi(p);
        p = strchr(p, ':') + 1;
        options->column = atoi(p);
    }
    else if (1 == colons && 0 == nondigits)
    {
        options->line = atoi(line);
        options->column = atoi(strchr(line, ':') + 1);
    }
    else if (1 == colons)
    {
        options->filename = strdup(line);
        *strchr(options->filename, ':') = '\0';
        options->line = atoi(strchr(line, ':') + 1);
    }
    else if (0 == colons && 0 == nondigits)
        options->line = atoi(line);
    else if (0 == colons)
    {
        options->filename = strdup(line);
        options->line = 1;
    }
    else
        fail("invalid value for --line");
}

void options_parse_send(struct options* options, const char* send)
{
    char* key = NULL;
    char* type = NULL;

    if (NULL == strchr(send, ','))
        fail("--send value must be KEY,TYPE,VALUE");
    key = strdup(send);
    *strchr(key, ',') = '\0';
    send = strchr(send, ',') + 1;

    if (NULL == strchr(send, ','))
        fail("--send value must be KEY,TYPE,VALUE");
    type = strdup(send);
    *strchr(type, ',') = '\0';
    send = strchr(send, ',') + 1;

    options->send = (char*)realloc(options->send, strlen(options->send) + strlen(key) + 16 + strlen(send) + 16);
    sprintf(options->send + strlen(options->send), "%lu:%s", strlen(key), key);
    if (!strcmp(type, "string"))
        sprintf(options->send + strlen(options->send), "%lu:%s", strlen(send), send);
    else if (!strcmp(type, "integer"))
        sprintf(options->send + strlen(options->send), "i%de", atoi(send));
    else
        fail("--send TYPE must be 'string' or 'integer'");
}

struct options* parse_options(int argc, char* argv[])
{
    struct options* options = new_options();
    int opt;
    _Bool have_print = false;
    while ((opt = getopt_long(argc, argv, SHORT_OPTIONS, LONG_OPTIONS, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            options->help = true;
            break;
        case 'l':
            options_parse_line(options, optarg);
            break;
        case 'n':
            free(options->namespace);
            options->namespace = strdup(optarg);
            break;
        case 'p':
            free(options->port);
            options->port = strdup(optarg);
            break;
        case OPT_OP:
            free(options->op);
            options->op = strdup(optarg);
            break;
        case OPT_PRINT:
            if (!have_print)
            {
                free_print_options(options->print);
                options->print = NULL;
                have_print = true;
            }
            append_print_option(&options->print, make_print_option(optarg));
            break;
        case OPT_SEND:
            options_parse_send(options, optarg);
            break;
        case '?':
            exit(2);
        }
    }
    options->code = collect_code(argc, argv, optind);
    return options;
}

void free_options(struct options* options)
{
    if (options->port)
        free(options->port);
    if (options->op)
        free(options->op);
    if (options->namespace)
        free(options->namespace);
    if (options->code)
        free(options->code);
    if (options->filename)
        free(options->filename);
    if (options->send)
        free(options->send);
    free_print_options(options->print);
    free(options);
}

/* -- nrepl --------------------------------------------------------------- */

struct nrepl
{
    struct options* options;
    int fd;
    struct breader *decode;
    _Bool exception_occurred;
    char* session;
};

struct nrepl* make_nrepl(struct options* options)
{
    struct nrepl* nrepl = (struct nrepl*)malloc(sizeof(struct nrepl));
    nrepl->options = options;
    nrepl->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nrepl->fd == -1)
        error("socket");
    nrepl->decode = make_breader(nrepl->fd);
    nrepl->exception_occurred = false;
    nrepl->session = NULL;
    return nrepl;
}

void free_nrepl(struct nrepl* nrepl)
{
    free_options(nrepl->options);
    if (nrepl->fd >= 0)
        close(nrepl->fd);
    if (nrepl->decode)
        free_breader(nrepl->decode);
    if (nrepl->session)
        free(nrepl->session);
    free(nrepl);
}

void nrepl_receive_until_done(struct nrepl* nrepl)
{
    _Bool done = false;
    while (!done)
    {
        struct bvalue* reply = breader_read(nrepl->decode);

        struct bvalue* new_session = bvalue_dictionary_get(reply, "new-session");
        if (new_session && BVALUE_BYTESTRING == new_session->type)
        {
            if (nrepl->session)
                free(nrepl->session);
            nrepl->session = strdup(new_session->value.bsvalue.data);
        }

        for (struct print_option* print = nrepl->options->print; print; print = print->next)
        {
            if (NULL == bvalue_dictionary_get(reply, print->key))
                continue;
            struct bvalue* s = bvalue_format(reply, print->format);
            write(print->fd, s->value.bsvalue.data, s->value.bsvalue.size);
            free_bvalue(s);
        }

        struct bvalue* ex = bvalue_dictionary_get(reply, "ex");
        if (ex)
            nrepl->exception_occurred = true;

        struct bvalue* status = bvalue_dictionary_get(reply, "status");
        if (bvalue_equals_string(status, "done") || bvalue_list_contains_string(status, "done"))
            done = true;
        free_bvalue(reply);
    }
}

void nrepl_send(struct nrepl* nrepl, const char* format, ...)
{
    va_list vargs;

    va_start(vargs, format);
    size_t length = vsnprintf(NULL, 0, format, vargs);
    va_end(vargs);

    char* message = (char*)malloc(length + 1);
    va_start(vargs, format);
    vsnprintf(message, length+1, format, vargs);
    va_end(vargs);

    if (send(nrepl->fd, message, length, 0) != length)
        error("send");

    free(message);

    nrepl_receive_until_done(nrepl);
}

int nrepl_exec(struct nrepl* nrepl)
{
    nrepl->exception_occurred = false;

    struct sockaddr_in address = options_address(nrepl->options, nrepl->options->port);
    if (-1 == connect(nrepl->fd, (struct sockaddr*)&address, sizeof(address)))
        error("connect");

    nrepl_send(nrepl, "d2:op5:clonee");

    char extra_options[512] = "";
    if (nrepl->options->line != -1)
        sprintf(extra_options + strlen(extra_options), "4:linei%de", nrepl->options->line);
    if (nrepl->options->column != -1)
        sprintf(extra_options + strlen(extra_options), "6:columni%de", nrepl->options->column);
    if (nrepl->options->filename)
        sprintf(extra_options + strlen(extra_options), "4:file%lu:%s", strlen(nrepl->options->filename), nrepl->options->filename);

    nrepl_send(nrepl, "d2:op%lu:%s2:ns%lu:%s7:session%lu:%s4:code%lu:%s%s%se",
        strlen(nrepl->options->op), nrepl->options->op,
        strlen(nrepl->options->namespace), nrepl->options->namespace,
        strlen(nrepl->session), nrepl->session,
        strlen(nrepl->options->code), nrepl->options->code,
        nrepl->options->send,
        extra_options);

    nrepl_send(nrepl, "d2:op5:close7:session%lu:%se",
        strlen(nrepl->session), nrepl->session);

    if (nrepl->exception_occurred)
        return 1;
    return 0;
}

/* ------------------------------------------------------------------------ */

void help(void)
{
    printf("\
rep: Single-shot nREPL client\n\
Synopsis:\n\
  rep [OPTIONS] [--] [CODE ...]\n\
Options:\n\
  -h, --help                      Show this help screen.\n\
  -l, --line=[FILE:]LINE[:COLUMN] Set reference file, line, and column for errors.\n\
  -n, --namespace=NS              Evaluate code in NS (default: user).\n\
  --op=OP                         nREPL operation (default: eval).\n\
  -p, --port=ADDRESS              TCP port, host:port, @portfile, or @FNAME@RELATIVE.\n\
  --print=KEY|KEY,FD,FORMAT       Print FORMAT to FD when KEY is present.\n\
  --send=KEY,TYPE,VALUE           Send additional KEY of VALUE in request.\n\
\n");
}

int main(int argc, char *argv[])
{
    struct options* options = parse_options(argc, argv);
    if (options->help)
    {
        help();
        exit(0);
    }
    struct nrepl* nrepl = make_nrepl(options);
    int error_code = nrepl_exec(nrepl);
    free_nrepl(nrepl);
    exit(error_code);
}
