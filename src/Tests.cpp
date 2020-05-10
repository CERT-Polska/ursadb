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
#include "libursa/QueryGraph.h"
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

QueryGraph mqg(const std::string &str, IndexType type) {
    QString out;
    for (const auto &c : str) {
        out.emplace_back(QToken::single(c));
    }
    return q(std::move(out)).to_graph(type, DatabaseConfig());
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

std::vector<std::string> basic_test_payload{
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    "kjhg", "\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8", "",
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    "\xA1\xA2Xbcde\xA3\xA4\xA5\xA6\xA7systXm32\xA5Xcdef\xA6\xA7",
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    "\xAA\xAA\xAA\xAA\xAA\xAAXm32\xA5Xd\xAA\xAA\xAA\xAA\xAA\xAA"};

void add_payload(IndexBuilder *builder,
                 const std::vector<std::string> &payload) {
    std::string contents;

    for (size_t i = 0; i < payload.size(); i++) {
        builder->add_file(i + 1,
                          reinterpret_cast<const uint8_t *>(payload[i].data()),
                          payload[i].size());
    }
}

void check_query_is_everything(const OnDiskIndex &ndx, const std::string &query,
                               IndexType type) {
    QueryCounters dummy;
    REQUIRE(ndx.query(mqg(query, type), &dummy).is_everything());
}

void check_query(const OnDiskIndex &ndx, const std::string &query,
                 std::vector<uint32_t> results, IndexType type) {
    QueryCounters dummy;
    auto run = SortedRun(std::move(results));
    REQUIRE(ndx.query(mqg(query, type), &dummy).vector() == run);
}

void check_test_payload_gram3(const OnDiskIndex &ndx) {
    auto type = IndexType::GRAM3;
    check_query_is_everything(ndx, "", type);
    check_query_is_everything(ndx, "a", type);
    check_query_is_everything(ndx, "ab", type);
    check_query(ndx, "kjhg", {1}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "\xA1\xA2\xA3", {2}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "m32\xA5X", {4, 5}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "Xm32\xA5X", {4, 5}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "Xm32\xA5s", {}, type);
    check_query(ndx, "Xbcdef", {4}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "\xA4\xA5\xA6\xA7", {2, 4}, type);
}

void check_test_payload_text4(const OnDiskIndex &ndx) {
    auto type = IndexType::TEXT4;
    check_query_is_everything(ndx, "", type);
    check_query_is_everything(ndx, "a", type);
    check_query_is_everything(ndx, "ab", type);
    check_query_is_everything(ndx, "abc", type);
    check_query(ndx, "Xbcd", {4}, type);
    check_query(ndx, "Xbcdef", {4}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "m32\xA5X", {}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query(ndx, "Xm32\xA5X", {4, 5}, type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query_is_everything(ndx, "\xA1\xA2\xA3", type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query_is_everything(ndx, "d\xA6\xA7", type);
    // NOLINTNEXTLINE(modernize-raw-string-literal)
    check_query_is_everything(ndx, "\xA4\xA5\xA6\xA7", type);
}

// RAII helper for OnDiskIndex.
template <typename BuilderT>
class OnDiskIndexTest {
    OnDiskIndex index_;

    static std::string build_and_get_fname(IndexType type) {
        fs::path test_fname = fs::temp_directory_path();
        test_fname /= "test_ndx_" + random_hex_string(8);
        BuilderT builder(type);
        add_payload(&builder, basic_test_payload);
        builder.save(test_fname);
        return test_fname;
    }

   public:
    explicit OnDiskIndexTest(IndexType type)
        : index_(build_and_get_fname(type)) {}

    ~OnDiskIndexTest() { fs::remove(index_.get_fpath()); }

    const OnDiskIndex &get() const { return index_; }
};

TEST_CASE("BitmapIndexBuilder for gram3", "[index_builder]") {
    OnDiskIndexTest<BitmapIndexBuilder> index(IndexType::GRAM3);
    check_test_payload_gram3(index.get());
}

TEST_CASE("BitmapIndexBuilder for text4", "[index_builder]]") {
    OnDiskIndexTest<BitmapIndexBuilder> index(IndexType::TEXT4);
    check_test_payload_text4(index.get());
}

TEST_CASE("FlatIndexBuilder for gram3", "[index_builder]") {
    OnDiskIndexTest<FlatIndexBuilder> index(IndexType::GRAM3);
    check_test_payload_gram3(index.get());
}

TEST_CASE("FlatIndexBuilder for text4", "[index_builder]") {
    OnDiskIndexTest<FlatIndexBuilder> index(IndexType::TEXT4);
    check_test_payload_text4(index.get());
}

std::vector<FileId> pick_common(
    int cutoff, const std::vector<const std::vector<uint32_t> *> &sources_raw) {
    std::vector<SortedRun> sources;
    std::vector<const SortedRun *> sourceptrs;
    sources.reserve(sources_raw.size());
    sourceptrs.reserve(sources_raw.size());

    for (const auto &src : sources_raw) {
        sources.push_back(SortedRun(std::vector<uint32_t>(*src)));
        sourceptrs.push_back(&sources.back());
    }

    auto out{SortedRun::pick_common(cutoff, sourceptrs)};
    return std::vector<FileId>(out.begin(), out.end());
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

QueryGraph make_kot() {
    // Basically:  k -> o -> t
    return QueryGraph::from_qstring(mqs("kot"));
}

QueryGraph make_caet() {
    //                  a
    // Basically:  c -<   >- t
    //                  e
    QString expect;
    expect.emplace_back(std::move(QToken::single('c')));
    expect.emplace_back(std::move(QToken::with_values({'a', 'e'})));
    expect.emplace_back(std::move(QToken::single('t')));
    return QueryGraph::from_qstring(expect);
}

TEST_CASE("Simple query graph parse", "[query_graphs]") {
    auto graph{make_kot()};

    REQUIRE(graph.size() == 3);
}

TEST_CASE("Graph parse with wildcard", "[query_graphs]") {
    auto graph{make_caet()};

    REQUIRE(graph.size() == 4);
}

TEST_CASE("Simple graph and", "[query_graphs]") {
    auto graph{make_kot()};

    SECTION("With kot") {
        graph.and_(std::move(make_kot()));
        REQUIRE(graph.size() == 7);
    }

    SECTION("With caet") {
        graph.and_(std::move(make_caet()));
        REQUIRE(graph.size() == 8);
    }
}

QueryFunc make_oracle(const std::string &accepting) {
    return [accepting](uint64_t gram1) {
        if (accepting.find(static_cast<char>(gram1)) != std::string::npos) {
            return QueryResult::everything();
        }
        return QueryResult::empty();
    };
}

TEST_CASE("Test basic query", "[query_graphs]") {
    auto graph{make_kot()};

    QueryCounters dummy;
    REQUIRE(graph.run(make_oracle("tok"), &dummy).is_everything());
    REQUIRE(!graph.run(make_oracle("abc"), &dummy).is_everything());
}

TEST_CASE("Test wildcard query", "[query_graphs]") {
    auto graph{make_caet()};

    QueryCounters dummy;
    REQUIRE(graph.run(make_oracle("cat"), &dummy).is_everything());
    REQUIRE(graph.run(make_oracle("cet"), &dummy).is_everything());
    REQUIRE(!graph.run(make_oracle("abc"), &dummy).is_everything());
}

// Special value, ensure that all expected queries were asked.
const uint64_t ORACLE_CHECK_MAGIC = std::numeric_limits<uint64_t>::max();

QueryFunc make_expect_oracle(IndexType type, std::vector<std::string> strings) {
    std::vector<uint32_t> accepted;
    for (const auto &str : strings) {
        const auto *dataptr = reinterpret_cast<const uint8_t *>(str.data());
        get_generator_for(type)(dataptr, str.size(), [&accepted](auto gram) {
            accepted.push_back(gram);
        });
    }
    return [type, accepted{std::move(accepted)},
            strings{std::move(strings)}](uint64_t raw_gram) mutable {
        if (raw_gram == ORACLE_CHECK_MAGIC) {
            if (!accepted.empty()) {
                throw std::runtime_error("Not all expected queries performed");
            }
            return QueryResult::everything();
        }
        std::optional<uint32_t> maybe_gram = convert_gram(type, raw_gram);
        if (!maybe_gram) {
            return QueryResult::everything();
        }
        uint32_t gram = *maybe_gram;
        auto it = std::find(accepted.begin(), accepted.end(), gram);
        if (it == accepted.end()) {
            throw std::runtime_error("Unexpected query");
        }
        accepted.erase(it);

        return QueryResult::everything();
    };
}

// Ensure that the queries that were executed match exectly to expected ones.
// This makes sure that query was parsed correctly and contains expected ngrams.
// For example: ensure_queries(mqs("cats"), IndexType::GRAM3, {"cats"});
// or: ensure_queries(mqs("cats"), IndexType::GRAM3, {"cat", "ats"});
//
// Make sure that there the graph will execute two queries, "cat" and "ats".
void ensure_queries(const QString &query, IndexType type,
                    std::vector<std::string> strings) {
    auto validator = get_validator_for(type);
    size_t size = get_ngram_size_for(type);
    QueryGraph graph{to_query_graph(query, size, DatabaseConfig(), validator)};
    auto oracle = make_expect_oracle(type, std::move(strings));
    QueryCounters dummy;
    graph.run(oracle, &dummy);
    oracle(ORACLE_CHECK_MAGIC);
}

TEST_CASE("Test gram3 graph generator: gram3 x 0", "[query_graphs]") {
    ensure_queries(mqs("ca"), IndexType::GRAM3, {});
}

TEST_CASE("Test gram3 graph generator: gram3 x 1", "[query_graphs]") {
    ensure_queries(mqs("cat"), IndexType::GRAM3, {"cat"});
}

TEST_CASE("Test gram3 graph generator: gram3 x 2", "[query_graphs]") {
    ensure_queries(mqs("cats"), IndexType::GRAM3, {"cats"});
}

TEST_CASE("Test text4 graph generator: text4 x 0", "[query_graphs]") {
    ensure_queries(mqs("cat"), IndexType::TEXT4, {});
}

TEST_CASE("Test text4 graph generator: text4 x 1", "[query_graphs]") {
    ensure_queries(mqs("cats"), IndexType::TEXT4, {"cats"});
}

TEST_CASE("Test text4 graph generator: text4 x 6", "[query_graphs]") {
    ensure_queries(mqs("catsNdogs"), IndexType::TEXT4, {"catsNdogs"});
}

TEST_CASE("Test text4 graph generator: text4 x (2+2)", "[query_graphs]") {
    ensure_queries(mqs("cats!dogs"), IndexType::TEXT4, {"cats", "dogs"});
}

TEST_CASE("Test hash4 graph generator: hash4 x 0", "[query_graphs]") {
    ensure_queries(mqs("cat"), IndexType::HASH4, {});
}

TEST_CASE("Test hash4 graph generator: hash4 x (6)", "[query_graphs]") {
    ensure_queries(mqs("cats!dogs"), IndexType::HASH4, {"cats!dogs"});
}

TEST_CASE("Test wide8 graph generator: wide8 x (0)", "[query_graphs]") {
    ensure_queries(mqs("a\0b\0c\0d"s), IndexType::WIDE8, {});
}

TEST_CASE("Test wide8 graph generator: wide8 x (1)", "[query_graphs]") {
    ensure_queries(mqs("a\0b\0c\0d\0"s), IndexType::WIDE8, {"a\0b\0c\0d\0"s});
}

TEST_CASE("Test wide8 graph generator: wide8 x (1)'", "[query_graphs]") {
    ensure_queries(mqs("\0a\0b\0c\0d\0"s), IndexType::WIDE8, {"a\0b\0c\0d\0"s});
}

TEST_CASE("Test wide8 graph generator: wide8 x (2)''", "[query_graphs]") {
    ensure_queries(mqs("cats\0a\0b\0c\0d\0hmm"s), IndexType::WIDE8,
                   {"s\0a\0b\0c\0d\0"s});
}

TEST_CASE("Test wide8 graph generator: wide8 x (1+1)''", "[query_graphs]") {
    ensure_queries(mqs("ssda\0b\0c\0d\0hmmq\0w\0e\0r\0xtq"s), IndexType::WIDE8,
                   {"a\0b\0c\0d\0"s, "q\0w\0e\0r\0"s});
}

QString mqs_alphabet() {
    QString expect;
    expect.emplace_back(std::move(QToken::single('a')));
    expect.emplace_back(std::move(QToken::single('b')));
    expect.emplace_back(std::move(QToken::single('c')));
    expect.emplace_back(std::move(QToken::with_values({'d', 'e', '!'})));
    expect.emplace_back(std::move(QToken::single('f')));
    expect.emplace_back(std::move(QToken::single('g')));
    expect.emplace_back(std::move(QToken::single('h')));
    return expect;
}

TEST_CASE("Test gram3 wildcards generator", "[query_graphs]") {
    ensure_queries(mqs_alphabet(), IndexType::GRAM3,
                   {"abc", "bcd", "cdf", "dfg", "bc!", "c!f", "!fg", "bce",
                    "cef", "efg", "fgh"});
}

TEST_CASE("Test text4 wildcards generator", "[query_graphs]") {
    ensure_queries(mqs_alphabet(), IndexType::TEXT4, {"abcdfgh", "abcefgh"});
}

TEST_CASE("Test hash4 wildcards generator", "[query_graphs]") {
    ensure_queries(mqs_alphabet(), IndexType::HASH4,
                   {"abcdfgh", "abcefgh", "abc!fgh"});
}

QString mqs_null_alphabet() {
    QString expect;
    expect.emplace_back(std::move(QToken::single('a')));
    expect.emplace_back(std::move(QToken::single('\0')));
    expect.emplace_back(std::move(QToken::single('b')));
    expect.emplace_back(std::move(QToken::single('\0')));
    expect.emplace_back(std::move(QToken::with_values({'c', 'd'})));
    expect.emplace_back(std::move(QToken::with_values({'\0', 'e'})));
    expect.emplace_back(std::move(QToken::single('d')));
    expect.emplace_back(std::move(QToken::single('\0')));
    return expect;
}

TEST_CASE("Test wide8 wildcards generator (alphabet)", "[query_graphs]") {
    ensure_queries(mqs_alphabet(), IndexType::WIDE8,
                   {"a\0b\0c\0d\0", "a\0b\0d\0d\0"});
}

QString mqs_spaghetti() {
    QString expect;
    expect.emplace_back(std::move(QToken::with_values({'a', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'b', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'c', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'d', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'e', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'f', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'g', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'h', '\0'})));
    expect.emplace_back(std::move(QToken::with_values({'i', '\0'})));
    return expect;
}

TEST_CASE("Test wide8 wildcards generator (spaghetti)", "[query_graphs]") {
    ensure_queries(mqs_alphabet(), IndexType::WIDE8,
                   {"a\0c\0e\0g\0", "b\0d\0f\0i\0"});
}
