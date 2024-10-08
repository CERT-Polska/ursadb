#define CATCH_CONFIG_MAIN

#include <cstdlib>
#include <string>
#include <utility>
#include <variant>

#include "catch/Catch.h"
#include "libursa/BitmapIndexBuilder.h"
#include "libursa/Database.h"
#include "libursa/DatabaseSnapshot.h"
#include "libursa/FlatIndexBuilder.h"
#include "libursa/IndexBuilder.h"
#include "libursa/OnDiskDataset.h"
#include "libursa/OnDiskIndex.h"
#include "libursa/Query.h"
#include "libursa/QueryParser.h"
#include "libursa/ResultWriter.h"
#include "libursa/Utils.h"

// For string literal ( "xxx"s ).
using namespace std::string_literals;

TriGram gram3_pack(const char (&s)[4]) {
    TriGram v0 = static_cast<uint8_t>(s[0]);
    TriGram v1 = static_cast<uint8_t>(s[1]);
    TriGram v2 = static_cast<uint8_t>(s[2]);
    return (v0 << 16U) | (v1 << 8U) | (v2 << 0U);
}

TriGram text4_pack(const char (&s)[5]) {
    TriGram v0 = get_b64_value(s[0]);
    TriGram v1 = get_b64_value(s[1]);
    TriGram v2 = get_b64_value(s[2]);
    TriGram v3 = get_b64_value(s[3]);
    return (v0 << 18U) | (v1 << 12U) | (v2 << 6U) | (v3 << 0U);
}

template <typename T>
T parse(const std::string &query_text) {
    Command cmd = parse_command(query_text);
    REQUIRE(std::holds_alternative<T>(cmd));
    return std::move(std::get<T>(cmd));
}

QString mqs(const std::string &str) {
    QString out;
    for (const auto &c : str) {
        out.emplace_back(QToken::single(c));
    }
    return out;
}

TEST_CASE("packing 3grams", "[internal]") {
    // pay attention to the input, this covers unexpected sign extension
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    REQUIRE(gram3_pack("\xCC\xBB\xAA") == (TriGram)0xCCBBAAU);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    REQUIRE(gram3_pack("\xAA\xBB\xCC") == (TriGram)0xAABBCCU);
    REQUIRE(gram3_pack("abc") == (TriGram)0x616263);
}

TEST_CASE("select hexstring without spaces", "[queryparser]") {
    auto cmd = parse<SelectCommand>("select{4d534d};");
    REQUIRE(cmd.get_query() == q(mqs("MSM")));
}

TEST_CASE("select hexstring with spaces", "[queryparser]") {
    auto cmd = parse<SelectCommand>("select { 4d 53 4d };");
    REQUIRE(cmd.get_query() == q(mqs("MSM")));
}

TEST_CASE("select hexstring with mixed spaces", "[queryparser]") {
    auto cmd = parse<SelectCommand>("select { 4d 534d };");
    REQUIRE(cmd.get_query() == q(mqs("MSM")));
}

TEST_CASE("select hexstring with full wildcard", "[queryparser]") {
    QString expect;
    expect.emplace_back(std::move(QToken::single(0x4d)));
    expect.emplace_back(std::move(QToken::wildcard()));
    expect.emplace_back(std::move(QToken::single(0x4d)));

    auto cmd = parse<SelectCommand>("select { 4d ?? 4d };");
    REQUIRE(cmd.get_query() == q(std::move(expect)));
}

TEST_CASE("select hexstring with high wildcard", "[queryparser]") {
    QString expect;
    expect.emplace_back(std::move(QToken::single(0x4d)));
    expect.emplace_back(std::move(QToken::high_wildcard(0x03)));
    expect.emplace_back(std::move(QToken::single(0x4d)));

    auto cmd = parse<SelectCommand>("select { 4d ?3 4d };");
    REQUIRE(cmd.get_query() == q(std::move(expect)));
}

TEST_CASE("select hexstring with low wildcard", "[queryparser]") {
    QString expect;
    expect.emplace_back(std::move(QToken::single(0x4d)));
    expect.emplace_back(std::move(QToken::low_wildcard(0x50)));
    expect.emplace_back(std::move(QToken::single(0x4d)));

    auto cmd = parse<SelectCommand>("select { 4d 5? 4d };");
    REQUIRE(cmd.get_query() == q(std::move(expect)));
}

