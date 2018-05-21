FROM debian:buster AS build

RUN apt update && apt install -y gcc-7 libzmq3-dev cmake build-essential

RUN mkdir src && mkdir src/build
COPY . src/
WORKDIR /src/build
RUN cmake -D CMAKE_C_COMPILER=gcc-7 -D CMAKE_CXX_COMPILER=g++-7 -D CMAKE_BUILD_TYPE=Release .. && make
RUN chmod +x /src/build/ursadb_test && /src/build/ursadb_test

FROM debian:buster

COPY --from=build /src/build/ursadb /usr/bin/ursadb
COPY --from=build /src/build/ursadb_new /usr/bin/ursadb_new
COPY --from=build /src/build/ursadb_bench /usr/bin/ursadb_bench
COPY --from=build /src/build/ursadb_test /usr/bin/ursadb_test

RUN mkdir /var/lib/ursadb
RUN apt update && apt install -y libzmq3-dev dumb-init
RUN chmod +x /usr/bin/ursadb
RUN rm -f /var/lib/ursadb/db.ursa
RUN /usr/bin/ursadb_new /var/lib/ursadb/db.ursa

EXPOSE 9281
VOLUME ["/var/lib/ursadb"]
WORKDIR /var/lib/ursadb
ENTRYPOINT ["/usr/bin/dumb-init", "--", "/usr/bin/ursadb"]
CMD ["db.ursa", "tcp://0.0.0.0:9281"]
