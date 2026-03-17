#include "hash_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>


// static void permutation_to_lehmer(const line_num *perm, size_t n, uint8_t *lehmer) {
//     bool used[40] = { false };
//     for (size_t i = 0; i < n; ++i) {
//         uint8_t count = 0;
//         for (size_t j = 0; j < perm[i]; ++j)
//             if (!used[j]) count++;
//         lehmer[i] = count;
//         used[perm[i]] = true;
//     }
// }

// static void pack_key(const line_num *key, size_t key_size, mpz_t packed) {
//     uint8_t lehmer[key_size];
//     permutation_to_lehmer(key, key_size, lehmer);
    
//     mpz_set_ui(packed, 0);
//     for (size_t i = 0; i < key_size; ++i) {
//         mpz_mul_ui(packed, packed, key_size - i);
//         mpz_add_ui(packed, packed, lehmer[i]);
//     }
// }

// size_t hash_function(const line_num *key, size_t key_size, size_t bucket_count) {
//     size_t hash = key_size;
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


// HashTable *hash_table_create(size_t bucket_count) {
//     HashTable *table = malloc(sizeof(HashTable));
//     table->buckets = calloc(bucket_count, sizeof(HashEntry *));
//     table->bucket_count = bucket_count;
//     return table;
// }

// bool hash_table_insert(HashTable *table, const line_num *key, size_t key_size) {
//     mpz_t packed_key;
//     mpz_init(packed_key);
//     pack_key(key, key_size, packed_key);
    
//     size_t hash = hash_function(key, key_size, table->bucket_count);
//     HashEntry *entry = table->buckets[hash];

//     while (entry != NULL) {
//         if (mpz_cmp(entry->packed_key, packed_key) == 0) {
//             mpz_clear(packed_key);
//             return false;
//         }
//         entry = entry->next;
//     }

//     entry = malloc(sizeof(HashEntry));
//     mpz_init(entry->packed_key);
//     mpz_set(entry->packed_key, packed_key);
// //    entry->key_size = key_size;
//     entry->next = table->buckets[hash];
//     table->buckets[hash] = entry;
    
//     mpz_clear(packed_key);
//     return true;
// }

// bool hash_table_contains(const HashTable *table, const line_num *key, size_t key_size) {
//     size_t hash = hash_function(key, key_size, table->bucket_count);
//     HashEntry *entry = table->buckets[hash];
//     if (entry == NULL) {
//         return false;
//     }

//     mpz_t packed_key;
//     mpz_init(packed_key);
//     pack_key(key, key_size, packed_key);

//     bool found = false;

//     while (entry != NULL) {
//         if (mpz_cmp(entry->packed_key, packed_key) == 0) {
//             found = true;
//             break;
//         }
//         entry = entry->next;
//     }
    
//     mpz_clear(packed_key);
//     return found;
// }

// void hash_table_destroy(HashTable *table) {
//     for (size_t i = 0; i < table->bucket_count; ++i) {
//         HashEntry *entry = table->buckets[i];
//         while (entry != NULL) {
//             HashEntry *next = entry->next;
//             mpz_clear(entry->packed_key);
//             free(entry);
//             entry = next;
//         }
//     }
//     free(table->buckets);
//     free(table);
// }


// void hash_table_print_stats(const HashTable *table) {
//     size_t total_elements = 0;
//     size_t non_empty_buckets = 0;
//     size_t max_collisions = 0;
//     size_t collision_counts[10] = {0}; // Для топ-10 коллизий

//     // Собираем статистику
//     for (size_t i = 0; i < table->bucket_count; ++i) {
//         HashEntry *entry = table->buckets[i];
//         size_t bucket_size = 0;

//         while (entry != NULL) {
//             bucket_size++;
//             entry = entry->next;
//         }

//         if (bucket_size > 0) {
//             non_empty_buckets++;
//             total_elements += bucket_size;

//             // Обновляем топ-10 коллизий
//             if (bucket_size > max_collisions) {
//                 max_collisions = bucket_size;
//             }

//             // Вставляем в топ-10 (простая сортировка вставкой)
//             if (bucket_size > collision_counts[9]) {
//                 collision_counts[9] = bucket_size;
//                 for (int j = 9; j > 0 && collision_counts[j] > collision_counts[j-1]; j--) {
//                     size_t temp = collision_counts[j];
//                     collision_counts[j] = collision_counts[j-1];
//                     collision_counts[j-1] = temp;
//                 }
//             }
//         }
//     }

//     // Печатаем статистику
//     printf("Hash Table Statistics:\n");
//     printf("----------------------\n");
//     printf("Total elements: %zu\n", total_elements);
//     printf("Bucket count: %zu\n", table->bucket_count);
//     printf("Load factor: %.2f%%\n", (float)non_empty_buckets / table->bucket_count * 100);
//     printf("Average elements per non-empty bucket: %.2f\n", (float)total_elements / non_empty_buckets);
//     printf("Max collisions in a single bucket: %zu\n", max_collisions);
    
//     printf("Top 10 collision counts:\n");
//     for (int i = 0; i < 10 && collision_counts[i] > 0; ++i) {
//         printf("%d. %zu\n", i+1, collision_counts[i]);
//     }
//     printf("----------------------\n");
// }


static void permutation_to_lehmer(const line_num *perm, size_t n, uint8_t *lehmer) {
    bool used[40] = { false };
    for (size_t i = 0; i < n; ++i) {
        uint8_t count = 0;
        for (size_t j = 0; j < perm[i]; ++j)
            if (!used[j]) count++;
        lehmer[i] = count;
        used[perm[i]] = true;
    }
}

