from .util import UrsadbTestContext, store_files, check_query, UrsadbConfig
from .util import ursadb  # noqa
import pytest


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


@pytest.mark.parametrize(
    "ursadb",
    [UrsadbConfig(query_max_ngram=256*256, query_max_edge=255)],
    indirect=["ursadb"],
)
def test_select_with_wildcards(ursadb: UrsadbTestContext):
    store_files(
        ursadb, "gram3", {"first": b"first", "fiRst": b"fiRst", "second": b"second"},
    )

    check_query(ursadb, '"first"', ["first"])
    check_query(ursadb, '"fiRst"', ["fiRst"])
    check_query(ursadb, '"fi\\x??st"', ["first", "fiRst"])
    check_query(ursadb, '"fi\\x?2st"', ["first", "fiRst"])

    check_query(ursadb, '{66 69 72 73 74}', ["first"])
    check_query(ursadb, '{66 69 52 73 74}', ["fiRst"])
    check_query(ursadb, '{66 69 ?? 73 74}', ["first", "fiRst"])
    check_query(ursadb, '{66 69 ?2 73 74}', ["first", "fiRst"])



@pytest.mark.parametrize(
    "ursadb",
    [UrsadbConfig(query_max_ngram=16, query_max_edge=16)],
    indirect=["ursadb"],
)
def test_select_with_wildcards_with_limits(ursadb: UrsadbTestContext):
    store_files(
        ursadb, "gram3", {"first": b"first", "fiRst": b"fiRst", "second": b"second"},
    )

    check_query(ursadb, '"first"', ["first"])
    check_query(ursadb, '"fiRst"', ["fiRst"])
    check_query(ursadb, '"fi\\x??st"', ["first", "fiRst", "second"])
    check_query(ursadb, '"fi\\x?2st"', ["first", "fiRst"])
    check_query(ursadb, '"fi\\x?2s\\x??"', ["first", "fiRst"])

    check_query(ursadb, '{66 69 72 73 74}', ["first"])
    check_query(ursadb, '{66 69 52 73 74}', ["fiRst"])
    check_query(ursadb, '{66 69 ?? 73 74}', ["first", "fiRst", "second"])
    check_query(ursadb, '{66 69 ?2 73 74}', ["first", "fiRst"])
    check_query(ursadb, '{66 69 ?2 73 ??}', ["first", "fiRst"])
