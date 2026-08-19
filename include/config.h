#ifndef CONFIG_H
#define CONFIG_H

int config_get(const char *filename,
               const char *key,
               char *value,
               int value_size);

int save_key(const char *filename,
             const unsigned char *key,
             size_t len);

int load_key(const char *filename,
             unsigned char *key,
             size_t len);

void print_hex(const unsigned char *key, size_t len);

#endif