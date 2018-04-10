#pragma once

#include <iostream>
#include <vector>
#include <fstream>

#include "Core.h"

std::vector<TriGram> get_trigrams(std::ifstream &infile, long insize);