static void pack_key(const line_num *key, size_t key_size, mpz_t packed) {
    uint8_t lehmer[key_size];
    permutation_to_lehmer(key, key_size, lehmer);
    
    mpz_set_ui(packed, 0);
    for (size_t i = 0; i < key_size; ++i) {
        mpz_mul_ui(packed, packed, key_size - i);
        mpz_add_ui(packed, packed, lehmer[i]);
    }
}

size_t hash_function(const line_num *key, size_t key_size, size_t bucket_count) {
    size_t hash = key_size;
    for (size_t i = 0; i < key_size; ++i) {
        hash = (hash << 5) ^ (hash >> 27) ^ key[i];
    }
    return hash % bucket_count;
}


HashTable *hash_table_create(size_t bucket_count) {
    HashTable *table = malloc(sizeof(HashTable));
    if (!table) return NULL;
    
    table->buckets = calloc(bucket_count, sizeof(HashBucket));
    if (!table->buckets) {
        free(table);
        return NULL;
    }
    
    table->bucket_count = bucket_count;
    
    // Инициализация всех бакетов
    for (size_t i = 0; i < bucket_count; ++i) {
        table->buckets[i].entries = NULL;
        table->buckets[i].count = 0;
    }
    
    return table;
}

bool hash_table_insert(HashTable *table, const line_num *key, size_t key_size) {
    if (!table) return false;

    mpz_t packed_key;
    mpz_init(packed_key);
    pack_key(key, key_size, packed_key);

    size_t bytes_needed = (mpz_sizeinbase(packed_key, 2) + 7) / 8;
    uint8_t *bytes = calloc(bytes_needed, sizeof(uint8_t));
    mpz_export(bytes, NULL, 1, 1, 0, 0, packed_key);  // Экспорт в big-endian
    mpz_clear(packed_key);
    
    size_t hash = hash_function(key, key_size, table->bucket_count);
    HashBucket *bucket = &table->buckets[hash];

    if (bucket->count >= 2) {
        free(bytes);
        return false;
    }

    // Проверка на существование ключа
    for (size_t i = 0; i < bucket->count; ++i) {
        if (bytes_needed == bucket->entries[i].count && memcmp(bucket->entries[i].bytes, bytes, bytes_needed * sizeof(uint8_t)) == 0) {
            free(bytes);
            return false;
        }
    }
    

    // Увеличение массива при необходимости
    size_t new_capacity = bucket->count + 1;
    HashEntry *new_entries = realloc(bucket->entries, new_capacity * sizeof(HashEntry));
    if (!new_entries) {
        free(bytes);
        return false;
    }
    bucket->entries = new_entries;
    

    // Добавление нового элемента
    bucket->entries[bucket->count].bytes = bytes;
    bucket->entries[bucket->count].count = bytes_needed;
    bucket->count++;
    
    return true;
}

bool hash_table_contains(const HashTable *table, const line_num *key, size_t key_size) {
    if (!table) return false;

    size_t hash = hash_function(key, key_size, table->bucket_count);
    const HashBucket *bucket = &table->buckets[hash];
    
    mpz_t packed_key;
    mpz_init(packed_key);
    pack_key(key, key_size, packed_key);

    size_t bytes_needed = (mpz_sizeinbase(packed_key, 2) + 7) / 8;
    uint8_t bytes[bytes_needed];
    mpz_export(bytes, NULL, 1, 1, 0, 0, packed_key);  // Экспорт в big-endian
    mpz_clear(packed_key);

    bool found = false;
    for (size_t i = 0; i < bucket->count; ++i) {
        if (bytes_needed == bucket->entries[i].count && memcmp(bucket->entries[i].bytes, bytes, bytes_needed * sizeof(uint8_t)) == 0) {
            found = true;
            break;
        }
    }
    
    return found;
}

void hash_table_destroy(HashTable *table) {
    if (!table) return;

    for (size_t i = 0; i < table->bucket_count; ++i) {
        HashBucket *bucket = &table->buckets[i];
        for (size_t j = 0; j < bucket->count; ++j) {
            free(bucket->entries[j].bytes);
        }
        free(bucket->entries);
    }
    free(table->buckets);
    free(table);
}

void hash_table_print_stats(const HashTable *table) {
    if (!table) return;

    size_t total_elements = 0;
    size_t non_empty_buckets = 0;
    size_t max_collisions = 0;
    size_t collision_counts[10] = {0};

    for (size_t i = 0; i < table->bucket_count; ++i) {
        const HashBucket *bucket = &table->buckets[i];
        size_t bucket_size = bucket->count;

        if (bucket_size > 0) {
            non_empty_buckets++;
            total_elements += bucket_size;

            if (bucket_size > max_collisions) {
                max_collisions = bucket_size;
            }

            if (bucket_size > collision_counts[9]) {
                collision_counts[9] = bucket_size;
                for (int j = 9; j > 0 && collision_counts[j] > collision_counts[j-1]; j--) {
                    size_t temp = collision_counts[j];
                    collision_counts[j] = collision_counts[j-1];
                    collision_counts[j-1] = temp;
                }
            }
        }
    }

    printf("Hash Table Statistics:\n");
    printf("----------------------\n");
    printf("Total elements: %zu\n", total_elements);
    printf("Bucket count: %zu\n", table->bucket_count);
    printf("Load factor: %.2f%%\n", (float)non_empty_buckets / table->bucket_count * 100);
    printf("Average elements per non-empty bucket: %.2f\n", (float)total_elements / non_empty_buckets);
    printf("Max collisions in a single bucket: %zu\n", max_collisions);
    
    printf("Top 10 collision counts:\n");
    for (int i = 0; i < 10 && collision_counts[i] > 0; ++i) {
        printf("%d. %zu\n", i+1, collision_counts[i]);
    }
    printf("----------------------\n");
}
