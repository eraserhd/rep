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

struct breader
{
    int fd;
    int peeked_char;
    _Bool want_dictionary_key;
    char* current_dictionary_key;
    void (* process_message_value) (const char* key, const char* bytevalue, size_t bytelength, int intvalue);
};

void bread(struct breader* decoder);

struct breader* make_breader(int fd, void (* process_message_value) (const char*, const char*, size_t, int))
{
    struct breader* decoder = (struct breader*)malloc(sizeof(struct breader));
    decoder->fd = fd;
    decoder->peeked_char = EOF;
    decoder->want_dictionary_key = false;
    decoder->current_dictionary_key = NULL;
    decoder->process_message_value = process_message_value;
    return decoder;
}

int bread_next_char(struct breader* decoder)
{
    if (EOF != decoder->peeked_char)
    {
        int result = decoder->peeked_char;
        decoder->peeked_char = EOF;
        return result;
    }
    char ch = 0;
    int count = recv(decoder->fd, &ch, 1, 0);
    if (count < 0)
        error("recv");
    if (0 == count)
        return EOF;
    return ch;
}

int bread_peek_char(struct breader* decoder)
{
    if (EOF == decoder->peeked_char)
        decoder->peeked_char = bread_next_char(decoder);
    return decoder->peeked_char;
}

void bread_dictionary(struct breader* decoder)
{
    bread_next_char(decoder);
    while('e' != bread_peek_char(decoder))
    {
        decoder->want_dictionary_key = true;
        bread(decoder);
        decoder->want_dictionary_key = false;
        bread(decoder);
        if (decoder->current_dictionary_key)
        {
            free(decoder->current_dictionary_key);
            decoder->current_dictionary_key = NULL;
        }
    }
    bread_next_char(decoder);
}

void bread_list(struct breader* decoder)
{
    bread_next_char(decoder);
    while('e' != bread_peek_char(decoder))
        bread(decoder);
    bread_next_char(decoder);
}

void bread_integer(struct breader* decoder)
{
    int value = 0;
    _Bool negative = false;
    bread_next_char(decoder);
    while('e' != bread_peek_char(decoder))
    {
        int ch = bread_next_char(decoder);
        if (ch == '-')
            negative = true;
        else
            value = value * 10 + (ch - '0');
    }
    bread_next_char(decoder);
    if (negative)
        value = -value;
    if (decoder->current_dictionary_key)
        decoder->process_message_value(decoder->current_dictionary_key, NULL, 0, value);
}

void bread_bytestring(struct breader* decoder)
{
    size_t length = 0;
    char *bytes = NULL;
    while (':' != bread_peek_char(decoder))
        length = length*10 + (bread_next_char(decoder) - '0');
    bread_next_char(decoder);
    
    bytes = (char*)malloc(length + 1);
    if (NULL == bytes)
        error("malloc");
    for (size_t i = 0; i < length; i++)
        bytes[i] = bread_next_char(decoder);
    bytes[length] = '\0';
    
    if (decoder->want_dictionary_key)
        decoder->current_dictionary_key = bytes;
    else
    {
        if (decoder->current_dictionary_key)
            decoder->process_message_value(decoder->current_dictionary_key, bytes, length, 0);
        free(bytes);
    }
}

void bread(struct breader* decoder)
{
    switch (bread_peek_char(decoder))
    {
    case 'd':
        bread_dictionary(decoder);
        break;
    case 'l':
        bread_list(decoder);
        break;
    case 'i':
        bread_integer(decoder);
        break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        bread_bytestring(decoder);
        break;
    case EOF:
        fail("Unexpected EOF");
    default:
        fail("bad character in nREPL stream");
    }
}

/* ------------------------------------------------------------------------ */

struct sockaddr_in opt_port =
{
    .sin_family = AF_INET,
    .sin_addr = htonl(INADDR_LOOPBACK),
    .sin_port = 0,
};

int nrepl_sock = -1;

void handle_message_key(const char* key, const char* bytes, size_t bytelength, int intvalue)
{
    if (bytes)
        printf("%s = %s\n", key, bytes);
    else
        printf("%s = %d\n", key, intvalue);
}

void nrepl_exec(const char* code)
{
    printf("::0\n");
    fflush(stdout);

    nrepl_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (nrepl_sock == -1)
    {
        perror("socket");
        exit(1);
    }

    printf("::00 %s %u\n", inet_ntoa(opt_port.sin_addr), ntohs(opt_port.sin_port));
    fflush(stdout);

    if (-1 == connect(nrepl_sock, (struct sockaddr*)&opt_port, sizeof(opt_port)))
    {
        perror("connect");
        exit(1);
    }

    printf("::1\n");
    fflush(stdout);

    char message[512];
    //snprintf(message, sizeof(message), "d2:op4:eval4:code%lu:%se\n", strlen(code), code);
    snprintf(message, sizeof(message), "d2:op5:clonee");


    printf("::2 %s\n", message);
    fflush(stdout);

    if (send(nrepl_sock, message, strlen(message), 0) != strlen(message))
    {
        perror("send");
        exit(1);
    }

    struct breader *decode = make_breader(nrepl_sock, handle_message_key);
    for (;;)
    {
        bread(decode);
    }

    close(nrepl_sock);
}


char *read_file(const char* filename, char* buffer, size_t buffer_size)
{
    FILE *portfile = fopen(filename, "r");
    if (NULL == portfile)
    {
        fclose(portfile);
        return NULL;
    }
    if (NULL == fgets(buffer, buffer_size, portfile))
    {
        fclose(portfile);
        return NULL;
    }
    fclose(portfile);
    return buffer;
}

void resolve_port_option(const char* port)
{
    if (*port == '@')
    {
        char linebuffer[256];
        if (!read_file(port + 1, linebuffer, sizeof(linebuffer)))
        {
            perror(port + 1);
            exit(1);
        }
        resolve_port_option(linebuffer);
        return;
    }
    if (strchr(port, ':'))
    {
        char *host_part = alloca(strlen(port));
        strcpy(host_part, port);
        *strchr(host_part, ':') = '\0';

        opt_port.sin_addr.s_addr = inet_addr(host_part);
        if (opt_port.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *ent = gethostbyname(host_part);
            if (NULL == ent)
            {
                perror(host_part);
                exit(1);
            }
            if (NULL == ent->h_addr)
            {
                fprintf(stderr, "%s has no addresses\n", host_part);
                exit(1);
            }
            opt_port.sin_addr = *(struct in_addr*)ent->h_addr;
        }

        port = strchr(port, ':') + 1;
    }
    opt_port.sin_port = htons(atoi(port));
}

const struct option LONG_OPTIONS[] =
{
    { "port", 1, NULL, 'p' },
    { NULL,   0, NULL, 0 }
};

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

int main(int argc, char *argv[])
{
    int opt;
    _Bool has_port = false;
    while ((opt = getopt_long(argc, argv, "p:", LONG_OPTIONS, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            resolve_port_option(optarg);
            has_port = true;
            break;

        case '?':
            exit(1);
        }
    }

    if (!has_port)
        resolve_port_option("@.nrepl-port");

    char* code = collect_code(argc, argv, optind);
    nrepl_exec(code);
    free(code);
    exit(0);
}
