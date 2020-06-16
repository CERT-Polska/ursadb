import unittest
import zmq  # type: ignore
import json
import os
import logging

current_path = os.path.abspath(os.path.dirname(__file__))
testdir = current_path + "/testdata/"


class TestUrsaQuery(unittest.TestCase):
    def test_ursa_query(self):

        ursa_query_files = [f for f in os.listdir(testdir)]

        context = zmq.Context()
        socket = context.socket(zmq.REQ)
        logging.info("Connecting...")
        socket.connect("tcp://0.0.0.0:9281")
        logging.info("Connected...")
        socket.send_string(
            'index "/opt/samples" with [gram3, text4, hash4, wide8];'
        )

        assert json.loads(socket.recv_string()).get("result").get("status") == "ok"
        logging.info("Files indexed.")

        for file in ursa_query_files:
            with open(testdir + file) as f:
                data = f.read()

            socket.send_string(
                data
            )
            logging.info("Query made.")
            logging.info(data)
            resp = json.loads(socket.recv_string()).get("result").get("files")
            logging.info(resp)
            assert len(resp) == 1
            assert file in resp[0]


if __name__ == "__main__":
    unittest.main()