TEST_CASE("select hexstring with explicit options", "[queryparser]") {
    QString expect;
    expect.emplace_back(std::move(QToken::single(0x4d)));
    expect.emplace_back(std::move(QToken::with_values({0x51, 0x52})));
    expect.emplace_back(std::move(QToken::single(0x4d)));

    auto cmd = parse<SelectCommand>("select { 4d (51 | 52) 4d };");
    REQUIRE(cmd.get_query() == q(std::move(expect)));
}

TEST_CASE("select hexstring with mixed explicit options", "[queryparser]") {
    QString expect;
    expect.emplace_back(std::move(QToken::single(0x4d)));
    expect.emplace_back(std::move(QToken::with_values(
        {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b,
         0x5c, 0x5d, 0x5e, 0x5f, 0x70})));
    expect.emplace_back(std::move(QToken::single(0x4d)));

    auto cmd = parse<SelectCommand>("select { 4d (5? | 70) 4d };");
    REQUIRE(cmd.get_query() == q(std::move(expect)));
}

TEST_CASE("select literal", "[queryparser]") {
    auto cmd = parse<SelectCommand>("select \"MSM\";");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
}

TEST_CASE("select literal with hex escapes", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select "\x4d\x53\x4d";)");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
}

TEST_CASE("select literal with uppercase hex escapes", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select "\x4D\x53\x4D";)");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
}

TEST_CASE("select char escapses", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select "\n\t\r\f\b\\\"";)");
    REQUIRE(cmd.get_query() == q(std::move(mqs("\n\t\r\f\b\\\""))));
}

TEST_CASE("select OR", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select "test" | "cat";)");
    std::vector<Query> expect_or;
    expect_or.emplace_back(q(mqs("test")));
    expect_or.emplace_back(q(mqs("cat")));
    REQUIRE(cmd.get_query() == q_or(std::move(expect_or)));
}

TEST_CASE("select AND", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select "test" & "cat";)");
    std::vector<Query> expect_and;
    expect_and.emplace_back(q(mqs("test")));
    expect_and.emplace_back(q(mqs("cat")));
    REQUIRE(cmd.get_query() == q_and(std::move(expect_and)));
}

TEST_CASE("select operator order", "[queryparser]") {
    auto cmd =
        parse<SelectCommand>(R"(select "cat" | "dog" & "msm" | "monk";)");

    std::vector<Query> expect_or;
    expect_or.emplace_back(q(std::move(mqs("msm"))));
    expect_or.emplace_back(q(std::move(mqs("monk"))));

    std::vector<Query> expect_and;
    expect_and.emplace_back(q(std::move(mqs("dog"))));
    expect_and.emplace_back(q_or(std::move(expect_or)));

    std::vector<Query> expect_final;
    expect_final.emplace_back(q(std::move(mqs("cat"))));
    expect_final.emplace_back(q_and(std::move(expect_and)));

    REQUIRE(cmd.get_query() == q_or(std::move(expect_final)));
}

TEST_CASE("select min .. of", "[queryparser]") {
    auto cmd =
        parse<SelectCommand>(R"(select min 2 of ("xyz", "cat", "hmm");)");
    std::vector<Query> expect_min;
    expect_min.emplace_back(q(std::move(mqs("xyz"))));
    expect_min.emplace_back(q(std::move(mqs("cat"))));
    expect_min.emplace_back(q(std::move(mqs("hmm"))));

    REQUIRE(cmd.get_query() == q_min_of(2, std::move(expect_min)));
}

TEST_CASE("select literal without iterator", "[queryparser]") {
    auto cmd = parse<SelectCommand>("select \"MSM\";");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
    REQUIRE(cmd.iterator_requested() == false);
}

TEST_CASE("select literal into iterator", "[queryparser]") {
    auto cmd = parse<SelectCommand>("select into iterator \"MSM\";");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
    REQUIRE(cmd.iterator_requested() == true);
}

TEST_CASE("select literal with taints", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select with taints ["hmm"] "MSM";)");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
    REQUIRE(cmd.taints() == std::set<std::string>{"hmm"});
}

TEST_CASE("select literal with datasets", "[queryparser]") {
    auto cmd = parse<SelectCommand>(R"(select with datasets ["hmm"] "MSM";)");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
    REQUIRE(cmd.datasets() == std::set<std::string>{"hmm"});
}

