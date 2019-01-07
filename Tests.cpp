#define CATCH_CONFIG_MAIN

#include <cstdlib>
#include <variant>

#include "IndexBuilder.h"
#include "BitmapIndexBuilder.h"
#include "FlatIndexBuilder.h"
#include "OnDiskIndex.h"
#include "OnDiskDataset.h"
#include "Query.h"
#include "QueryParser.h"
#include "Utils.h"
#include "lib/Catch.h"

TriGram gram3_pack(const char (&s)[4]) {
    TriGram v0 = (uint8_t)s[0];
    TriGram v1 = (uint8_t)s[1];
    TriGram v2 = (uint8_t)s[2];
    return (v0 << 16U) | (v1 << 8U) | (v2 << 0U);
}

TriGram text4_pack(const char (&s)[5]) {
    TriGram v0 = get_b64_value(s[0]);
    TriGram v1 = get_b64_value(s[1]);
    TriGram v2 = get_b64_value(s[2]);
    TriGram v3 = get_b64_value(s[3]);
    return (v0 << 18U) | (v1 << 12U) | (v2 << 6U) | (v3 << 0U);
}

Query do_select(std::string query_text) {
    Command cmd = parse_command(query_text);
    REQUIRE(std::holds_alternative<SelectCommand>(cmd));
    return std::get<SelectCommand>(cmd).get_query();
}

QString mqs(const std::string &str) {
    QString out;

    for (const auto &c : str) {
        if (c != '?') {
            out.emplace_back(QTokenType::CHAR, c);
        } else {
            out.emplace_back(QTokenType::WILDCARD);
        }
    }

    return out;
}

TEST_CASE("pack", "[pack]") {
    // pay attention to the input, this covers unexpected sign extension
    REQUIRE(gram3_pack("\xCC\xBB\xAA") == (TriGram)0xCCBBAAU);
    REQUIRE(gram3_pack("\xAA\xBB\xCC") == (TriGram)0xAABBCCU);
    REQUIRE(gram3_pack("abc") == (TriGram)0x616263);
}

TEST_CASE("select simple", "[queryparser]") {
    Query q = do_select("select \"test\";");
    REQUIRE(q.get_type() == QueryType::PRIMITIVE);
    REQUIRE(q.as_string_repr() == "test");
}

TEST_CASE("select hex lowercase", "[queryparser]") {
    Query query = do_select("select {4d534d};");
    REQUIRE(query == q(mqs("MSM")));
}

TEST_CASE("select hex uppercase", "[queryparser]") {
    Query query = do_select("select {4D534D};");
    REQUIRE(query == q(mqs("MSM")));
}

TEST_CASE("select hex spaces", "[queryparser]") {
    Query query = do_select("select {4d 53 4d};");
    REQUIRE(query == q(mqs("MSM")));
}

TEST_CASE("select hex string escapses lowercase", "[queryparser]") {
    Query query = do_select("select \"\\x4d\\x53\\x4d\";");
    REQUIRE(query == q(mqs("MSM")));
}

TEST_CASE("select hex string escapses uppercase", "[queryparser]") {
    Query query = do_select("select \"\\x4D\\x53\\x4D\";");
    REQUIRE(query == q(mqs("MSM")));
}

TEST_CASE("select char escapses", "[queryparser]") {
    Query query = do_select("select \"\\n\\t\\r\\f\\b\\\\\\\"\";");
    REQUIRE(query == q(mqs("\n\t\r\f\b\\\"")));
}

TEST_CASE("select OR", "[queryparser]") {
    Query query = do_select("select \"test\" | \"cat\";");
    REQUIRE(query == q_or({q(mqs("test")), q(mqs("cat"))}));
}

TEST_CASE("select AND", "[queryparser]") {
    Query query = do_select("select \"test\" & \"cat\";");
    REQUIRE(query == q_and({q(mqs("test")), q(mqs("cat"))}));
}

TEST_CASE("select operator order", "[queryparser]") {
    Query query = do_select("select \"cat\" | \"dog\" & \"msm\" | \"monk\";");
    REQUIRE(query == q_or({q(mqs("cat")), q_and({q(mqs("dog")), q_or({q(mqs("msm")), q(mqs("monk"))})})}));
}

TEST_CASE("compact all command", "[queryparser]") {
    Command cmd = parse_command("compact all;");
    CompactCommand compact_cmd = std::get<CompactCommand>(cmd);
    REQUIRE(compact_cmd.get_type() == CompactType::All);
}

