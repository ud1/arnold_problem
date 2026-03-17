#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <gmp.h>

typedef uint8_t line_num;


typedef struct HashEntry {
    uint8_t *bytes;
    uint8_t count;
} HashEntry;

typedef struct HashBucket {
    HashEntry *entries;
    size_t count;
//    size_t capacity;
} HashBucket;

typedef struct HashTable {
    HashBucket *buckets;
    size_t bucket_count;
} HashTable;


// typedef struct HashEntry {
//     mpz_t packed_key;
// //    size_t key_size;
//     struct HashEntry *next;
// } HashEntry;

// typedef struct HashTable {
//     HashEntry **buckets;
//     size_t bucket_count;
// } HashTable;

// Функция для создания хеш-таблицы
HashTable *hash_table_create(size_t bucket_count);

// Функция для добавления массива элементов в хеш-таблицу
bool hash_table_insert(HashTable *table, const line_num *key, size_t key_size);

// Функция для проверки, содержится ли массив в хеш-таблице
bool hash_table_contains(const HashTable *table, const line_num *key, size_t key_size);

// Функция для уничтожения хеш-таблицы и освобождения памяти
void hash_table_destroy(HashTable *table);

void hash_table_print_stats(const HashTable *table);

#endif // HASH_TABLE_H


// size_t hash_function_(const line_num *key, size_t key_size, size_t bucket_count) {
//     size_t hash = 0;
//     for (size_t i = 0; i < key_size; ++i) {
//         hash = hash * 31 + key[i];
//     }
//     return hash % bucket_count;
// }

// size_t hash_function(const line_num *key, size_t key_size, size_t bucket_count) {
//     uint32_t hash = key_size;
// //    printf("key_size = %d\n", key_size);
// //    printf("%d\n", hash);
//     for (size_t i = 0; i < key_size; ++i) {
// //        printf("%d\n", hash);
//         // hash = (hash << 5) ^ (hash >> 8) ^ (hash >> 26) ^ key[i];
//         hash = (hash << 5) ^ (hash >> 27) ^ key[i];
//     }
//     // printf("hash = %d\n", hash);
//     return hash % bucket_count;
// }