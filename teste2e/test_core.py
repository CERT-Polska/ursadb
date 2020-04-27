from util import UrsadbTestContext, store_files
from util import ursadb  # noqa
from typing import List
import os
import hashlib


def check_query(ursadb: UrsadbTestContext, query: str, expected: List[str]):
    response = ursadb.check_request(f"select {query};")
    assert response["type"] == "select"
    assert response["result"]["mode"] == "raw"
    assert len(response["result"]["files"]) == len(expected)

    for fpath in response["result"]["files"]:
        assert any(fpath.endswith(f"/{fname}") for fname in expected)


def test_indexing_small(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"kot": b"Ala ma kota ale czy kot ma Ale?"})

    ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 1,
                    "indexes": [{"type": "gram3"}],
                    "taints": [],
                }
            }
        },
    )
    check_query(ursadb, '"ale"', ["kot"])
    check_query(ursadb, '":hmm:"', [])


def test_indexing_big(ursadb: UrsadbTestContext):
    store_files(
        ursadb,
        "gram3",
        {"kot": b"!" * 1024 * 1024 * 20 + b"ale bitmap index builder here!"},
    )

    # TODO: failing, because we create a second, empty dataset.
    # check_basic_topology(ursadb)
    check_query(ursadb, '"ale"', ["kot"])
    check_query(ursadb, '":hmm:"', [])


def get_index_hash(ursadb: UrsadbTestContext, type: str) -> str:
    """ Tries to find sha256 hash of the provided index """
    indexes = list(ursadb.ursadb_dir.glob(f"{type}*"))
    assert len(indexes) == 1
    return hashlib.sha256(indexes[0].read_bytes()).hexdigest()


def test_gram3_index_works_as_expected(ursadb: UrsadbTestContext):
    store_files(
        ursadb,
        "gram3",
        {
            "kot": b"aaaaabb   bbccccc",
            "zzz": b"aaaaabbccccc",
            "yyy": b"\xff\xff\xff",
        },
    )
    check_query(ursadb, '"abbc"', ["kot", "zzz"])
    check_query(ursadb, "{ff ff ff}", ["yyy"])
    assert get_index_hash(ursadb, "gram3")[:16] == "ca4a0662863a42b9"


def test_text4_index_works_as_expected(ursadb: UrsadbTestContext):
    store_files(
        ursadb,
        "text4",
        {
            "kot": b"aaaaabb   bbccccc",
            "zzz": b"aaaaabbccccc",
            "yyy": b"\xff\xff\xff",
        },
    )
    check_query(ursadb, '"abbc"', ["zzz"])
    check_query(ursadb, "{ff ff ff}", ["kot", "zzz", "yyy"])
    assert get_index_hash(ursadb, "text4")[:16] == "32078e5136ea7705"


def test_wide8_index_works_as_expected(ursadb: UrsadbTestContext):
    store_files(
        ursadb,
        "wide8",
        {
            "kot": b"aaaaabb   bbccccc",
            "zzz": b"aaaaabbccccc",
            "yyy": b"\xff\xff\xff",
            "vvv": b"a\x00b\x00c\x00d\x00efgh",
        },
    )
    check_query(ursadb, '"abbc"', ["kot", "zzz", "yyy", "vvv"])
    check_query(ursadb, "{ff ff ff}", ["kot", "zzz", "yyy", "vvv"])
    check_query(ursadb, '"a\\x00b\\x00c\\x00d\\x00"', ["vvv"])
    assert get_index_hash(ursadb, "wide8")[:16] == "16f473f632b106a6"


def num_pattern(i: int) -> str:
    """ Tries to make an unique pattern that will not generate collisions """
    return f"_.:{i}:._([{i}])_!?{i}?!_"


def test_large_index_spill_and_merge(ursadb: UrsadbTestContext):
    store_files(
        ursadb,
        "gram3",
        {
            str(i): num_pattern(i).encode() + os.urandom(1000000)
            for i in range(100)
        },
    )

    for i in range(100):
        check_query(ursadb, f'"{num_pattern(i)}"', [str(i)])

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) > 1

    expected = {}
    for i in range(256):
        ngram = os.urandom(3).hex()
        response = ursadb.check_request(f"select {{ {ngram} }};")
        expected[ngram] = response["result"]["files"]

    ursadb.check_request("compact all;")

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 1

    for i in range(100):
        check_query(ursadb, f'"{num_pattern(i)}"', [str(i)])

    for ngram, value in expected.items():
        response = ursadb.check_request(f"select {{ {ngram} }};")
        assert value == response["result"]["files"]


def test_incompatible_merge(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"gram3": b"gram3"})
    store_files(ursadb, "text4", {"text4": b"text4"})

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2

    check_query(ursadb, '"gram3"', ["gram3"])
    check_query(ursadb, '"text4"', ["text4"])

    # TODO(#88) request instead of check_request before #88 is merged
    ursadb.request("compact all;")

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2


def test_compatible_merge(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"gram3": b"gram3"})
    store_files(ursadb, "gram3", {"3marg": b"3marg"})

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2

    check_query(ursadb, '"gram3"', ["gram3"])
    check_query(ursadb, '"3marg"', ["3marg"])

    ursadb.check_request("compact all;")

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 1

    check_query(ursadb, '"gram3"', ["gram3"])
    check_query(ursadb, '"3marg"', ["3marg"])

    assert get_index_hash(ursadb, "gram3")[:16] == "7f6499ef40129703"