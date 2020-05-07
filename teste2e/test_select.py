from .util import UrsadbTestContext, store_files, check_query
from .util import ursadb  # noqa


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
