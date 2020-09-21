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

struct sockaddr_in opt_port =
{
    .sin_addr = INADDR_LOOPBACK,
    .sin_port = 0,
};

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
        }
    }

    if (!has_port)
        resolve_port_option("@.nrepl-port");

    size_t code_size = 0;
    for (int i = optind; i < argc; ++i)
        code_size += strlen(argv[i]) + 1;
    char *code = (char *)alloca(code_size);
    code[0] = '\0';
    for (int i = optind; i < argc; ++i)
    {
        if (code[0])
            strcat(code, " ");
        strcat(code, argv[i]);
    }

    printf("code::%s::\n", code);
    exit(0);
}
