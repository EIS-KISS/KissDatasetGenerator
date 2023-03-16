#pragma once
#include <stdint.h>

uint64_t murmurHash64(const void* key,int len, uint64_t seed);
