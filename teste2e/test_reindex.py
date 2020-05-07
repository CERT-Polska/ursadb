from .util import UrsadbTestContext, store_files, check_query
from .util import ursadb  # noqa


def test_basic_reindex(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"kot": b":thinking:", "hmm": b"thingy"})

    datasets = ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 2,
                    "indexes": [{"type": "gram3", "size": "#UNK#"}],
                    "size": "#UNK#",
                    "taints": [],
                }
            }
        },
    )["result"]["datasets"]
    assert len(datasets) == 1
    dsname = list(datasets.keys())[0]

    check_query(ursadb, '"hing"', ["kot", "hmm"])

    ursadb.check_request(f'reindex "{dsname}" with [gram3, text4];')
    ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 2,
                    "indexes": [{"type": "gram3", "size": "#UNK#"}],
                    "size": "#UNK#",
                    "taints": [],
                }
            }
        },
    )

    check_query(ursadb, '"hing"', ["hmm"])


def test_reindex_taints(ursadb: UrsadbTestContext):
    store_files(ursadb, "gram3", {"kot": b":thinking:", "hmm": b"thingy"})

    datasets = ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 2,
                    "indexes": [{"type": "gram3", "size": "#UNK#"}],
                    "size": "#UNK#",
                    "taints": [],
                }
            }
        },
    )["result"]["datasets"]
    assert len(datasets) == 1
    dsname = list(datasets.keys())[0]

    ursadb.check_request(f'dataset "{dsname}" taint "xyz";')

    check_query(ursadb, '"hing"', ["kot", "hmm"])

    ursadb.check_request(f'reindex "{dsname}" with [gram3, text4];')

    ursadb.check_request(
        "topology;",
        {
            "datasets": {
                "#UNK#": {
                    "file_count": 2,
                    "indexes": [{"type": "gram3", "size": "#UNK#"}],
                    "size": "#UNK#",
                    "taints": [],
                }
            }
        },
    )
    check_query(ursadb, '"hing"', ["hmm"])
