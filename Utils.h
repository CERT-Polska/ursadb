#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "Core.h"

using TrigramGenerator = std::vector<TriGram> (*)(const uint8_t *mem,
                                                  size_t size);

TrigramGenerator get_generator_for(IndexType type);
std::vector<TriGram> get_trigrams(const uint8_t *mem, size_t size);
std::vector<TriGram> get_b64grams(const uint8_t *mem, size_t size);
void compress_run(const std::vector<FileId> &run, std::ostream &out);
std::vector<FileId> read_compressed_run(const uint8_t *start,
                                        const uint8_t *end);
std::string get_index_type_name(IndexType type);