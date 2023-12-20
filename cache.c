#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include "cache.h"
#include "jbod.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  if (cache_size == 0&& num_entries <= 4096 && num_entries > 1 ) { // if cache doesn't exist and there are between 1 and 4096 entries, then
    cache = calloc(num_entries, sizeof(cache_entry_t)); // create cache return 1 for success
    cache_size = num_entries;
    return 1;
  }
  return -1; // failed to create cache
}

int cache_destroy(void) {
  if (cache_size > 0) { // if the cache exists
    free(cache); // free the space used by it
    cache = NULL;
    cache_size = 0;
    return 1;
  }
  return -1; // cache doesn't exist
}


int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if (cache_size > 0 && buf != NULL) { // if the cache and buffer exists
    num_queries += 1;
    for (int i = 0; i < cache_size; i += 1) { 
      if ((cache[i].disk_num == disk_num) && (cache[i].block_num == block_num) && (cache[i].valid == true)) { // if the cache is found
        num_hits += 1; // set clock and hits
        clock += 1;
        cache[i].clock_accesses = clock;
        memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
        return 1; // return success
      }
    }
  }
  return -1; // not found
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  int current = 0;
  while (current < cache_size && !(cache[current].valid && cache[current].disk_num == disk_num && cache[current].block_num == block_num)) {
    current += 1; // incrememnt till the cache black location is found
  }
  if (current < cache_size) { // if the location is valid
    clock += 1; // update
    cache[current].clock_accesses = clock;
    memcpy(cache[current].block, buf, JBOD_BLOCK_SIZE); 
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if (buf == NULL || cache_size == 0 || block_num < 0 || block_num >= 256 || disk_num < 0 || disk_num >= 16) { // if invalid then, return invalid
    return -1; 
  }
  int currIndex = -1;
  int currTime = -1;
  int reqIndex = -1;
  for (int i = 0; i < cache_size; i += 1) {
    if (cache[i].block_num == block_num && cache[i].disk_num == disk_num && cache[i].valid) { // if the entry already exists 
      return -1; // return error
    } else if (reqIndex == -1 && !cache[i].valid) { // if entry not found before and the slot's valid
      reqIndex = i; // set wanted index to current
    }
    if (cache[i].valid && currTime < cache[i].clock_accesses) { 
      currTime = cache[i].clock_accesses; // set recent time to current time
      currIndex = i; // set recent index to current index
    }
  }
  
  reqIndex = (reqIndex == -1) ? currIndex : reqIndex;

  // update info
  clock += 1;
  cache[reqIndex].valid = true;
  cache[reqIndex].disk_num = disk_num;
  cache[reqIndex].block_num = block_num;
  cache[reqIndex].clock_accesses = clock;
  
  memcpy(cache[reqIndex].block, buf, JBOD_BLOCK_SIZE); // copy

  return 1; // return success
}

bool cache_enabled(void) {
  return false;
}

int cache_resize(int new_num_entries) {
  if (new_num_entries <= 1 || new_num_entries > 4096) { // if 1 or less, or more than 4096, return invalid
    return -1;
  } else if (new_num_entries == cache_size) { // if the sizes are the same then return success
    return 1;
  }

  cache_entry_t *newCache = calloc(new_num_entries, sizeof(cache_entry_t));
  if (!newCache) {
    return -1; // if it fails, return invalid
  }
  else if (new_num_entries < cache_size) { // add the old data to the new cache
    for (int i = 0; i < new_num_entries; i += 1) {
      if (cache[i].valid) {
        newCache[i] = cache[i];
      }
    }
  } else {
    for (int i = 0; i < cache_size; i += 1) {
      if (cache[i].valid) {
        newCache[i] = cache[i];
      }
    }
  }
  
  free(cache); // free the old cache
  cache = newCache;
  cache_size = new_num_entries;
  clock = (cache_size == 0) ? 0 : clock;
  return 1; // return succcess
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float)num_hits / num_queries);
}