#pragma once

#include <iostream>
#include <vector>
#include <fstream>

#include "core.h"

std::vector<TriGram> get_trigrams(std::ifstream &infile, long insize);
