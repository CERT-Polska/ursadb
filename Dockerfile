FROM debian:buster AS build

RUN apt update \
    && apt install -y gcc-7 g++-7 libzmq3-dev cmake build-essential git

RUN mkdir src && mkdir src/build
COPY . src/
WORKDIR /src/build
RUN cmake -D CMAKE_CXX_COMPILER=/usr/bin/g++-7 -D CMAKE_BUILD_TYPE=Release .. && make -j$(nproc)

FROM debian:buster

COPY --from=build /src/build/ursadb /usr/bin/ursadb
COPY --from=build /src/build/ursadb_new /usr/bin/ursadb_new
COPY --from=build /src/build/ursadb_bench /usr/bin/ursadb_bench
COPY --from=build /src/build/ursadb_test /usr/bin/ursadb_test
COPY --from=build /src/build/ursacli /usr/bin/ursacli

COPY entrypoint.sh /entrypoint.sh

RUN mkdir /var/lib/ursadb \
    && apt update && apt install -y libzmq3-dev dumb-init \
    && chmod +x /entrypoint.sh /usr/bin/ursadb /usr/bin/ursadb_new \
    && useradd -u 1000 -d /var/lib/ursadb ursa \
    && chown -R ursa: /var/lib/ursadb /entrypoint.sh

USER ursa

EXPOSE 9281
VOLUME ["/var/lib/ursadb"]
WORKDIR /var/lib/ursadb
ENTRYPOINT ["/entrypoint.sh"]
CMD ["db.ursa", "tcp://0.0.0.0:9281"]