TEST_CASE("compact smart command", "[queryparser]") {
    Command cmd = parse_command("compact smart;");
    CompactCommand compact_cmd = std::get<CompactCommand>(cmd);
    REQUIRE(compact_cmd.get_type() == CompactType::Smart);
}

TEST_CASE("index command with default types", "[queryparser]") {
    Command cmd = parse_command("index \"cat\";");
    IndexCommand index_cmd = std::get<IndexCommand>(cmd);
    REQUIRE(index_cmd.get_paths() == std::vector<std::string> {"cat"});
    REQUIRE(index_cmd.get_index_types() == IndexCommand::default_types());
}

TEST_CASE("index command with type override", "[queryparser]") {
    Command cmd = parse_command("index \"cat\" with [text4, wide8];");
    IndexCommand index_cmd = std::get<IndexCommand>(cmd);
    REQUIRE(index_cmd.get_paths() == std::vector<std::string> {"cat"});
    REQUIRE(index_cmd.get_index_types() == std::vector{IndexType::TEXT4, IndexType::WIDE8});
}

TEST_CASE("index command with empty type override", "[queryparser]") {
    Command cmd = parse_command("index \"cat\" with [];");
    IndexCommand index_cmd = std::get<IndexCommand>(cmd);
    REQUIRE(index_cmd.get_paths() == std::vector<std::string> {"cat"});
    REQUIRE(index_cmd.get_index_types().empty());
}

TEST_CASE("index command with escapes", "[queryparser]") {
    Command cmd = parse_command("index \"\\x63\\x61\\x74\";");
    IndexCommand index_cmd = std::get<IndexCommand>(cmd);
    REQUIRE(index_cmd.get_paths() == std::vector<std::string> {"cat"});
}

TEST_CASE("index command with hexstring", "[queryparser]") {
    Command cmd = parse_command("index {63 61 74};");
    IndexCommand index_cmd = std::get<IndexCommand>(cmd);
    REQUIRE(index_cmd.get_paths() == std::vector<std::string> {"cat"});
}

TEST_CASE("get_trigrams", "[gram3]") {
    std::string str;
    std::vector<TriGram> gram3;

    SECTION("String len < 3") {
        str = "";
        gram3 = get_trigrams((const uint8_t *)str.c_str(), str.length());
        REQUIRE(gram3.empty());
        str = "a";
        gram3 = get_trigrams((const uint8_t *)str.c_str(), str.length());
        REQUIRE(gram3.empty());
        str = "aa";
        gram3 = get_trigrams((const uint8_t *)str.c_str(), str.length());
        REQUIRE(gram3.empty());
    }

    SECTION("String len 3") {
        str = "abc";
        gram3 = get_trigrams((const uint8_t *)str.c_str(), str.length());
        REQUIRE(gram3[0] == gram3_pack("abc"));
        REQUIRE(gram3.size() == 1);
    }

    SECTION("String len 4") {
        str = "abcd";
        gram3 = get_trigrams((const uint8_t *)str.c_str(), str.length());
        REQUIRE(gram3[0] == gram3_pack("abc"));
        REQUIRE(gram3[1] == gram3_pack("bcd"));
        REQUIRE(gram3.size() == 2);
    }
}

TEST_CASE("get_b64grams", "[text4]") {
    std::string str;
    std::vector<TriGram> gram3;

    str = "";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "a";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "ab";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "abc";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "abcd";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    str = "abcde";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 2);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    REQUIRE(gram3[1] == text4_pack("bcde"));
    str = "abcde\xAAXghi";
    gram3 = get_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 3);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    REQUIRE(gram3[1] == text4_pack("bcde"));
    REQUIRE(gram3[2] == text4_pack("Xghi"));
}

TEST_CASE("get_wide_b64grams", "[wide8]") {
    std::string str;
    std::vector<TriGram> gram3;

    str = "";
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "a";
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "ab";
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "abcdefg";
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = std::string("\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 10);
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = std::string("a\0b\0c\0d\0", 8);
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    str = std::string("a\0b\0c\0d\0e\0", 10);
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 2);
    REQUIRE(gram3[0] == text4_pack("abcd"));
    REQUIRE(gram3[1] == text4_pack("bcde"));
    str = std::string("\0a\0b\0c\0d", 8);
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = std::string("\0a\0b\0c\0d\0", 9);
    gram3 = get_wide_b64grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == text4_pack("abcd"));
}

