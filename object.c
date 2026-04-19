#include "pes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <openssl/sha.h>

void hash_to_hex(const ObjectID *id, char *hex_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        sprintf(hex_out + (i * 2), "%02x", id->hash[i]);
    }
    hex_out[HASH_HEX_SIZE] = '\0';
}

int hex_to_hash(const char *hex, ObjectID *id_out) {
    for (int i = 0; i < HASH_SIZE; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1)
            return -1;
        id_out->hash[i] = (uint8_t)byte;
    }
    return 0;
}

void compute_hash(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    SHA256_CTX ctx;

    SHA256_Init(&ctx);
    SHA256_Update(&ctx, &type, sizeof(type));
    SHA256_Update(&ctx, data, len);
    SHA256_Final(id_out->hash, &ctx);
}

void object_path(const ObjectID *id, char *path_out, size_t path_size) {
    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(id, hex);
    snprintf(path_out, path_size, ".pes/objects/%s", hex);
}

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out) {
    mkdir(".pes", 0777);
    mkdir(".pes/objects", 0777);

    compute_hash(type, data, len, id_out);

    char path[512];
    object_path(id_out, path, sizeof(path));

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fwrite(&type, sizeof(type), 1, f);
    fwrite(&len, sizeof(len), 1, f);
    fwrite(data, 1, len, f);

    fclose(f);
    return 0;
}

int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out) {
    char path[512];
    object_path(id, path, sizeof(path));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fread(type_out, sizeof(*type_out), 1, f);
    fread(len_out, sizeof(*len_out), 1, f);

    *data_out = malloc(*len_out);
    if (!*data_out) {
        fclose(f);
        return -1;
    }

    fread(*data_out, 1, *len_out, f);
    fclose(f);

    ObjectID check;
    compute_hash(*type_out, *data_out, *len_out, &check);

    if (memcmp(check.hash, id->hash, HASH_SIZE) != 0) {
        free(*data_out);
        return -1;
    }

    return 0;
}

int object_exists(const ObjectID *id) {
    char path[512];
    object_path(id, path, sizeof(path));
    return access(path, F_OK) == 0;
}