TEST_CASE("select literal with datasets and taints", "[queryparser]") {
    auto cmd = parse<SelectCommand>(
        R"(select with taints ["kot"] with datasets ["hmm"] "MSM";)");
    REQUIRE(cmd.get_query() == q(std::move(mqs("MSM"))));
    REQUIRE(cmd.datasets() == std::set<std::string>{"hmm"});
    REQUIRE(cmd.taints() == std::set<std::string>{"kot"});
}

TEST_CASE("compact all command", "[queryparser]") {
    auto cmd = parse<CompactCommand>("compact all;");
    REQUIRE(cmd.get_type() == CompactType::All);
}

TEST_CASE("compact smart command", "[queryparser]") {
    auto cmd = parse<CompactCommand>("compact smart;");
    REQUIRE(cmd.get_type() == CompactType::Smart);
}

TEST_CASE("index command with default types", "[queryparser]") {
    auto cmd = parse<IndexCommand>("index \"cat\";");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"cat"});
    REQUIRE(cmd.get_index_types() == std::vector{IndexType::GRAM3});
    REQUIRE(cmd.ensure_unique() == true);
}

TEST_CASE("index command with type override", "[queryparser]") {
    auto cmd = parse<IndexCommand>("index \"cat\" with [text4, wide8];");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"cat"});
    REQUIRE(cmd.get_index_types() ==
            std::vector{IndexType::TEXT4, IndexType::WIDE8});
}

TEST_CASE("index command with nocheck", "[queryparser]") {
    auto cmd = parse<IndexCommand>("index \"cat\" nocheck;");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"cat"});
    REQUIRE(cmd.ensure_unique() == false);
}

TEST_CASE("index command with empty type override", "[queryparser]") {
    auto cmd = parse<IndexCommand>("index \"cat\" with [];");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"cat"});
    REQUIRE(cmd.get_index_types().empty());
}

TEST_CASE("index command with escapes", "[queryparser]") {
    auto cmd = parse<IndexCommand>(R"(index "\x63\x61\x74";)");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"cat"});
}

TEST_CASE("index command with hexstring", "[queryparser]") {
    auto cmd = parse<IndexCommand>("index {63 61 74};");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"cat"});
}

TEST_CASE("index command with multiple paths", "[queryparser]") {
    auto cmd = parse<IndexCommand>(R"(index "aaa" "bbb";)");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"aaa", "bbb"});
}

TEST_CASE("index command with taints", "[queryparser]") {
    auto cmd = parse<IndexCommand>(R"(index "aaa" with taints ["kot"];)");
    REQUIRE(cmd.get_paths() == std::vector<std::string>{"aaa"});
    REQUIRE(cmd.taints() == std::set<std::string>{"kot"});
}

TEST_CASE("index from command", "[queryparser]") {
    auto cmd = parse<IndexFromCommand>("index from list \"aaa\";");
    REQUIRE(cmd.get_path_list_fname() == "aaa");
    REQUIRE(cmd.get_index_types() == std::vector{IndexType::GRAM3});
    REQUIRE(cmd.ensure_unique() == true);
}

TEST_CASE("index from command with type override", "[queryparser]") {
    auto cmd = parse<IndexFromCommand>("index from list \"aaa\" with [hash4];");
    REQUIRE(cmd.get_path_list_fname() == "aaa");
    REQUIRE(cmd.get_index_types() == std::vector{IndexType::HASH4});
}

TEST_CASE("index from command with nocheck", "[queryparser]") {
    auto cmd = parse<IndexFromCommand>("index from list \"aaa\" nocheck;");
    REQUIRE(cmd.get_path_list_fname() == "aaa");
    REQUIRE(cmd.ensure_unique() == false);
}

TEST_CASE("dataset.taint command", "[queryparser]") {
    auto cmd = parse<TaintCommand>(R"(dataset "xyz" taint "hmm";)");
    REQUIRE(cmd.get_dataset() == "xyz");
    REQUIRE(cmd.get_mode() == TaintMode::Add);
    REQUIRE(cmd.get_taint() == "hmm");
}

TEST_CASE("dataset.untaint command", "[queryparser]") {
    auto cmd = parse<TaintCommand>(R"(dataset "xyz" untaint "hmm";)");
    REQUIRE(cmd.get_dataset() == "xyz");
    REQUIRE(cmd.get_mode() == TaintMode::Clear);
    REQUIRE(cmd.get_taint() == "hmm");
}

TEST_CASE("iterator.pop command", "[queryparser]") {
    auto cmd = parse<IteratorPopCommand>("iterator \"xyz\" pop 3;");
    REQUIRE(cmd.get_iterator_id() == "xyz");
    REQUIRE(cmd.elements_to_pop() == 3);
}

