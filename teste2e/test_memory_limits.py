"""Test stability of the database in environments with constrained memory.

Since all environments have some kind of memory constraint, it sounds like a
reasonable thing to test.
"""

from util import UrsadbTestContext, store_files, UrsadbConfig
from util import ursadb  # noqa
import zmq
import pytest


def test_no_oom(ursadb: UrsadbTestContext) -> None:
    """ No OOM when there are no limits (as a sanity test) """
    store_files(
        ursadb, "gram3", {"file1name": b"file1data"},
    )
    store_files(
        ursadb, "gram3", {"file2name": b"file2data"},
    )

    res = ursadb.request("compact all;")
    assert "error" not in res


@pytest.mark.parametrize(
    "ursadb",
    [UrsadbConfig(rlimit_ram=1 * 1024 * 1024 * 1024)],
    indirect=["ursadb"],
)
def test_oom_no_crash(ursadb: UrsadbTestContext) -> None:
    """ 1GiB is not enough for indexing and compacting """
    store_files(ursadb, "gram3", {"file1name": b"file1data"}, expect_error=True)
    store_files(ursadb, "gram3", {"file2name": b"file2data"}, expect_error=True)

    res = ursadb.request("compact all;")
    assert "error" in res


@pytest.mark.parametrize(
    "ursadb",
    [UrsadbConfig(rlimit_ram=4 * 1024 * 1024 * 1024)],
    indirect=["ursadb"],
)
def test_no_oom_with_4gb(ursadb: UrsadbTestContext) -> None:
    """ 4 GiB is the official minimal required memory (at least currently) """
    store_files(ursadb, "gram3", {"file1name": b"file1data"})
    store_files(ursadb, "gram3", {"file2name": b"file2data"})

    res = ursadb.request("compact all;")
    assert "error" not in res
