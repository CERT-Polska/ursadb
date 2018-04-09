#pragma once

#include <iostream>
#include <vector>
#include <fstream>

#include "core.h"

void yield_trigrams(std::ifstream &infile, long insize, std::vector<TriGram> &out);