TEST_CASE("get_h4grams", "[hash4]") {
    std::string str;
    std::vector<TriGram> gram3;

    str = "";
    gram3 = get_h4grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "a";
    gram3 = get_h4grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "ab";
    gram3 = get_h4grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "abc";
    gram3 = get_h4grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.empty());
    str = "abcd";
    gram3 = get_h4grams((const uint8_t *)str.c_str(), str.length());
    REQUIRE(gram3.size() == 1);
    REQUIRE(gram3[0] == (gram3_pack("abc") ^ gram3_pack("bcd")));
    str = "abcde";
    gram3 = get_h4grams((const uint8_t *)str.c_str(), str.length());
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
    const auto *ptr = (uint8_t *)s.data();

    std::vector<FileId> read_fids = read_compressed_run(ptr, ptr + s.length());

    REQUIRE(fids.size() == read_fids.size());

    for (unsigned int i = 0; i < fids.size(); i++) {
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

void add_test_payload(IndexBuilder &builder) {
    std::string contents;

    contents = "kjhg";
    builder.add_file(1, (const uint8_t *)contents.data(), contents.size());

    contents = "\xA1\xA2\xA3\xA4\xA5\xA6\xA7\xA8";
    builder.add_file(2, (const uint8_t *)contents.data(), contents.size());

    contents = "";
    builder.add_file(3, (const uint8_t *)contents.data(), contents.size());

    contents = "\xA1\xA2Xbcde\xA3\xA4\xA5\xA6\xA7systXm32\xA5Xcdef\xA6\xA7";
    builder.add_file(4, (const uint8_t *)contents.data(), contents.size());

    contents = "\xAA\xAA\xAA\xAA\xAA\xAAXm32\xA5Xd\xAA\xAA\xAA\xAA\xAA\xAA";
    builder.add_file(5, (const uint8_t *)contents.data(), contents.size());
}

void test_builder_gram3(IndexBuilder &builder, std::string &index_fname) {
    add_test_payload(builder);
    builder.save(index_fname);

    OnDiskIndex ndx(index_fname);
    std::vector<FileId> res;

    REQUIRE(ndx.query_str(mqs("")).is_everything());
    REQUIRE(ndx.query_str(mqs("a")).is_everything());
    REQUIRE(ndx.query_str(mqs("ab")).is_everything());

    res = ndx.query_str(mqs("kjhg")).vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 1);

    res = ndx.query_str(mqs("\xA1\xA2\xA3")).vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 2);

    res = ndx.query_str(mqs("m32\xA5X")).vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    res = ndx.query_str(mqs("Xm32\xA5X")).vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    res = ndx.query_str(mqs("Xm32\xA5s")).vector();
    REQUIRE(res.size() == 0);

    res = ndx.query_str(mqs("Xbcdef")).vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 4);

    res = ndx.query_str(mqs("\xA4\xA5\xA6\xA7")).vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 2);
    REQUIRE(res[1] == 4);

    std::remove(index_fname.c_str());
}

TEST_CASE("BitmapIndexBuilder for gram3", "[index_builder_gram3]") {
    std::string index_fname = "_test_idx_gram3.ursa";

    BitmapIndexBuilder builder(IndexType::GRAM3);
    test_builder_gram3(builder, index_fname);
}

void test_builder_text4(IndexBuilder &builder, std::string &index_fname) {
    add_test_payload(builder);
    builder.save(index_fname);

    OnDiskIndex ndx(index_fname);
    std::vector<FileId> res;

    REQUIRE(ndx.query_str(mqs("")).is_everything());
    REQUIRE(ndx.query_str(mqs("a")).is_everything());
    REQUIRE(ndx.query_str(mqs("ab")).is_everything());
    REQUIRE(ndx.query_str(mqs("abc")).is_everything());

    res = ndx.query_str(mqs("Xbcd")).vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 4);

    res = ndx.query_str(mqs("Xbcdef")).vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 4);

    res = ndx.query_str(mqs("m32\xA5X")).vector();
    REQUIRE(res.size() == 0);

    res = ndx.query_str(mqs("Xm32\xA5X")).vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    res = ndx.query_str(mqs("Xm32\xA5X")).vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    REQUIRE(ndx.query_str(mqs("\xA1\xA2\xA3")).is_everything());
    REQUIRE(ndx.query_str(mqs("d\xA6\xA7")).is_everything());
    REQUIRE(ndx.query_str(mqs("\xA4\xA5\xA6\xA7")).is_everything());

    std::remove(index_fname.c_str());
}

TEST_CASE("BitmapIndexBuilder for text4", "[index_builder_text4]") {
    std::string index_fname = "_test_idx_text4.ursa";

    BitmapIndexBuilder builder(IndexType::TEXT4);
    test_builder_text4(builder, index_fname);
}

