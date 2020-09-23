#include <alloca.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>

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

/* --- breader ------------------------------------------------------------ */

typedef void (* breader_callback_t) (void* cookie, const char* key, const char* bytevalue, size_t bytelength, int intvalue);

struct breader
{
    int fd;
    int peeked_char;
    _Bool want_dictionary_key;
    char* current_dictionary_key;
    void* cookie;
    breader_callback_t process_message_value;
};

void breader_read(struct breader* reader);

struct breader* make_breader(int fd, void* cookie, breader_callback_t process_message_value)
{
    struct breader* reader = (struct breader*)malloc(sizeof(struct breader));
    reader->fd = fd;
    reader->peeked_char = EOF;
    reader->want_dictionary_key = false;
    reader->current_dictionary_key = NULL;
    reader->cookie = cookie;
    reader->process_message_value = process_message_value;
    return reader;
}

void free_breader(struct breader* reader)
{
    if (reader->current_dictionary_key)
    {
        free(reader->current_dictionary_key);
        reader->current_dictionary_key = NULL;
    }
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

void bread_dictionary(struct breader* reader)
{
    bread_next_char(reader);
    while('e' != bread_peek_char(reader))
    {
        reader->want_dictionary_key = true;
        breader_read(reader);
        reader->want_dictionary_key = false;
        breader_read(reader);
        if (reader->current_dictionary_key)
        {
            free(reader->current_dictionary_key);
            reader->current_dictionary_key = NULL;
        }
    }
    bread_next_char(reader);
}

void bread_list(struct breader* reader)
{
    bread_next_char(reader);
    while('e' != bread_peek_char(reader))
        breader_read(reader);
    bread_next_char(reader);
}

void bread_integer(struct breader* reader)
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
    if (reader->current_dictionary_key)
        reader->process_message_value(reader->cookie, reader->current_dictionary_key, NULL, 0, value);
}

void bread_bytestring(struct breader* reader)
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
    
    if (reader->want_dictionary_key)
        reader->current_dictionary_key = bytes;
    else
    {
        if (reader->current_dictionary_key)
            reader->process_message_value(reader->cookie, reader->current_dictionary_key, bytes, length, 0);
        free(bytes);
    }
}

void breader_read(struct breader* reader)
{
    switch (bread_peek_char(reader))
    {
    case 'd':
        bread_dictionary(reader);
        break;
    case 'l':
        bread_list(reader);
        break;
    case 'i':
        bread_integer(reader);
        break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bread_bytestring(reader);
        break;
    case EOF:
        fail("Unexpected EOF");
    default:
        fail("bad character in nREPL stream");
    }
}

/* -- options ------------------------------------------------------------- */

struct options
{
    char* port;
    char* namespace;
    char* op;
    char* code;
    _Bool help;
};

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

struct sockaddr_in options_address(struct options* options, const char* port)
{
    if (*port == '@')
    {
        char linebuffer[256];
        if (!read_file(port + 1, linebuffer, sizeof(linebuffer)))
            error(port + 1);
        return options_address(options, linebuffer);
    }
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
        .sin_port = 0,
    };
    if (strchr(port, ':'))
    {
        char *host_part = alloca(strlen(port));
        strcpy(host_part, port);
        *strchr(host_part, ':') = '\0';

        address.sin_addr.s_addr = inet_addr(host_part);
        if (address.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *ent = gethostbyname(host_part);
            if (NULL == ent)
                error(host_part);
            if (NULL == ent->h_addr)
            {
                fprintf(stderr, "%s has no addresses\n", host_part);
                exit(1);
            }
            address.sin_addr = *(struct in_addr*)ent->h_addr;
        }

        port = strchr(port, ':') + 1;
    }
    else
    {
        address.sin_port = htons(atoi(port));
    }
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
};

const struct option LONG_OPTIONS[] =
{
    { "help",      0, NULL, 'h' },
    { "namespace", 1, NULL, 'n' },
    { "op",        1, NULL, OPT_OP },
    { "port",      1, NULL, 'p' },
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
    return options;
}

struct options* parse_options(int argc, char* argv[])
{
    struct options* options = new_options();
    int opt;
    while ((opt = getopt_long(argc, argv, "n:p:", LONG_OPTIONS, NULL)) != -1)
    {
        switch (opt)
        {
        case 'h':
            options->help = true;
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
        case '?':
            exit(1);
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
    free(options);
}

/* -- nrepl --------------------------------------------------------------- */

struct nrepl
{
    int fd;
    struct breader *decode;
    _Bool request_done;
    char* session;
};

void handle_message_key(struct nrepl* nrepl, const char* key, const char* bytes, size_t bytelength, int intvalue)
{
    if (bytes != NULL)
    {
        if (!strcmp(key, "status") && !strcmp(bytes, "done"))
            nrepl->request_done = true;
        else if (!strcmp(key, "new-session"))
        {
            if (nrepl->session)
                free(nrepl->session);
            nrepl->session = strdup(bytes);
        }
        else if (!strcmp(key, "out"))
            fwrite(bytes, 1, bytelength, stdout);
        else if (!strcmp(key, "value"))
        {
            fwrite(bytes, 1, bytelength, stdout);
            printf("\n");
        }
        else if (!strcmp(key, "err"))
            fwrite(bytes, 1, bytelength, stderr);
    }
}

struct nrepl* make_nrepl(void)
{
    struct nrepl* nrepl = (struct nrepl*)malloc(sizeof(struct nrepl));
    nrepl->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nrepl->fd == -1)
        error("socket");
    nrepl->decode = make_breader(nrepl->fd, nrepl, (breader_callback_t)handle_message_key);
    nrepl->request_done = false;
    nrepl->session = NULL;
    return nrepl;
}

void free_nrepl(struct nrepl* nrepl)
{
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
    nrepl->request_done = false;
    while (!nrepl->request_done)
        breader_read(nrepl->decode);
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

void nrepl_exec(struct options* options)
{
    struct nrepl* nrepl = make_nrepl();

    struct sockaddr_in address = options_address(options, options->port);
    if (-1 == connect(nrepl->fd, (struct sockaddr*)&address, sizeof(address)))
        error("connect");

    nrepl_send(nrepl, "d2:op5:clonee");

    nrepl_send(nrepl, "d2:op%lu:%s2:ns%lu:%s7:session%lu:%s4:code%lu:%se",
        strlen(options->op), options->op,
        strlen(options->namespace), options->namespace,
        strlen(nrepl->session), nrepl->session,
        strlen(options->code), options->code);

    nrepl_send(nrepl, "d2:op5:close7:session%lu:%se",
        strlen(nrepl->session), nrepl->session);

    free_nrepl(nrepl);
}

/* ------------------------------------------------------------------------ */

void help(void)
{
    printf("\
rep: Single-shot nREPL client\n\
Synopsis:\n\
  rep [OPTIONS] [--] [CODE ...]\n\
Options:\n\
  -h, --help          Show this help screen.\n\
  -n, --namespace=NS  Evaluate code in NS (default: user).\n\
  --op=OP             nREPL operation (default: eval).\n\
  -p, --port=ADDRESS  TCP port, host:port, @portfile, or @FNAME@RELATIVE.\n\
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
    nrepl_exec(options);
    free_options(options);
    exit(0);
}
