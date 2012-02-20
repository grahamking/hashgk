
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "lookup3.c"

const int DEFAULT_INITIAL_SIZE = 64;

/*
 * UTIL
 */

/* Hash of 'key'. Uses external lib 'lookup3.c'. */
uint32_t get_hash(char *key) {
    return hashlittle(key, strlen(key), 42);
}

/* Number of significant bits in 'value' */
int bitcount(uint32_t value) {

    int bits = 0;

    if (value == 1) {
        return 1;
    }

    while (value > (int) pow(2, bits)) {
        bits++;
    }

    return bits;
}

/*
 * ITEM
 */

struct Item {
    char *key;
    char *value;
};

struct Item *item_new(char *key, char *value) {
    struct Item *item = malloc(sizeof(struct Item));
    item->key = strdup(key);
    item->value = strdup(value);
    return item;
}

void item_del(struct Item *item) {
    free(item->key);
    free(item->value);
    free(item);
}

/*
 * NODE
 */

struct Node {
    int extra_len;
    int extra_cap;
    struct Item *first;
    struct Item **extra;
};

struct Node *node_new(struct Item *item) {
    struct Node *node = malloc(sizeof(struct Node));
    node->first = item;
    node->extra_len = 0;
    node->extra_cap = 0;
    return node;
}

void node_add(struct Node *node, struct Item *item) {
    if (node->extra == NULL) {
        node->extra = (struct Item **) malloc(sizeof(struct Item *));
        node->extra[0] = item;
        node->extra_len = 1;
        node->extra_cap = 1;
    }
    else {
        node->extra_len++;
        node->extra_cap++;
        node->extra = realloc(node->extra, sizeof(struct Item *) * node->extra_len);
        node->extra[node->extra_len - 1] = item;
    }
}

/* Get value stored at 'key' in 'node' */
char *node_get_value(struct Node *node, char *key) {
    int i;
    struct Item *item;

    assert(node != NULL);
    assert(key != NULL);

    if (node->extra_len == 0 || strcmp(node->first->key, key) == 0) {
        return node->first->value;
    }
    else {
        // Search extra
        for (i = 0; i < node->extra_len; i++) {
            item = node->extra[i];
            if (strcmp(item->key, key) == 0) {
                return item->value;
            }
        }
    }
    return NULL;
}

/* Number of collisions in this node */
int node_collision_count(struct Node *node) {
    return node->extra_len;
}

/*
 * DICT
 */

struct Dict {
    int size;
    int size_bits;
    struct Node **store;
};

/* Find position in the map of key.
 * key: String to hash.
 * size_bits: Number of bits of the hash we use.
 */
int find_pos(char *key, int size_bits) {
    uint32_t hash = get_hash(key);
    int pos = (hash & hashmask(size_bits));
    return pos;
}

/* Create a new map */
struct Dict *dict_new(int initial_size) {
    struct Node **store = (struct Node **) malloc(sizeof(struct Node) * initial_size);
    struct Dict *dict = malloc(sizeof(struct Dict));
    dict->store = store;
    dict->size = initial_size;
    dict->size_bits = bitcount(initial_size);
    return dict;
}

/* Add 'item' to 'store' at 'pos', creating a node if necessary */
void dict_add_to_pos(struct Node **store, int pos, struct Item *item) {

    struct Node *node = store[pos];
    if (node == NULL) {
        node = node_new(item);
        store[pos] = node;
    }
    else {
        node_add(node, item);
    }
}


/* Grow the dictionary, re-hashing everything.
 * Do this when we have too many collisions.
 */
void dict_grow(struct Dict *dict) {
    int new_size = dict->size * 2;
    int new_size_bits = bitcount(new_size);
    struct Node **new_store = (struct Node **) malloc(sizeof(struct Node) * new_size);

    struct Node *node;
    int index;
    int extra_index;
    int new_pos;

    for (index = 0; index < dict->size; index++) {
        node = dict->store[index];
        if (node == NULL || node->first == NULL) {
            continue;
        }

        new_pos = find_pos(node->first->key, new_size_bits);
        dict_add_to_pos(new_store, new_pos, node->first);

        for (extra_index = 0; extra_index < node->extra_len; extra_index++) {
            new_pos = find_pos(node->extra[extra_index]->key, new_size_bits);
            dict_add_to_pos(new_store, new_pos, node->extra[extra_index]);
        }
        free(node);
    }

    free(dict->store);
    dict->store = new_store;
    dict->size = new_size;
    dict->size_bits = new_size_bits;
}

