#include <iostream>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <array>
#include <list>

#define FileId uint32_t
#define TriGram uint32_t


class DatasetBuilder {
    std::vector<std::string> fids;
    std::array<std::vector<FileId>, 16777216> index;

public:
    DatasetBuilder() {

    }

    FileId register_fname(const std::string &fname) {
        auto new_id = (FileId)fids.size();
        fids.push_back(fname);
        return new_id;
    }

    void add_trigram(const FileId &fid, const TriGram &val) {
        index[val].push_back(fid);
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

    void save() {
        std::vector<uint32_t> offsets;
        std::ofstream out("dataset.ursa", std::ofstream::binary);

        char header[4] = { (char)0xCA, (char)0x7D, (char)0xA7, (char)0xA};
        out.write(header, 4);

        for (int i = 0; i < 16777216; i++) {
            offsets.push_back((uint32_t)out.tellp());
            for (auto b : compress_run(index[i])) {
                out.write((char *)&b, 1);
            }
        }

        for (auto offset : offsets) {
            out.write((char *)&offset, 4);
        }

        out.close();
    }
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


class Index {
public:
    std::ifstream raw_data;
    uint32_t run_offsets[16777216];

    Index() {
        raw_data = std::ifstream("dataset.ursa", std::ifstream::ate | std::ifstream::binary);
        long fsize = raw_data.tellg();
        raw_data.seekg(fsize - 16777216*4, std::ifstream::beg);
        raw_data.read((char *)run_offsets, 16777216*4);
    }

    std::vector<FileId> read_compressed_run(std::ifstream &runs, size_t len) {
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

    std::vector<FileId> query_index(const TriGram &trigram) {
        uint32_t ptr = run_offsets[trigram];
        uint32_t next_ptr = run_offsets[trigram+1]; // TODO ensure overflow

        raw_data.seekg(ptr);
        return read_compressed_run(raw_data, next_ptr - ptr);
    }
};

DatasetBuilder builder;
Index idx;

int main() {
    /* index_file(builder, "test.txt");
    index_file(builder, "test2.txt");
    index_file(builder, "test3.txt");
    builder.save(); */

    for (int i = 0; i < 16777216; i++) {
        if (idx.run_offsets[i] != idx.run_offsets[i+1]) {
            display_trigram((uint32_t)i);
        }
    }

    std::vector<FileId> fid = idx.query_index(6583664);

    for (auto f : fid) {
        std::cout << f << std::endl;
    }

    return 0;
}
