from util import UrsadbTestContext, store_files
from util import ursadb  # noqa


def test_pop(ursadb: UrsadbTestContext) -> None:
    store_files(
        ursadb,
        "gram3",
        {"file0": b"file0", "file1": b"file1", "file2": b"file2"},
    )
    response = ursadb.check_request(
        'select into iterator "file";', {"mode": "iterator"}
    )
    iterator = response["result"]["iterator"]

    ursadb.check_request(
        f'iterator "{iterator}" pop 1;',
        {"files": [], "iterator_position": 1, "mode": "raw", "total_files": 3},
    )
    ursadb.check_request(
        f'iterator "{iterator}" pop 1;',
        {"files": [], "iterator_position": 2, "mode": "raw", "total_files": 3},
    )
    ursadb.check_request(
        f'iterator "{iterator}" pop 1;',
        {"files": [], "iterator_position": 3, "mode": "raw", "total_files": 3},
    )
    result = ursadb.request(f'iterator "{iterator}" pop 1;')
    assert "error" in result
    assert result["error"]["retry"] is False
