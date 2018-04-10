#!/bin/bash

make
echo "test file 1: monk" > file1.test
echo "test file 2: msm" > file2.test
echo "test file 3: ursadb" > file3.test
./ursadb index testdb.ursa *.test
echo -n "mon "; ./ursadb query testdb.ursa mon
echo -n "msm "; ./ursadb query testdb.ursa msm
echo -n "tes "; ./ursadb query testdb.ursa tes
md5sum testdb.ursa
