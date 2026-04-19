// index.c — Staging area implementation

#include "index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// ─── PROVIDED ────────────────────────────────────────────────────────────────

IndexEntry* index_find(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0)
            return &index->entries[i];
    }
    return NULL;
}

int index_remove(Index *index, const char *path) {
    for (int i = 0; i < index->count; i++) {
        if (strcmp(index->entries[i].path, path) == 0) {
            int remaining = index->count - i - 1;

            if (remaining > 0) {
                memmove(&index->entries[i],
                        &index->entries[i + 1],
                        remaining * sizeof(IndexEntry));
            }

            index->count--;
            return index_save(index);
        }
    }

    fprintf(stderr, "error: '%s' is not in the index\n", path);
    return -1;
}

int index_status(const Index *index) {
    printf("Staged changes:\n");

    int staged_count = 0;

    for (int i = 0; i < index->count; i++) {
        printf("  staged:     %s\n", index->entries[i].path);
        staged_count++;
    }

    if (staged_count == 0)
        printf("  (nothing to show)\n");

    printf("\nUnstaged changes:\n");

    int unstaged_count = 0;

    for (int i = 0; i < index->count; i++) {
        struct stat st;

        if (stat(index->entries[i].path, &st) != 0) {
            printf("  deleted:    %s\n", index->entries[i].path);
            unstaged_count++;
        } else {
            if (st.st_mtime != (time_t)index->entries[i].mtime_sec ||
                st.st_size != (off_t)index->entries[i].size) {
                printf("  modified:   %s\n", index->entries[i].path);
                unstaged_count++;
            }
        }
    }

    if (unstaged_count == 0)
        printf("  (nothing to show)\n");

    printf("\nUntracked files:\n");

    int untracked_count = 0;
    DIR *dir = opendir(".");

    if (dir) {
        struct dirent *ent;

        while ((ent = readdir(dir)) != NULL) {
            if (!strcmp(ent->d_name, ".") ||
                !strcmp(ent->d_name, "..") ||
                !strcmp(ent->d_name, ".pes") ||
                !strcmp(ent->d_name, "pes"))
                continue;

            if (strstr(ent->d_name, ".o"))
                continue;

            int tracked = 0;

            for (int i = 0; i < index->count; i++) {
                if (!strcmp(index->entries[i].path, ent->d_name)) {
                    tracked = 1;
                    break;
                }
            }

            if (!tracked) {
                struct stat st;
                stat(ent->d_name, &st);

                if (S_ISREG(st.st_mode)) {
                    printf("  untracked:  %s\n", ent->d_name);
                    untracked_count++;
                }
            }
        }

        closedir(dir);
    }

    if (untracked_count == 0)
        printf("  (nothing to show)\n");

    printf("\n");
    return 0;
}

// ─── TODO FUNCTIONS IMPLEMENTED ──────────────────────────────────────────────

int index_load(Index *index) {
    index->count = 0;

    FILE *f = fopen(INDEX_FILE, "r");

    if (!f)
        return 0;

    while (index->count < MAX_INDEX_ENTRIES) {
        IndexEntry *e = &index->entries[index->count];
        char hex[HASH_HEX_SIZE + 1];

        int r = fscanf(f, "%o %64s %lu %u %511s",
                       &e->mode,
                       hex,
                       &e->mtime_sec,
                       &e->size,
                       e->path);

        if (r != 5)
            break;

        hex_to_hash(hex, &e->hash);
        index->count++;
    }

    fclose(f);
    return 0;
}

int index_save(const Index *index) {
    FILE *f = fopen(".pes/index.tmp", "w");

    if (!f)
        return -1;

    for (int i = 0; i < index->count; i++) {
        char hex[HASH_HEX_SIZE + 1];

        hash_to_hex(&index->entries[i].hash, hex);

        fprintf(f, "%o %s %lu %u %s\n",
                index->entries[i].mode,
                hex,
                index->entries[i].mtime_sec,
                index->entries[i].size,
                index->entries[i].path);
    }

    fclose(f);
    rename(".pes/index.tmp", INDEX_FILE);

    return 0;
}

int index_add(Index *index, const char *path) {
    struct stat st;

    if (stat(path, &st) != 0)
        return -1;

    FILE *f = fopen(path, "rb");

    if (!f)
        return -1;

    char *data = malloc(st.st_size);

    fread(data, 1, st.st_size, f);
    fclose(f);

    ObjectID id;
    object_write(OBJ_BLOB, data, st.st_size, &id);

    free(data);

    IndexEntry *e = index_find(index, path);

    if (!e)
        e = &index->entries[index->count++];

    strcpy(e->path, path);
    e->hash = id;
    e->mode = 100644;
    e->mtime_sec = st.st_mtime;
    e->size = st.st_size;

    return index_save(index);
}
