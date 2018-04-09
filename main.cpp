#include <iostream>
#include <unistd.h>
#include <vector>

#define FileId uint32_t
#define TriGram uint32_t


class Index {
    uint32_t run_offsets[16777216];

public:
    std::vector<FileId> read_compressed_run(const std::vector<uint8_t> &run) {
        std::vector<FileId> res;
        uint32_t acc = 0;
        uint32_t shift = 0;
        uint32_t base = 0;

        for (uint32_t next : run) {
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

    std::vector<uint8_t> compress_run(const std::vector<FileId> &files) {
        uint32_t prev = 0;
        std::vector<uint8_t> result;

        for (uint32_t fid : files) {
            uint32_t diff = (fid + 1U) - prev;
            while (diff >= 0x80U) {
                result.push_back((uint8_t)(0x80U | (diff & 0x7FU)));
                diff >>= 7;
            }
            result.push_back((uint8_t)diff);
            prev = fid + 1U;
        }

        return result;
    }

    std::vector<FileId> query_index(const TriGram &trigram) {
        uint32_t ptr = run_offsets[trigram];
        uint32_t next_ptr = run_offsets[trigram+1]; // TODO ensure overflow
        // TODO run_data[ptr] until next_ptr
        // return read_compressed_run()
    }
};


int main() {
    std::cout << "Hello, World!" << std::endl;
    std::vector<FileId> fid;
    for (int i = 0; i < 1000; i++) {
        fid.push_back(rand());
    }

    for (FileId f : fid) {
        std::cout << (int)f << " ";
    }
    std::cout << std::endl;

    std::vector<uint8_t> uvt = compress_run(fid);
    for (uint8_t v : uvt) {
        std::cout << (int)v << " ";
    }
    std::cout << std::endl;

    std::vector<FileId> fid2 = read_compressed_run(uvt);

    for (FileId f : fid) {
        std::cout << (int)f << " ";
    }
    std::cout << std::endl;

    return 0;
}