/* Put 'value' in map at 'key' */
void dict_set(struct Dict *dict, char *key, char *value) {
    struct Item *item = item_new(key, value);
    int pos = find_pos(key, dict->size_bits);
    dict_add_to_pos(dict->store, pos, item);
}

/* Get value from the map */
char *dict_get(struct Dict *dict, char *key) {
    int pos = find_pos(key, dict->size_bits);
    struct Node *node = dict->store[pos];
    return node_get_value(node, key);
}

/* Delete the map */
void dict_del(struct Dict *dict) {
    free(dict->store);
    free(dict);
}


/* Number of collisions in current map */
int dict_collision_count(struct Dict *dict) {
    int count = 0;
    int i = 0;

    for (i = 0; i < dict->size; i++) {
        if (dict->store[i] == NULL) {
            continue;
        }
        count += node_collision_count(dict->store[i]);
    }

    return count;
}

///////////////////////
// TEST
//////////////////////

/* Split passwd line. Fills username and fullname. */
const int NAME_POS = 4;
void split_line(char *line, char **out_username, char **out_fullname) {
    char *next_tok = strtok(line, ":,");
    char *username;
    char *fullname;

    int pos = 0;
    while (next_tok != NULL && pos <= NAME_POS) {
        if (pos == 0) {
            username = strdup(next_tok);
        }
        else if (pos == NAME_POS) {
            fullname = strdup(next_tok);
        }
        next_tok = strtok(NULL, ":,");
        pos++;
    }
    *out_username = username;
    *out_fullname = fullname;
}

/* Fill dict with passwd file */
void fill_dict(FILE *file, struct Dict *dict) {

    ssize_t num_read;
    char *lineptr = NULL;
    size_t to_read = 0;
    char *username;
    char *fullname;

    num_read = getline(&lineptr, &to_read, file);
    while (num_read != -1) {
        split_line(lineptr, &username, &fullname);
        dict_set(dict, username, fullname);

        num_read = getline(&lineptr, &to_read, file);
    }

}

/*
 * MAIN
 */
int main(int argc, char *argv[]) {

    struct Dict *dict;
    int initial_size = DEFAULT_INITIAL_SIZE;

    if (argc == 2) {
        initial_size = atoi(argv[1]);
    }

    dict = dict_new(initial_size);
    FILE *passwd = fopen("/etc/passwd", "rt");
    fill_dict(passwd, dict);

    printf("Size: %d, Key bits: %d, Collisions: %d\n",
            dict->size,
            dict->size_bits,
            dict_collision_count(dict));

    dict_grow(dict);

    printf("Size: %d, Key bits: %d, Collisions: %d\n",
            dict->size,
            dict->size_bits,
            dict_collision_count(dict));

    dict_grow(dict);

    printf("Size: %d, Key bits: %d, Collisions: %d\n",
            dict->size,
            dict->size_bits,
            dict_collision_count(dict));

    printf("graham: %s\n", dict_get(dict, "graham"));
    printf("postgres: %s\n", dict_get(dict, "postgres"));
    printf("mysql: %s\n", dict_get(dict, "mysql"));
    printf("redis: %s\n", dict_get(dict, "redis"));
    printf("mongodb: %s\n", dict_get(dict, "mongodb"));
    printf("root: %s\n", dict_get(dict, "root"));
    printf("backup: %s\n", dict_get(dict, "backup"));
    printf("daemon: %s\n", dict_get(dict, "daemon"));
    printf("news: %s\n", dict_get(dict, "news"));
    printf("uucp: %s\n", dict_get(dict, "uucp"));

    dict_del(dict);
    return 0;
}
