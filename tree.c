#include "tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);

#define MODE_FILE 0100644
#define MODE_EXEC 0100755
#define MODE_DIR 0040000

uint32_t get_file_mode(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;

    if (S_ISDIR(st.st_mode)) return MODE_DIR;
    if (st.st_mode & S_IXUSR) return MODE_EXEC;
    return MODE_FILE;
}

static int cmp(const void *a, const void *b) {
    return strcmp(((TreeEntry *)a)->name, ((TreeEntry *)b)->name);
}

int tree_parse(const void *data, size_t len, Tree *tree_out) {
    tree_out->count = 0;

    const unsigned char *p = data;
    const unsigned char *end = p + len;

    while (p < end && tree_out->count < MAX_TREE_ENTRIES) {
        TreeEntry *e = &tree_out->entries[tree_out->count];

        char modebuf[16];
        int mi = 0;

        while (p < end && *p != ' ')
            modebuf[mi++] = *p++;

        modebuf[mi] = '\0';
        p++;

        e->mode = strtol(modebuf, NULL, 8);

        int ni = 0;
        while (p < end && *p != '\0')
            e->name[ni++] = *p++;

        e->name[ni] = '\0';
        p++;

        memcpy(e->hash.hash, p, HASH_SIZE);
        p += HASH_SIZE;

        tree_out->count++;
    }

    return 0;
}

int tree_serialize(const Tree *tree, void **data_out, size_t *len_out) {
    Tree temp = *tree;

    qsort(temp.entries, temp.count, sizeof(TreeEntry), cmp);

    size_t max = temp.count * 400;
    unsigned char *buf = malloc(max);

    size_t off = 0;

    for (int i = 0; i < temp.count; i++) {
        TreeEntry *e = &temp.entries[i];

        off += sprintf((char *)buf + off, "%o %s", e->mode, e->name);
        buf[off++] = '\0';

        memcpy(buf + off, e->hash.hash, HASH_SIZE);
        off += HASH_SIZE;
    }

    *data_out = buf;
    *len_out = off;

    return 0;
}

int tree_from_index(ObjectID *id_out) {
    Tree tree;
    tree.count = 0;

    void *raw;
    size_t len;

    tree_serialize(&tree, &raw, &len);
    int rc = object_write(OBJ_TREE, raw, len, id_out);

    free(raw);
    return rc;
}
