from .util import (
    UrsadbTestContext,
    store_files,
    check_query,
    get_index_hash,
    UrsadbConfig,
)
from .util import ursadb  # noqa
import os
import pytest  # type: ignore


def num_pattern(i: int) -> str:
    """ Tries to make an unique pattern that will not generate collisions """
    return f"_.:{i}:._([{i}])_!?{i}?!_"


def test_merge_sanity(ursadb: UrsadbTestContext) -> None:
    """ Don't merge more than merge_max_datasets at once """
    store_files(ursadb, "gram3", {"file1name": b"file1data"})
    store_files(ursadb, "gram3", {"file2name": b"file2data"})
    store_files(ursadb, "gram3", {"file3name": b"file3data"})

    ursadb.check_request("compact all;")
    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 1

    assert get_index_hash(ursadb, "gram3")[:16] == "963e51db376fa456"


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
        assert set(value) == set(response["result"]["files"])


def test_merge_compatible_types(ursadb: UrsadbTestContext):
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


def test_merge_incompatible_types(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"gram3": b"gram3"})
    store_files(ursadb, "text4", {"text4": b"text4"})

    check_query(ursadb, '"gram3"', ["gram3"])
    check_query(ursadb, '"text4"', ["text4"])

    ursadb.check_request("compact all;")

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2


def test_merge_incompatible_taints(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"gram3": b"gram3"})
    store_files(ursadb, "gram3", {"3marg": b"3marg"})

    topology = ursadb.check_request("topology;")
    dsnames = list(topology["result"]["datasets"].keys())
    for name in dsnames:
        ursadb.check_request(f'dataset "{name}" taint "{name}";')

    check_query(ursadb, '"gram3"', ["gram3"])
    check_query(ursadb, '"3marg"', ["3marg"])

    ursadb.check_request("compact all;")

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2


def test_merge_compatible_taints(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"gram3": b"gram3"})
    store_files(ursadb, "gram3", {"3marg": b"3marg"})

    topology = ursadb.check_request("topology;")
    dsnames = list(topology["result"]["datasets"].keys())
    for name in dsnames:
        ursadb.check_request(f'dataset "{name}" taint "xyz";')

    check_query(ursadb, '"gram3"', ["gram3"])
    check_query(ursadb, '"3marg"', ["3marg"])

    ursadb.check_request("compact all;")

    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 1


@pytest.mark.parametrize(
    "ursadb", [UrsadbConfig(merge_max_datasets=2)], indirect=["ursadb"],
)
def test_merge_max_datasets(ursadb: UrsadbTestContext) -> None:
    """ Don't merge more than merge_max_datasets at once """
    store_files(ursadb, "gram3", {"file1name": b"file1data"})
    store_files(ursadb, "gram3", {"file2name": b"file2data"})
    store_files(ursadb, "gram3", {"file3name": b"file3data"})

    ursadb.check_request("compact all;")
    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2


@pytest.mark.parametrize(
    "ursadb", [UrsadbConfig(merge_max_files=2)], indirect=["ursadb"],
)
def test_merge_max_files(ursadb: UrsadbTestContext) -> None:
    """ Don't merge more than merge_max_datasets at once """
    store_files(ursadb, "gram3", {"file1name": b"file1data"})
    store_files(ursadb, "gram3", {"file2name": b"file2data"})
    store_files(ursadb, "gram3", {"file3name": b"file3data"})

    ursadb.check_request("compact all;")
    topology = ursadb.check_request("topology;")
    assert len(topology["result"]["datasets"]) == 2
