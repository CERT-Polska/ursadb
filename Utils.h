#pragma once

#include <vector>
#include <fstream>
#include <string>

#include "Core.h"

std::vector<TriGram> get_trigrams(const uint8_t *mem, size_t size);
void compress_run(const std::vector<FileId> &run, std::ostream &out);
std::vector<FileId> read_compressed_run(const uint8_t *start, const uint8_t *end);
std::string get_index_type_name(IndexType type);