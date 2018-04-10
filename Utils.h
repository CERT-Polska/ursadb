#pragma once

#include <iostream>
#include <vector>
#include <fstream>

#include "Core.h"
#include "MemMap.h"

std::vector<TriGram> get_trigrams(const MemMap &infile);