void make_query(Database &db, std::string query_str, std::set<std::string> expected_out) {
    Task *task = db.allocate_task();
    Query query = do_select(query_str);
    std::vector<std::string> out;
    db.snapshot().execute(query, task, &out);
    db.commit_task(task->id);

    std::vector<std::string> out_fixed;

    for (const auto &x : out) {
        std::string xx = x.substr(x.find_last_of("/") + 1);
        xx.resize(xx.size() - 4);
        out_fixed.push_back(xx);
    }

    std::set<std::string> out_set(out_fixed.begin(), out_fixed.end());
    REQUIRE(out_set == expected_out);
}

TEST_CASE("Query end2end test", "[e2e_test]") {
    Database::create("_test_db.ursa");
    Database db("_test_db.ursa");
    DatabaseSnapshot snap = db.snapshot();

    Task *task = db.allocate_task();
    db.snapshot().index_path(task, {IndexType::GRAM3, IndexType::HASH4, IndexType::TEXT4, IndexType::WIDE8}, {"test/"});
    db.commit_task(task->id);

    make_query(db, "select \"nonexistent\";", {});
    make_query(db, "select min 1 of ({000000}, {010101});", {});
    make_query(db, "select min 2 of ({000000});", {});
    make_query(db, "select \"foot\" & \"ing\";", {"ISLT", "WEEC", "GJND", "QTXN"});
    make_query(db, "select \"dragons\" & \"bridge\";", {"GJND", "QTXN"});
    make_query(db, "select min 2 of (\"wing\", \"tool\", \"less\");", {"IPVX", "GJND", "IJKZ"});
}

TEST_CASE("Test internal_pick_common", "[internal_pick_common]") {
    std::vector<FileId> source1 = {1, 2, 3};
    REQUIRE(internal_pick_common(1, {&source1}) == std::vector<FileId> {1, 2, 3});
    REQUIRE(internal_pick_common(2, {&source1}) == std::vector<FileId> {});

    std::vector<FileId> source2 = {3, 4, 5};
    REQUIRE(internal_pick_common(1, {&source1, &source2}) == std::vector<FileId> {1, 2, 3, 4, 5});
    REQUIRE(internal_pick_common(2, {&source1, &source2}) == std::vector<FileId> {3});

    std::vector<FileId> source3 = {1, 2, 3};
    REQUIRE(internal_pick_common(1, {&source1, &source3}) == std::vector<FileId> {1, 2, 3});
    REQUIRE(internal_pick_common(2, {&source1, &source3}) == std::vector<FileId> {1, 2, 3});

    std::vector<FileId> source4 = {4, 5, 6};
    REQUIRE(internal_pick_common(1, {&source1, &source4}) == std::vector<FileId> {1, 2, 3, 4, 5, 6});
    REQUIRE(internal_pick_common(2, {&source1, &source4}) == std::vector<FileId> {});

    REQUIRE(internal_pick_common(1, {&source1, &source2, &source4}) == std::vector<FileId> {1, 2, 3, 4, 5, 6});
    REQUIRE(internal_pick_common(2, {&source1, &source2, &source4}) == std::vector<FileId> {3, 4, 5});
    REQUIRE(internal_pick_common(3, {&source1, &source2, &source4}) == std::vector<FileId> {});

    REQUIRE(internal_pick_common(1, {&source1, &source2, &source3}) == std::vector<FileId> {1, 2, 3, 4, 5});
    REQUIRE(internal_pick_common(2, {&source1, &source2, &source3}) == std::vector<FileId> {1, 2, 3});
    REQUIRE(internal_pick_common(3, {&source1, &source2, &source3}) == std::vector<FileId> {3});

    std::vector<FileId> source5 = {};
    REQUIRE(internal_pick_common(1, {&source5}) == std::vector<FileId> {});
    REQUIRE(internal_pick_common(2, {&source5, &source5}) == std::vector<FileId> {});
    REQUIRE(internal_pick_common(1, {&source1, &source5}) == std::vector<FileId> {1, 2, 3});
    REQUIRE(internal_pick_common(1, {&source5, &source1}) == std::vector<FileId> {1, 2, 3});
    REQUIRE(internal_pick_common(2, {&source1, &source1, &source5}) == std::vector<FileId> {1, 2, 3});
    REQUIRE(internal_pick_common(2, {&source1, &source5, &source1}) == std::vector<FileId> {1, 2, 3});
}
