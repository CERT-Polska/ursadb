#include "dataset.h"

void OnDiskDataset::load(const std::string &fname) {
    raw_data = std::ifstream(fname, std::ifstream::binary);
    char header[4] = { (char)0xCA, (char)0x7D, (char)0xA7, (char)0xA};
    char cur_header[4];
    raw_data.read((char *)&cur_header, 4);

    if (cur_header[0] != (char)0xCA || cur_header[1] != (char)0x7D || cur_header[2] != (char)0xA7 || cur_header[3] != (char)0xA) {
        throw std::runtime_error("Invalid header, improper magic.");
    }

    while (1) {
        uint16_t fnlen;
        raw_data.read((char *)&fnlen, 2);

        if (fnlen == 0) {
            break;
        }

        std::string str;
        str.resize(fnlen);
        raw_data.read(&str[0], fnlen);
        fnames.push_back(str);
    }

    raw_data.seekg(0, std::ifstream::end);

    long fsize = raw_data.tellg();
    raw_data.seekg(fsize - 16777216*4, std::ifstream::beg);
    raw_data.read((char *)run_offsets, 16777216*4);
}

const std::string &OnDiskDataset::get_file_name(FileId fid) {
    return fnames.at(fid);
}

std::vector<FileId> OnDiskDataset::read_compressed_run(std::ifstream &runs, size_t len) {
    std::vector<FileId> res;
    uint32_t acc = 0;
    uint32_t shift = 0;
    uint32_t base = 0;

    for (int i = 0; i < len; i++) {
        char next_raw;
        runs.read(&next_raw, 1);
        auto next = (uint32_t)next_raw;

        acc += (next & 0x7FU) << shift;
        shift += 7U;
        if ((next & 0x80U) == 0) {
            base += acc;
            res.push_back(base - 1U);
            acc = 0;
            shift = 0;
        }
    }

    return res;
}

std::vector<FileId> OnDiskDataset::query_index(const TriGram &trigram) {
    uint32_t ptr = run_offsets[trigram];
    uint32_t next_ptr = run_offsets[trigram+1]; // TODO ensure overflow

    raw_data.seekg(ptr);
    return read_compressed_run(raw_data, next_ptr - ptr);
}
