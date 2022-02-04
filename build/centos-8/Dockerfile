FROM rockylinux/rockylinux:8
LABEL org.opencontainers.image.authors="support@halon.io"

ARG HALON_REPO_USER
ARG HALON_REPO_PASS

RUN yum update -y && yum install -y epel-release

RUN echo "[halon]" >> /etc/yum.repos.d/Halon.repo
RUN echo "name=Halon repository" >> /etc/yum.repos.d/Halon.repo
RUN echo "baseurl=https://${HALON_REPO_USER}:${HALON_REPO_PASS}@repo.halon.io/dists/rhel8/amd64/stable" >> /etc/yum.repos.d/Halon.repo
RUN echo "gpgcheck=1" >> /etc/yum.repos.d/Halon.repo
RUN echo "gpgkey=https://raw.githubusercontent.com/halon/changelog/master/pgp-keys/7F0A73B5.asc" >> /etc/yum.repos.d/Halon.repo
RUN echo "enabled=1" >> /etc/yum.repos.d/Halon.repo
RUN yum install -y halon-5.8.0

RUN yum install -y cmake3 make gcc-c++ rpm-build git openssl-devel

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

RUN echo -n "CENTOS8" > /OSRELEASE.txt

RUN yum install -y 'dnf-command(config-manager)'
RUN yum config-manager --set-enabled powertools

COPY build.sh /build.sh
CMD ["/build.sh"]
