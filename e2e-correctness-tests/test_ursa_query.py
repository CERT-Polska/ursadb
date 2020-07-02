import unittest
import zmq  # type: ignore
import json
import os
import logging

current_path = os.path.abspath(os.path.dirname(__file__))
testdir = current_path + "/yararules/"
filedir = current_path + "/testfiles/"


class TestUrsaQuery(unittest.TestCase):
    def test_ursa_query(self):
        ursa_query_files = [f for f in os.listdir(testdir) if ".txt" in f]

        assert ursa_query_files

        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        logging.info("Connecting to UrsaDB...")
        socket.connect("tcp://0.0.0.0:9281")
        logging.info("Connected...")
        socket.send_string('index "' + filedir + '" ' "with [gram3, text4, hash4, wide8];")
        assert json.loads(socket.recv_string()).get("result").get("status") == "ok"

        failures = []
        for query_file in ursa_query_files:
            logging.info("Query for: " + str(query_file))
            with open(testdir + query_file) as f:
                data = f.read()

            logging.info("Query: " + str(data))
            socket.send_string(data)

            resp = json.loads(socket.recv_string()).get("result").get("files")
            logging.info("Files: " + str(resp))

            if (query_file.startswith('negative')) and (len(resp) != 0):
                logging.info(
                    "Test failed for "
                    + str(query_file)
                    + ". "
                    + str(len(resp))
                    + " files found: "
                    + str(resp)
                )
                failures.append(query_file)
            elif (not query_file.startswith('negative')) and (
                (len(resp) != 1) or (query_file[:-4] not in resp[0])
            ):
                logging.info(
                    "Test failed for "
                    + str(query_file)
                    + ". "
                    + str(len(resp))
                    + " files found: "
                    + str(resp)
                )
                failures.append(query_file)

        assert len(failures) == 0


if __name__ == "__main__":
    unittest.main()
