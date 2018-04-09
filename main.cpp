#include <iostream>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <array>
#include <list>


#include "dataset.h"
#include "dataset_builder.h"


class Database {
    std::vector<OnDiskDataset> datasets;
};


void yield_trigrams(std::ifstream &infile, long insize, std::vector<TriGram> &out) {
    uint8_t ringbuffer[3];
    infile.read((char *)ringbuffer, 3);
    int offset = 2;

    std::cout << insize << std::endl;

    while (offset < insize) {
        uint32_t gram3 =
                (ringbuffer[(offset - 2) % 3] << 16U) +
                (ringbuffer[(offset - 1) % 3] << 8U) +
                (ringbuffer[(offset - 0) % 3] << 0U);
        out.push_back(gram3);
        offset += 1;
        infile.read((char *)&ringbuffer[offset % 3], 1);
    }
}

void display_trigram(uint32_t trigram) {
    std::cout << (char)((trigram >> 16U) & 0xFFU) << " " << (char)((trigram >> 8U) & 0xFFU)
              << " " << (char)((trigram >> 0U) & 0xFFU) << ": " << (int)trigram << std::endl;
}

void index_file(DatasetBuilder &builder, const std::string &fname) {
    FileId fid = builder.register_fname(fname);

    std::ifstream in(fname, std::ifstream::ate | std::ifstream::binary);
    long fsize = in.tellg();
    in.seekg(0, std::ifstream::beg);

    std::vector<TriGram> out;
    yield_trigrams(in, fsize, out);

    for (TriGram gram3 : out) {
        std::cout << (int)gram3 << " " << std::endl;
        builder.add_trigram(fid, gram3);
    }
}


DatasetBuilder builder;
OnDiskDataset idx;

int main() {
    index_file(builder, "test.txt");
    index_file(builder, "test2.txt");
    index_file(builder, "test3.txt");
    index_file(builder, "test4.txt");
    builder.save("dataset.ursa");

    idx.load("dataset.ursa");

    /* for (int i = 0; i < 16777216; i++) {
        if (idx.run_offsets[i] != idx.run_offsets[i+1]) {
            display_trigram((uint32_t)i);
        }
    } */

    std::vector<FileId> fid = idx.query_index((('U' << 16U) + ('T' << 8U) + ('F' << 0U)));

    for (auto f : fid) {
        std::cout << idx.get_file_name(f) << std::endl;
    }

    std::vector<FileId> fid2 = idx.query_index((('p' << 16U) + ('a' << 8U) + ('.' << 0U)));

    for (auto f : fid2) {
        std::cout << idx.get_file_name(f) << std::endl;
    }

    return 0;
}
