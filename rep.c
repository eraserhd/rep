#include <ctype.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <alloca.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#elif defined(_WIN32) || defined(WIN32)
#include <malloc.h>
#include <winsock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <unistd.h>
#include <libgen.h>

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

void fail(const char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(255);
}

void options_fail(const char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(2);
}

void error(const char* what)
{
    perror(what);
    exit(255);
}

char* strdup_up_to(const char* input, char ch)
{
    char* result = strdup(input);
    char* p = strchr(result, ch);
    if (p)
        *p = '\0';
    return result;
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

_Bool bvalue_has_status(struct bvalue* reply, const char* status_text)
{
    struct bvalue* status = bvalue_dictionary_get(reply, "status");
    if (!status)
        return false;
    if (bvalue_equals_string(status, status_text))
        return true;
    if (bvalue_list_contains_string(status, status_text))
        return true;
    return false;
}

void bvalue_append_string(struct bvalue** value, const char* bytes, size_t length)
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

void bvalue_append_bvalue(struct bvalue** targetp, struct bvalue* value)
{
    char ivalue[64];
    switch (value->type)
    {
    case BVALUE_INTEGER:
        sprintf(ivalue, "%d", value->value.ivalue);
        bvalue_append_string(targetp, ivalue, strlen(ivalue));
        break;
    case BVALUE_BYTESTRING:
        bvalue_append_string(targetp, value->value.bsvalue.data, value->value.bsvalue.size);
        break;
    case BVALUE_LIST:
    case BVALUE_DICTIONARY:
        fail("unhandled case of bvalue_append_bvalue()");
        break;
    }
}

const char PERCENT = '%';
const char NEWLINE = '\n';

void bvalue_append_format(struct bvalue** targetp, struct bvalue* value, const char* format)
{
    for (const char* p = format; *p; p++)
    {
        if (*p != '%')
        {
            bvalue_append_string(targetp, p, 1);
            continue;
        }
        switch (p[1])
        {
        case '%':
            p++;
            bvalue_append_string(targetp, &PERCENT, 1);
            break;
        case 'n':
            p++;
            bvalue_append_string(targetp, &NEWLINE, 1);
            break;
        case '.':
            p++;
            bvalue_append_bvalue(targetp, value);
            break;
        case '{':
            p += 2;
            const char* end = strchr(p, '}');
            if (!end)
                fail("no closing brace in format");
            char *key_name = NULL;
            char *element_pattern = NULL;
            if (strchr(p, ',') && strchr(p, ',') < end)
            {
                key_name = strdup_up_to(p, ',');
                element_pattern = strdup_up_to(strchr(p, ',') + 1, '}');
            }
            else
            {
                key_name = strdup_up_to(p, '}');
                element_pattern = strdup("%.%n");
            }
            struct bvalue* embed = bvalue_dictionary_get(value, key_name);
            free(key_name);
            if (embed)
            {
                if (BVALUE_LIST == embed->type)
                {
                    for (struct bvalue* i = embed; i; i = i->value.lvalue.tail)
                        bvalue_append_format(targetp, i->value.lvalue.item, element_pattern);
                }
                else
                    bvalue_append_bvalue(targetp, embed);
            }
            free(element_pattern);
            p = end;
            break;
        default:
            fail("invalid character in format");
        }
    }
}

struct bvalue* bvalue_format(struct bvalue* value, const char* format)
{
    struct bvalue* result = allocate_bvalue_bytestring(128);
    bvalue_append_format(&result, value, format);
    return result;
}


void bvalue_dump(struct bvalue* value, const char* prefix)
{
    switch (value->type)
    {
    case BVALUE_BYTESTRING:
        printf("\"%s\"", value->value.bsvalue.data);
        break;
    case BVALUE_INTEGER:
        printf("%d", value->value.ivalue);
        break;
    case BVALUE_DICTIONARY:
        {
            printf("{");
            char *new_prefix = (char*)malloc(strlen(prefix)+3);
            strcpy(new_prefix, prefix);
            strcat(new_prefix, "  ");
            for (struct bvalue* iterator = value; iterator; iterator = iterator->value.dvalue.tail)
            {
                printf("\n%s", new_prefix);
                bvalue_dump(iterator->value.dvalue.key, new_prefix);
                printf(": ");
                bvalue_dump(iterator->value.dvalue.value, new_prefix);
            }
            free(new_prefix);
            printf("\n%s}", prefix);
        }
        break;
    case BVALUE_LIST:
        {
            printf("[");
            char *new_prefix = (char*)malloc(strlen(prefix)+3);
            strcpy(new_prefix, prefix);
            strcat(new_prefix, "  ");
            for (struct bvalue* iterator = value; iterator; iterator = iterator->value.lvalue.tail)
            {
                printf("\n%s", new_prefix);
                bvalue_dump(iterator->value.lvalue.item, new_prefix);
            }
            free(new_prefix);
            printf("\n%s]", prefix);
        }
        break;
    }
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
        print->key = strdup_up_to(optarg, ',');
        const char* p = strchr(optarg, ',') + 1;
        print->fd = atoi(p);
        p = strchr(p, ',');
        if (NULL == p)
            options_fail("--print option is either KEY or KEY,FD,FORMAT");
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

void remove_print_option(struct print_option** options, const char* key)
{
    while (*options)
    {
        if (!strcmp((*options)->key, key))
        {
            struct print_option* dead = *options;
            *options = (*options)->next;
            dead->next = NULL;
            free_print_options(dead);
        }
        else
            options = &(*options)->next;
    }
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
    _Bool verbose;
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
#ifdef __unix__
        struct stat statb;
#elif defined(_WIN32) || defined(WIN32)
        struct _stat statb;
#endif
        char* path_to_check = (char*)malloc(strlen(directory) + strlen(filename) + 2);
        sprintf(path_to_check, "%s/%s", directory, filename);
#ifdef __unix__
        if (0 == stat(path_to_check, &statb))
#elif defined(_WIN32) || defined(WIN32)
        if (0 == _stat(path_to_check, &statb))
#endif
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
    if (NULL == getcwd(absolute_directory, PATH_MAX))
        error("getcwd");
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
        char* filename = strdup_up_to(port + 1, '@');
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
        char *host_part = strdup_up_to(port, ':');
#ifdef __unix__
        if (!inet_aton(host_part, &address.sin_addr))
#elif defined(_WIN32) || defined(WIN32)
        size_t hostl = strlen(host_part) + 1;
        wchar_t *whost = calloc(sizeof(wchar_t), hostl);
        mbstowcs(whost, host_part, hostl);
        LPCWSTR whost_part = whost;
        if (!InetPtonW(AF_INET, whost_part, &address.sin_addr))
#endif
        {
            struct hostent *ent = gethostbyname(host_part);
            if (NULL == ent)
                error(host_part);
            if (NULL == ent->h_addr)
            {
                fprintf(stderr, "%s has no addresses\n", host_part);
                exit(255);
            }
            address.sin_addr.s_addr = *(u_long *)ent->h_addr_list[0];
        }
        free(host_part);
#if defined(_WIN32) || defined(WIN32)
        whost_part = NULL;
        free(whost);
#endif
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
    OPT_NO_PRINT,
    OPT_PRINT,
    OPT_SEND,
};


const char SHORT_OPTIONS[] = "hl:n:p:v";
const struct option LONG_OPTIONS[] =
{
    { "help",      0, NULL, 'h' },
    { "line",      1, NULL, 'l' },
    { "namespace", 1, NULL, 'n' },
    { "no-print",  1, NULL, OPT_NO_PRINT },
    { "op",        1, NULL, OPT_OP },
    { "port",      1, NULL, 'p' },
    { "print",     1, NULL, OPT_PRINT },
    { "send",      1, NULL, OPT_SEND },
    { "verbose",   0, NULL, 'v' },
    { NULL,        0, NULL, 0 }
};

struct options* new_options(void)
{
    struct options* options = (struct options*)malloc(sizeof(struct options));
    options->port = strdup("@.nrepl-port@.");
    options->op = strdup("eval");
    options->namespace = strdup("user");
    options->code = NULL;
    options->help = false;
    options->line = -1;
    options->column = -1;
    options->filename = NULL;
    options->send = strdup("");
    options->print = make_default_print_options();
    options->verbose = false;
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
        options->filename = strdup_up_to(line, ':');
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
        options->filename = strdup_up_to(line, ':');
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
        options_fail("invalid value for --line");
}

void options_parse_send(struct options* options, const char* send)
{
    char* key = NULL;
    char* type = NULL;

    if (NULL == strchr(send, ','))
        options_fail("--send value must be KEY,TYPE,VALUE");
    key = strdup_up_to(send, ',');
    send = strchr(send, ',') + 1;

    if (NULL == strchr(send, ','))
        options_fail("--send value must be KEY,TYPE,VALUE");
    type = strdup_up_to(send, ',');
    send = strchr(send, ',') + 1;

    options->send = (char*)realloc(options->send, strlen(options->send) + strlen(key) + 16 + strlen(send) + 16);
    sprintf(options->send + strlen(options->send), "%lu:%s", strlen(key), key);
    if (!strcmp(type, "string"))
        sprintf(options->send + strlen(options->send), "%lu:%s", strlen(send), send);
    else if (!strcmp(type, "integer"))
        sprintf(options->send + strlen(options->send), "i%de", atoi(send));
    else
        options_fail("--send TYPE must be 'string' or 'integer'");
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
        case 'v':
            options->verbose = true;
            break;
        case OPT_OP:
            free(options->op);
            options->op = strdup(optarg);
            break;
        case OPT_NO_PRINT:
            remove_print_option(&options->print, optarg);
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

        if (nrepl->options->verbose)
        {
            printf("<< ");
            bvalue_dump(reply, "<< ");
            printf("\n");
        }

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
            (void)write(print->fd, s->value.bsvalue.data, s->value.bsvalue.size);
            free_bvalue(s);
        }

        struct bvalue* ex = bvalue_dictionary_get(reply, "ex");
        if (ex)
            nrepl->exception_occurred = true;

        if (bvalue_has_status(reply, "done"))
            done = true;
        if (bvalue_has_status(reply, "error"))
            nrepl->exception_occurred = true;
        if (bvalue_has_status(reply, "namespace-not-found"))
        {
            static const char MESSAGE[] = "the namespace does not exist\n";
            (void)write(2, MESSAGE, strlen(MESSAGE));
        }

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

    if (nrepl->options->verbose)
    {
        printf(">> ");
        fwrite(message, 1, length, stdout);
        printf("\n");
    }

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
  --no-print=KEY                  Suppress output for KEY.\n\
  --op=OP                         nREPL operation (default: eval).\n\
  -p, --port=ADDRESS              TCP port, host:port, @portfile, or @FNAME@RELATIVE.\n\
  --print=KEY|KEY,FD,FORMAT       Print FORMAT to FD when KEY is present.\n\
  --send=KEY,TYPE,VALUE           Send additional KEY of VALUE in request.\n\
  -v, --verbose                   Show all messages sent and received.\n\
\n");
}

int main(int argc, char *argv[])
{
#if defined(_WIN32) || defined(WIN32)
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Error starting up windows sockets: %d", WSAGetLastError());
        return 1;
    }
#endif

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
