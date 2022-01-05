FROM ubuntu:20.04
LABEL org.opencontainers.image.authors="support@halon.io"

ARG HALON_REPO_USER
ARG HALON_REPO_PASS

RUN apt-get update

RUN apt-get install -y wget gnupg
RUN apt-get install -y apt-transport-https

RUN wget -qO - https://raw.githubusercontent.com/halon/changelog/master/pgp-keys/7F0A73B5.asc | apt-key add -
RUN echo "deb https://repo.halon.io/ focal stable" >> /etc/apt/sources.list.d/halon.list
RUN echo "machine repo.halon.io login ${HALON_REPO_USER} password ${HALON_REPO_PASS}" >> /etc/apt/auth.conf
RUN apt-get update && apt-get install -y halon=5.8.0

RUN apt-get install -y build-essential cmake git libssl-dev curl

RUN mkdir /tmp/halon
RUN cd /tmp/halon \
    && curl -O -L https://github.com/Nordix/hiredis-cluster/archive/refs/tags/0.7.0.tar.gz \
    && tar -zxvf 0.7.0.tar.gz && cd hiredis-cluster-0.7.0 \
    && mkdir build && cd build && cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_SSL=ON -DDISABLE_TESTS=ON .. \
    && make && make install \
    && mkdir -p /libhiredis_cluster && cp ../COPYING /libhiredis_cluster/LICENSE
RUN cd /tmp/halon \
    && curl -O -L https://github.com/redis/hiredis/archive/refs/tags/v1.0.0.tar.gz \
    && tar -zxvf v1.0.0.tar.gz && cd hiredis-1.0.0 \
    && mkdir -p /hiredis && cp COPYING /hiredis/LICENSE
RUN rm -rf /tmp/halon

RUN echo -n "UBU2004" > /OSRELEASE.txt

COPY build.sh /build.sh
CMD ["/build.sh"]
