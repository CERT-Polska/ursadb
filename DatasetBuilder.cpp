#include "DatasetBuilder.h"

#include "Utils.h"
#include "OnDiskDataset.h"

DatasetBuilder::DatasetBuilder() {
    indices.emplace_back();
}

FileId DatasetBuilder::register_fname(const std::string &fname) {
    auto new_id = (FileId) fids.size();
    fids.push_back(fname);
    return new_id;
}

void DatasetBuilder::save(const std::string &fname) {
    indices[0].save("index." + fname);

    json dataset;

    std::set<std::string> c_set{"index." + fname};
    json j_indices(c_set);
    json j_fids(fids);

    dataset["indices"] = j_indices;
    dataset["filenames"] = j_fids;

    std::ofstream o(fname);
    o << std::setw(4) << dataset << std::endl;
    o.close();
}

void DatasetBuilder::index(const std::string &filepath) {
    FileId fid = register_fname(filepath);

    std::ifstream in(filepath, std::ifstream::ate | std::ifstream::binary);
    long fsize = in.tellg();
    in.seekg(0, std::ifstream::beg);

    std::vector<TriGram> out = get_trigrams(in, fsize);

    for (TriGram gram3 : out) {
        std::cout << "add trigram " << std::hex << gram3 << std::endl;
        indices[0].add_trigram(fid, gram3);
    }
}
