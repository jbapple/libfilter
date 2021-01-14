#pragma once

#include <stdint.h>

static const uint64_t libfilter_hash_entropy_bytes =
    sizeof(uint64_t) * (9 * 3 + (8 - 1) * 3 * 10 + 2 * 8 * 3 * 10 + 2 * 7 * 3 + 3 - 1);

static const uint64_t libfilter_hash_tabulate_entropy_bytes =
    sizeof(uint64_t) * (9 * 3 + (8 - 1) * 3 * 10 + 2 * 8 * 3 * 10 + 2 * 7 * 3 + 3 - 1) +
    sizeof(uint64_t) * 4 * sizeof(uint64_t) * 256;

void libfilter_hash_au(
    const uint64_t entropy[libfilter_hash_entropy_bytes / sizeof(uint64_t)],
    const char* char_input, size_t length, uint64_t output[3]);

uint64_t libfilter_hash_tabulate(
    const uint64_t entropy[libfilter_hash_tabulate_entropy_bytes / sizeof(uint64_t)],
    const char* char_input, size_t length);
