#include "linux_net.h"
#include <string.h>
#include "config.h"

// Get the value of a key from a configuration file
int config_get(const char *filename,
               const char *key,
               char *value,
               int value_size)
{
    FILE *file = fopen(filename, "r");

    if (file == NULL)
        return -1;

    char line[256];

    while (fgets(line, sizeof(line), file)) {

        char *equals = strchr(line, '=');

        if (equals == NULL)
            continue;

        *equals = '\0';

        char *config_key = line;
        char *config_value = equals + 1;

        config_value[strcspn(config_value, "\r\n")] = '\0';

        while (*config_key == ' ' || *config_key == '\t')
            config_key++;

        while (*config_value == ' ' || *config_value == '\t')
            config_value++;

        char *end = config_key + strlen(config_key) - 1;

        while (end >= config_key &&
               (*end == ' ' || *end == '\t')) {
            *end = '\0';
            end--;
        }

        if (strcmp(config_key, key) == 0) {

            if ((int)strlen(config_value) >= value_size) {
                fclose(file);
                return -1;
            }

            strcpy(value, config_value);

            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return -1;
}


// Print a key in hexadecimal format
void print_hex(const unsigned char *key, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x", key[i]);

    printf("\n");
}


// Save a binary key as hexadecimal
int save_key(const char *filename,
             const unsigned char *key,
             size_t len)
{
    FILE *file = fopen(filename, "w");

    if (file == NULL) {
        perror(filename);
        return -1;
    }

    for (size_t i = 0; i < len; i++)
        fprintf(file, "%02x", key[i]);

    fprintf(file, "\n");

    fclose(file);
    return 0;
}


// Load a hexadecimal key and convert it to binary
int load_key(const char *filename,
             unsigned char *key,
             size_t len)
{
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror(filename);
        return -1;
    }

    char hex[256];

    if (fgets(hex, sizeof(hex), file) == NULL) {
        fclose(file);
        return -1;
    }

    hex[strcspn(hex, "\r\n")] = '\0';

    if (strlen(hex) != len * 2) {
        fclose(file);
        return -1;
    }

    for (size_t i = 0; i < len; i++) {
        if (sscanf(hex + (i * 2), "%2hhx", &key[i]) != 1) {
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return 0;
}