TEST_CASE("reindex command", "[queryparser]") {
    auto cmd = parse<ReindexCommand>("reindex \"xyz\" with [wide8];");
    REQUIRE(cmd.dataset_id() == "xyz");
    REQUIRE(cmd.get_index_types() == std::vector{IndexType::WIDE8});
}

TEST_CASE("topology command", "[queryparser]") {
    parse<TopologyCommand>("topology;");
}

TEST_CASE("ping command", "[queryparser]") { parse<PingCommand>("ping;"); }

TEST_CASE("status command", "[queryparser]") {
    parse<StatusCommand>("status;");
}

TEST_CASE("get_trigrams", "[ngrams]") {
    std::string str;
    std::vector<TriGram> gram3;

    SECTION("String len < 3") {
        str = "";
        gram3 = get_trigrams(reinterpret_cast<const uint8_t *>(str.c_str()),
                             str.length());
        REQUIRE(gram3.empty());
        str = "a";
        gram3 = get_trigrams(reinterpret_cast<const uint8_t *>(str.c_str()),
                             str.length());
        REQUIRE(gram3.empty());
        str = "aa";
        gram3 = get_trigrams(reinterpret_cast<const uint8_t *>(str.c_str()),
                             str.length());
        REQUIRE(gram3.empty());
    }

    SECTION("String len 3") {
        str = "abc";
        gram3 = get_trigrams(reinterpret_cast<const uint8_t *>(str.c_str()),
                             str.length());
        REQUIRE(gram3[0] == gram3_pack("abc"));
        REQUIRE(gram3.size() == 1);
    }

    SECTION("String len 4") {
        str = "abcd";
        gram3 = get_trigrams(reinterpret_cast<const uint8_t *>(str.c_str()),
                             str.length());
        REQUIRE(gram3[0] == gram3_pack("abc"));
        REQUIRE(gram3[1] == gram3_pack("bcd"));
        REQUIRE(gram3.size() == 2);
    }
}

TEST_CASE("get_b64grams", "[ngrams]") {
    std::string str;
    std::vector<TriGram> gram3;

    str = "";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.empty());
    str = "a";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.empty());
    str = "ab";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.empty());
    str = "abc";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.empty());
    str = "abcd";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    str = "abcde";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.size() == 2);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    REQUIRE(gram3[1] == text4_pack("bcde"));
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    str = "abcde\xAAXghi";
    gram3 = get_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                         str.length());
    REQUIRE(gram3.size() == 3);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    REQUIRE(gram3[1] == text4_pack("bcde"));
    REQUIRE(gram3[2] == text4_pack("Xghi"));
}

TEST_CASE("get_wide_b64grams", "[ngrams]") {
    std::string str;
    std::vector<TriGram> gram3;

    str = "";
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.empty());
    str = "a";
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.empty());
    str = "ab";
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.empty());
    str = "abcdefg";
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.empty());
    str = std::string("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.empty());
    str = std::string("a\0b\0c\0d\0", 8);
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    str = std::string("a\0b\0c\0d\0e\0", 10);
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.size() == 2);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    REQUIRE(gram3[1] == text4_pack("bcde"));
    str = std::string("\0a\0b\0c\0d", 8);
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.empty());
    str = std::string("\0a\0b\0c\0d\0", 9);
    gram3 = get_wide_b64grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                              str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == text4_pack("abcd"));
}

TEST_CASE("get_h4grams", "[ngrams]") {
    std::string str;
    std::vector<TriGram> gram3;

    str = "";
    gram3 = get_h4grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                        str.length());
    REQUIRE(gram3.empty());
    str = "a";
    gram3 = get_h4grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                        str.length());
    REQUIRE(gram3.empty());
    str = "ab";
    gram3 = get_h4grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                        str.length());
    REQUIRE(gram3.empty());
    str = "abc";
    gram3 = get_h4grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                        str.length());
    REQUIRE(gram3.empty());
    str = "abcd";
    gram3 = get_h4grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                        str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == (gram3_pack("abc") ^ gram3_pack("bcd")));
    str = "abcde";
    gram3 = get_h4grams(reinterpret_cast<const uint8_t *>(str.c_str()),
                        str.length());
    REQUIRE(gram3.size() == 2);
    REQUIRE(gram3[0] == (gram3_pack("abc") ^ gram3_pack("bcd")));
    REQUIRE(gram3[1] == (gram3_pack("bcd") ^ gram3_pack("cde")));
}

