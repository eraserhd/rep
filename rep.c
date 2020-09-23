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
    struct sockaddr_in address;
    char* op;
    char* code;
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

void resolve_port_option(struct options* options, const char* port)
{
    if (*port == '@')
    {
        char linebuffer[256];
        if (!read_file(port + 1, linebuffer, sizeof(linebuffer)))
            error(port + 1);
        resolve_port_option(options, linebuffer);
        return;
    }
    if (strchr(port, ':'))
    {
        char *host_part = alloca(strlen(port));
        strcpy(host_part, port);
        *strchr(host_part, ':') = '\0';

        options->address.sin_addr.s_addr = inet_addr(host_part);
        if (options->address.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *ent = gethostbyname(host_part);
            if (NULL == ent)
                error(host_part);
            if (NULL == ent->h_addr)
            {
                fprintf(stderr, "%s has no addresses\n", host_part);
                exit(1);
            }
            options->address.sin_addr = *(struct in_addr*)ent->h_addr;
        }

        port = strchr(port, ':') + 1;
    }
    options->address.sin_port = htons(atoi(port));
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
    { "op",   1, NULL, OPT_OP },
    { "port", 1, NULL, 'p' },
    { NULL,   0, NULL, 0 }
};

struct options* new_options(void)
{
    struct options* options = (struct options*)malloc(sizeof(struct options));
    memset(&options->address, 0, sizeof(options->address));
    options->address.sin_family = AF_INET;
    options->address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    options->address.sin_port = 0;
    options->op = strdup("eval");
    options->code = NULL;
    return options;
}

struct options* parse_options(int argc, char* argv[])
{
    struct options* options = new_options();
    int opt;
    while ((opt = getopt_long(argc, argv, "p:", LONG_OPTIONS, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            resolve_port_option(options, optarg);
            break;

        case OPT_OP:
            free(options->op);
            options->op = strdup(optarg);
            break;

        case '?':
            exit(1);
        }
    }
    if (0 == options->address.sin_port)
        resolve_port_option(options, "@.nrepl-port");
    options->code = collect_code(argc, argv, optind);
    return options;
}

void free_options(struct options* options)
{
    free(options->op);
    if (options->code)
        free(options->code);
    free(options);
}

/* -- nrepl --------------------------------------------------------------- */

struct nrepl
{
    _Bool request_done;
    char* session;
};

struct nrepl* make_nrepl(void)
{
    struct nrepl* nrepl = (struct nrepl*)malloc(sizeof(struct nrepl));
    nrepl->request_done = false;
    nrepl->session = NULL;
    return nrepl;
}

void free_nrepl(struct nrepl* nrepl)
{
    if (nrepl->session)
        free(nrepl->session);
    free(nrepl);
}

void handle_message_key(struct nrepl* nrepl, const char* key, const char* bytes, size_t bytelength, int intvalue)
{
    if (bytes != NULL)
    {
        if (!strcmp(key, "status") && !strcmp(bytes, "done"))
            nrepl->request_done = true;
        if (!strcmp(key, "new-session"))
            nrepl->session = strdup(bytes);
        if (!strcmp(key, "out"))
            fwrite(bytes, 1, bytelength, stdout);
        if (!strcmp(key, "value"))
        {
            fwrite(bytes, 1, bytelength, stdout);
            printf("\n");
        }
        if (!strcmp(key, "err"))
            fwrite(bytes, 1, bytelength, stderr);
    }
}

void nrepl_exec(struct options* options)
{
    struct nrepl* nrepl = make_nrepl();

    int nrepl_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nrepl_sock == -1)
        error("socket");
    if (-1 == connect(nrepl_sock, (struct sockaddr*)&options->address, sizeof(options->address)))
        error("connect");

    const char CLONE_MESSAGE[] = "d2:op5:clonee";

    if (send(nrepl_sock, CLONE_MESSAGE, strlen(CLONE_MESSAGE), 0) != strlen(CLONE_MESSAGE))
        error("send");

    struct breader *decode = make_breader(nrepl_sock, nrepl, (breader_callback_t)handle_message_key);

    nrepl->request_done = false;
    while (!nrepl->request_done)
        breader_read(decode);

    char* eval_message = (char*)malloc(strlen(options->code) + 128);
    sprintf(eval_message, "d2:op%lu:%s7:session%lu:%s4:code%lu:%se",
        strlen(options->op), options->op,
        strlen(nrepl->session), nrepl->session,
        strlen(options->code), options->code);

    if (send(nrepl_sock, eval_message, strlen(eval_message), 0) != strlen(eval_message))
        error("send");

    free(eval_message);
    eval_message = NULL;

    nrepl->request_done = false;
    while (!nrepl->request_done)
        breader_read(decode);

    char close_message[128];
    snprintf(close_message, sizeof(close_message), "d2:op5:close7:session%lu:%se",
        strlen(nrepl->session), nrepl->session);

    if (send(nrepl_sock, close_message, strlen(close_message), 0) != strlen(close_message))
        error("send");

    nrepl->request_done = false;
    while (!nrepl->request_done)
        breader_read(decode);

    free_breader(decode);
    close(nrepl_sock);

    free_nrepl(nrepl);
}

/* ------------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    struct options* options = parse_options(argc, argv);
    nrepl_exec(options);
    free_options(options);
    exit(0);
}
