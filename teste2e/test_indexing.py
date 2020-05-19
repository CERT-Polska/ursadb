from .util import UrsadbTestContext, store_files, check_query, get_index_hash, UrsadbConfig
from .util import ursadb  # noqa
import pytest


def test_indexing_small(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"kot": b"Ala ma kota ale czy kot ma Ale?"})

    ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 1,
                    "indexes": [{"type": "gram3", "size": "#UNK#"}],
                    "size": "#UNK#",
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

    check_query(ursadb, '"ale"', ["kot"])
    check_query(ursadb, '":hmm:"', [])


def test_indexing_list(ursadb: UrsadbTestContext):
    tmpdir = ursadb.tmpdir()
    (tmpdir / "test").mkdir()
    (tmpdir / "test" / "file").write_bytes(b"asdfgh")

    (tmpdir / "list.txt").write_text(str(tmpdir / "test"))

    ursadb.check_request(f"index from list \"{str(tmpdir / 'list.txt')}\";")
    check_query(ursadb, '"asdfgh"', ["file"])


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


@pytest.mark.parametrize(
    "ursadb",
    [UrsadbConfig(query_max_ngram=256)],
    indirect=["ursadb"],
)
def test_wide8_index_works_as_expected(ursadb: UrsadbTestContext):
    store_files(
        ursadb,
        "wide8",
        {
            "kot": b"aaaaabb   bbccccc",
            "zzz": b"aaaaabbccccc",
            "yyy": b"\xff\xff\xff",
            "vvv": b"a\x00b\x00c\x00d\x00efgh",
            "qqq": b"a\x00c\x00b\x00d\x00efgh",
        },
    )
    check_query(ursadb, '"abbc"', ["kot", "zzz", "yyy", "vvv", "qqq"])
    check_query(ursadb, "{ff ff ff}", ["kot", "zzz", "yyy", "vvv", "qqq"])
    check_query(ursadb, '"a\\x00b\\x00c\\x00d\\x00"', ["vvv"])
    check_query(
        ursadb,
        "{61 (00|01) (62|63) (00|01) (63|62) (00|01) 64 00}",
        ["vvv", "qqq"],
    )
    assert get_index_hash(ursadb, "wide8")[:16] == "c73b55c36445ca6b"


def test_select_with_taints(ursadb: UrsadbTestContext):
    store_files(
        ursadb, "gram3", {"tainted": b"test",},
    )

    topology = ursadb.check_request("topology;")
    dsname = list(topology["result"]["datasets"].keys())[0]

    store_files(
        ursadb, "gram3", {"untainted": b"test",},
    )

    ursadb.check_request(f'dataset "{dsname}" taint "test";')

    check_query(ursadb, 'with taints [] "test"', ["tainted", "untainted"])
    check_query(ursadb, 'with taints ["test"] "test"', ["tainted"])
    check_query(ursadb, 'with taints ["other"] "test"', [])
    check_query(ursadb, 'with taints ["test", "other"] "test"', [])


def test_select_with_datasets(ursadb: UrsadbTestContext):
    store_files(
        ursadb, "gram3", {"first": b"test",},
    )

    topology = ursadb.check_request("topology;")
    dsname = list(topology["result"]["datasets"].keys())[0]

    store_files(
        ursadb, "gram3", {"second": b"test",},
    )

    check_query(ursadb, f'with datasets ["{dsname}"] "test"', ["first"])


def test_index_with_taints(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"kot": b"Random file"}, taints=["taint"])

    ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 1,
                    "indexes": [{"type": "gram3", "size": "#UNK#"}],
                    "size": "#UNK#",
                    "taints": ["taint"],
                }
            }
        },
    )
    check_query(ursadb, '"file"', ["kot"])
    check_query(ursadb, 'with taints ["taint"] "file"', ["kot"])
    check_query(ursadb, 'with taints ["zzz"] "file"', [])

