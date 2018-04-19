#define CATCH_CONFIG_MAIN

#include <cstdlib>
#include <variant>

#include "IndexBuilder.h"
#include "OnDiskIndex.h"
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

TEST_CASE("Test pack", "[pack]") {
    // pay attention to the input, this covers unexpected sign extension
    REQUIRE(gram3_pack("\xCC\xBB\xAA") == (TriGram)0xCCBBAAU);
    REQUIRE(gram3_pack("\xAA\xBB\xCC") == (TriGram)0xAABBCCU);
    REQUIRE(gram3_pack("abc") == (TriGram)0x616263);
}

TEST_CASE("Test select simple", "[queryparser]") {
    Command cmd = parse_command("select \"test\";");
    REQUIRE(std::holds_alternative<SelectCommand>(cmd));
    Query q = std::get<SelectCommand>(cmd).get_query();
    REQUIRE(q.get_type() == QueryType::PRIMITIVE);
    REQUIRE(q.as_value() == "test");
}

TEST_CASE("Test select OR", "[queryparser]") {
    Command cmd = parse_command("select \"test\" | \"cat\";");
    Query query = std::get<SelectCommand>(cmd).get_query();
    REQUIRE(query == q_or({q("test"), q("cat")}));
}

TEST_CASE("Test select AND", "[queryparser]") {
    Command cmd = parse_command("select \"test\" & \"cat\";");
    Query query = std::get<SelectCommand>(cmd).get_query();
    REQUIRE(query == q_and({q("test"), q("cat")}));
}

TEST_CASE("Test select operator order", "[queryparser]") {
    Command cmd = parse_command("select \"cat\" | \"dog\" & \"msm\" | \"monk\";");
    Query query = std::get<SelectCommand>(cmd).get_query();
    REQUIRE(query == q_or({q("cat"), q_and({q("dog"), q_or({q("msm"), q("monk")})})}));
}

TEST_CASE("Test compact command", "[queryparser]") {
    Command cmd = parse_command("compact;");
    REQUIRE(std::holds_alternative<CompactCommand>(cmd));
}

TEST_CASE("Test index command", "[queryparser]") {
    Command cmd = parse_command("index \"cat\";");
    IndexCommand index_cmd = std::get<IndexCommand>(cmd);
    REQUIRE(index_cmd.get_path() == "cat");
}

TEST_CASE("Test get_trigrams", "[gram3]") {
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

TEST_CASE("Test get_b64grams", "[text4]") {
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

TEST_CASE("Test get_h4grams", "[hash4]") {
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
            last_fid += rand() % 120;
        } else {
            last_fid += rand() % 100000;
        }

        fids.push_back(last_fid);
    }

    compress_run(fids, ss);

    std::string s = ss.str();
    const auto *ptr = (uint8_t *)s.data();

    std::vector<FileId> read_fids = read_compressed_run(ptr, ptr + s.length());

    REQUIRE(fids.size() == read_fids.size());

    for (int i = 0; i < fids.size(); i++) {
        REQUIRE(fids[i] == read_fids[i]);
    }
}

TEST_CASE("Compress run sanity", "[compress_run]") {
    std::stringstream ss;
    std::vector<FileId> fids = {1, 2, 5, 8, 256+8+1};
    compress_run(fids, ss);
    std::string s = ss.str();

    REQUIRE(s == "\x02\x01\x03\x03\x81\x02");
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

TEST_CASE("Test IndexBuilder for gram3", "[index_builder_gram3]") {
    std::string index_fname = "_test_idx_gram3.ursa";

    IndexBuilder builder(IndexType::GRAM3);
    add_test_payload(builder);
    builder.save(index_fname);

    OnDiskIndex ndx(index_fname);
    std::vector<FileId> res;

    REQUIRE(ndx.query_str("").is_everything());
    REQUIRE(ndx.query_str("a").is_everything());
    REQUIRE(ndx.query_str("ab").is_everything());

    res = ndx.query_str("kjhg").vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 1);

    res = ndx.query_str("\xA1\xA2\xA3").vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 2);

    res = ndx.query_str("m32\xA5X").vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    res = ndx.query_str("Xm32\xA5X").vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    res = ndx.query_str("Xm32\xA5s").vector();
    REQUIRE(res.size() == 0);

    res = ndx.query_str("Xbcdef").vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 4);

    res = ndx.query_str("\xA4\xA5\xA6\xA7").vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 2);
    REQUIRE(res[1] == 4);

    std::remove(index_fname.c_str());
}

TEST_CASE("Test IndexBuilder for text4", "[index_builder_text4]") {
    std::string index_fname = "_test_idx_text4.ursa";

    IndexBuilder builder(IndexType::TEXT4);
    add_test_payload(builder);
    builder.save(index_fname);

    OnDiskIndex ndx(index_fname);
    std::vector<FileId> res;

    REQUIRE(ndx.query_str("").is_everything());
    REQUIRE(ndx.query_str("a").is_everything());
    REQUIRE(ndx.query_str("ab").is_everything());
    REQUIRE(ndx.query_str("abc").is_everything());

    res = ndx.query_str("Xbcd").vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 4);

    res = ndx.query_str("Xbcdef").vector();
    REQUIRE(res.size() == 1);
    REQUIRE(res[0] == 4);

    res = ndx.query_str("m32\xA5X").vector();
    REQUIRE(res.size() == 0);

    res = ndx.query_str("Xm32\xA5X").vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    res = ndx.query_str("Xm32\xA5X").vector();
    REQUIRE(res.size() == 2);
    REQUIRE(res[0] == 4);
    REQUIRE(res[1] == 5);

    REQUIRE(ndx.query_str("\xA1\xA2\xA3").is_everything());
    REQUIRE(ndx.query_str("d\xA6\xA7").is_everything());
    REQUIRE(ndx.query_str("\xA4\xA5\xA6\xA7").is_everything());

    std::remove(index_fname.c_str());
}
