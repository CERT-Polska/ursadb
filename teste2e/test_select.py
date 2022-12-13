from .util import UrsadbTestContext, store_files, check_query, UrsadbConfig
from .util import ursadb  # noqa
import pytest


def test_select_with_taints(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"tainted": b"test"})

    topology = ursadb.check_request("topology;")
    dsname = list(topology["result"]["datasets"].keys())[0]

    store_files(ursadb, "gram3", {"untainted": b"test"})

    ursadb.check_request(f'dataset "{dsname}" taint "test";')

    check_query(ursadb, 'with taints [] "test"', ["tainted", "untainted"])
    check_query(ursadb, 'with taints ["test"] "test"', ["tainted"])
    check_query(ursadb, 'with taints ["other"] "test"', [])
    check_query(ursadb, 'with taints ["test", "other"] "test"', [])


def test_select_with_weird_filenames(ursadb: UrsadbTestContext):
    weird_names = [
        "hmm hmm",
        "hmm \" ' hmm",
        "hmm \\ hmm",
        "hmm $(ls) $$ $shell hmm",
        "hmm <> hmm",
    ]
    store_files(ursadb, "gram3", {name: b"test" for name in weird_names})

    check_query(ursadb, '"test"', weird_names)


def test_select_with_datasets(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"first": b"test"})

    topology = ursadb.check_request("topology;")
    dsname = list(topology["result"]["datasets"].keys())[0]

    store_files(ursadb, "gram3", {"second": b"test"})

    check_query(ursadb, f'with datasets ["{dsname}"] "test"', ["first"])


def test_select_minof(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"aaaa": b"aaaa", "aaab": b"aaab", "aaac": b"aaac"})

    check_query(ursadb, f'min 2 of ("aaa", "aab")', ["aaab"])


def test_select_ascii_wide(ursadb: UrsadbTestContext):
    # ensure that `ascii wide` strings don't produce too large results
    store_files(
        ursadb,
        ["gram3", "text4", "wide8"],
        {
            "ascii": b"koty",
            "wide": b"k\x00o\x00t\x00y\x00",
            "falsepositive": b"k\x00o\x00???\x00o\x00t\x00y\x00",
        },
    )

    check_query(ursadb, f'"koty"', ["ascii"])
    check_query(ursadb, f'"k\\x00o\\x00t\\x00y\\x00"', ["wide"])
    check_query(ursadb, f'("koty" | "k\\x00o\\x00t\\x00y\\x00")', ["ascii", "wide"])


@pytest.mark.parametrize(
    "ursadb",
    [UrsadbConfig(query_max_ngram=256 * 256, query_max_edge=255)],
    indirect=["ursadb"],
)
def test_select_with_wildcards(ursadb: UrsadbTestContext):
    store_files(
        ursadb, "gram3", {"first": b"first", "fiRst": b"fiRst", "second": b"second"}
    )

    check_query(ursadb, '"first"', ["first"])
    check_query(ursadb, '"fiRst"', ["fiRst"])
    check_query(ursadb, '"fi\\x??st"', ["first", "fiRst", "second"])
    check_query(ursadb, '"fi\\x?2st"', ["first", "fiRst", "second"])

    check_query(ursadb, "{66 69 72 73 74}", ["first"])
    check_query(ursadb, "{66 69 52 73 74}", ["fiRst"])
    check_query(ursadb, "{66 69 ?? 73 74}", ["first", "fiRst", "second"])
    check_query(ursadb, "{66 69 ?2 73 74}", ["first", "fiRst", "second"])


@pytest.mark.parametrize(
    "ursadb", [UrsadbConfig(query_max_ngram=16, query_max_edge=16)], indirect=["ursadb"]
)
def test_select_with_wildcards_with_limits(ursadb: UrsadbTestContext):
    store_files(
        ursadb, "gram3", {"first": b"first", "fiRst": b"fiRst", "second": b"second"}
    )

    check_query(ursadb, '"first"', ["first"])
    check_query(ursadb, '"fiRst"', ["fiRst"])
    check_query(ursadb, '"fi\\x??st"', ["first", "fiRst", "second"])
    check_query(ursadb, '"fi\\x?2st"', ["first", "fiRst", "second"])
    check_query(ursadb, '"fi\\x?2s\\x??"', ["first", "fiRst"])

    check_query(ursadb, "{66 69 72 73 74}", ["first"])
    check_query(ursadb, "{66 69 52 73 74}", ["fiRst"])
    check_query(ursadb, "{66 69 ?? 73 74}", ["first", "fiRst", "second"])
    check_query(ursadb, "{66 69 ?2 73 74}", ["first", "fiRst", "second"])
    check_query(ursadb, "{66 69 ?2 73 ??}", ["first", "fiRst", "second"])
