#include "linux_net.h"
#include <sodium.h>

void print_hex(const unsigned char *key, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x", key[i]);

    printf("\n");
}

int save_key(const char *filename,
             const unsigned char *key,
             size_t len)
{
    FILE *file = fopen(filename, "w");

    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    for (size_t i = 0; i < len; i++)
        fprintf(file, "%02x", key[i]);

    fprintf(file, "\n");

    fclose(file);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s <client|server>\n", argv[0]);
        return 1;
    }

    if (sodium_init() < 0)
        return 1;

    unsigned char pk[crypto_kx_PUBLICKEYBYTES];
    unsigned char sk[crypto_kx_SECRETKEYBYTES];

    crypto_kx_keypair(pk, sk);

    char public_file[256];
    char private_file[256];

    snprintf(public_file, sizeof(public_file),
             "config/%s_public.key", argv[1]);

    snprintf(private_file, sizeof(private_file),
             "config/%s_private.key", argv[1]);

    if (save_key(public_file, pk, sizeof(pk)) < 0)
        return 1;

    if (save_key(private_file, sk, sizeof(sk)) < 0)
        return 1;

    printf("%s key pair generated.\n", argv[1]);

    printf("Public key: ");
    print_hex(pk, sizeof(pk));

    return 0;
}