TEST_CASE("Compress run symmetry", "[compress_run]") {
    std::stringstream ss;

    std::vector<FileId> fids;
    int last_fid = 0;

    srand(1337);

    for (int i = 0; i < 1000; i++) {
        if (rand() % 100 < 80) {
            last_fid += 1 + rand() % 120;
        } else {
            last_fid += 1 + rand() % 100000;
        }

        fids.push_back(last_fid);
    }

    compress_run(fids, ss);

    std::string s = ss.str();
    const auto *ptr = reinterpret_cast<uint8_t *>(s.data());

    std::vector<FileId> read_fids = read_compressed_run(ptr, ptr + s.length());

    REQUIRE(fids.size() == read_fids.size());

    for (size_t i = 0; i < fids.size(); i++) {
        REQUIRE(fids[i] == read_fids[i]);
    }
}

TEST_CASE("Compress run sanity", "[compress_run]") {
    std::stringstream ss;
    std::vector<FileId> fids = {1, 2, 5, 8, 256 + 8 + 1};
    compress_run(fids, ss);
    std::string s = ss.str();

    REQUIRE(s == std::string("\x01\x00\x02\x02\x80\x02", 6));
}

std::vector<FileId> pick_common(
    int cutoff, const std::vector<const std::vector<uint32_t> *> &sources_raw) {
    std::vector<SortedRun> sources;
    std::vector<SortedRun *> sourceptrs;
    sources.reserve(sources_raw.size());
    sourceptrs.reserve(sources_raw.size());

    for (auto &src : sources_raw) {
        sources.push_back(SortedRun(std::vector<uint32_t>(*src)));
        sourceptrs.push_back(&sources.back());
    }

    auto out{SortedRun::pick_common(cutoff, sourceptrs)};
    return out.decompressed();
}

TEST_CASE("Test pick_common", "[pick_common]") {
    std::vector<FileId> source1 = {1, 2, 3};
    REQUIRE(pick_common(1, {&source1}) == std::vector<FileId>{1, 2, 3});
    REQUIRE(pick_common(2, {&source1}).empty());

    std::vector<FileId> source2 = {3, 4, 5};
    REQUIRE(pick_common(1, {&source1, &source2}) ==
            std::vector<FileId>{1, 2, 3, 4, 5});
    REQUIRE(pick_common(2, {&source1, &source2}) == std::vector<FileId>{3});

    std::vector<FileId> source3 = {1, 2, 3};
    REQUIRE(pick_common(1, {&source1, &source3}) ==
            std::vector<FileId>{1, 2, 3});
    REQUIRE(pick_common(2, {&source1, &source3}) ==
            std::vector<FileId>{1, 2, 3});

    std::vector<FileId> source4 = {4, 5, 6};
    REQUIRE(pick_common(1, {&source1, &source4}) ==
            std::vector<FileId>{1, 2, 3, 4, 5, 6});
    REQUIRE(pick_common(2, {&source1, &source4}).empty());

    REQUIRE(pick_common(1, {&source1, &source2, &source4}) ==
            std::vector<FileId>{1, 2, 3, 4, 5, 6});
    REQUIRE(pick_common(2, {&source1, &source2, &source4}) ==
            std::vector<FileId>{3, 4, 5});
    REQUIRE(pick_common(3, {&source1, &source2, &source4}).empty());

    REQUIRE(pick_common(1, {&source1, &source2, &source3}) ==
            std::vector<FileId>{1, 2, 3, 4, 5});
    REQUIRE(pick_common(2, {&source1, &source2, &source3}) ==
            std::vector<FileId>{1, 2, 3});
    REQUIRE(pick_common(3, {&source1, &source2, &source3}) ==
            std::vector<FileId>{3});

    std::vector<FileId> source5 = {};
    REQUIRE(pick_common(1, {&source5}).empty());
    REQUIRE(pick_common(2, {&source5, &source5}).empty());
    REQUIRE(pick_common(1, {&source1, &source5}) ==
            std::vector<FileId>{1, 2, 3});
    REQUIRE(pick_common(1, {&source5, &source1}) ==
            std::vector<FileId>{1, 2, 3});
    REQUIRE(pick_common(2, {&source1, &source1, &source5}) ==
            std::vector<FileId>{1, 2, 3});
    REQUIRE(pick_common(2, {&source1, &source5, &source1}) ==
            std::vector<FileId>{1, 2, 3});
}
