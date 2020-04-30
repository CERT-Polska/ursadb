"""Test stability of compacting in a highly multithreaded environment.

Especially, ensure that merge don't lose files, and there can't be two merge
operations at once.
"""

from util import UrsadbTestContext, store_files
from util import ursadb  # noqa
from multiprocessing import Pool
import zmq
import time


def f(db_url):
    context = zmq.Context()
    socket = context.socket(zmq.REQ)
    socket.setsockopt(zmq.LINGER, 0)
    socket.setsockopt(zmq.RCVTIMEO, 30000)
    socket.connect(db_url)
    socket.send_string(f"compact all;")
    return socket.recv_string()


def test_concurrent_merge(ursadb: UrsadbTestContext) -> None:
    """ Test what happens when we index multiple times at once """
    store_files(
        ursadb, "gram3", {"file1name": b"file1data"},
    )
    store_files(
        ursadb, "gram3", {"file2name": b"file2data"},
    )

    p = Pool(3)

    r = p.map_async(f, [ursadb.backend] * 3)

    compact_started = False
    for i in range(4):
        tasks = ursadb.check_request("status;")["result"]["tasks"]
        num_compact = len([t for t in tasks if "compact" in t["request"]])
        assert num_compact <= 1
        if num_compact == 1:
            compact_started = True
        if compact_started and num_compact == 0:
            break
        time.sleep(0.5)

    r.wait()

    ds = ursadb.check_request("topology;")["result"]["datasets"]
    assert len(ds) == 1

    files = ursadb.check_request("select {};")["result"]["files"]
    assert len(files) == 2


def test_merge_ratchet(ursadb: UrsadbTestContext) -> None:
    """Test that no files are lost when we index and merge at the same time"""
    socks = []
    for i in range(6):
        store_files(
            ursadb, "gram3", {f"file{i}name": b"filedata" + str(i).encode()},
        )
        time.sleep(0.2)
        socks.append(ursadb.start_request("compact all;"))
    for sock in socks:
        sock.recv_string()

    files = ursadb.check_request("select {};")["result"]["files"]
    assert len(files) == 6
    ds = ursadb.check_request("topology;")["result"]["datasets"]
    assert len(ds) < 5  # ideally 3, but that would require too much timing
