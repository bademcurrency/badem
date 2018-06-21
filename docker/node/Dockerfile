FROM ubuntu:16.04

ARG NETWORK=live

ENV BOOST_ROOT=/tmp/boost_install \
    BOOST_BASENAME=boost_1_66_0 \
    BOOST_URL=https://sourceforge.net/projects/boost/files/boost/1.66.0/boost_1_66_0.tar.gz/download

ADD ci /tmp/ci

WORKDIR "/tmp"

RUN apt-get update -qq && apt-get install -yqq \
    build-essential \
    cmake \
    g++ \
    wget && \
    wget -qO ${BOOST_BASENAME}.tar.gz ${BOOST_URL} && \
    tar xzf ${BOOST_BASENAME}.tar.gz && \
    cd ${BOOST_BASENAME} && \
    ./bootstrap.sh && \
    ./b2 -d0 --prefix=${BOOST_ROOT} link=static install && \
    rm -rf ${BOOST_BASENAME} && \
    rm -f ${BOOST_BASENAME}.tar.gz && \
    cd .. && \
    mkdir /usr/share/badem/

ADD ./ /tmp/src

RUN mkdir /tmp/build && \
    cd /tmp/build && \
    cmake /tmp/src -DBOOST_ROOT=${BOOST_ROOT} -DACTIVE_NETWORK=badem_${NETWORK}_network && \
    make badem_node && \
    cd .. && \
    echo ${NETWORK} > /etc/badem-network

FROM ubuntu:16.04
COPY --from=0 /tmp/build/badem_node /usr/bin
COPY --from=0 /etc/badem-network /etc
COPY docker/node/entry.sh /entry.sh
COPY docker/node/config /usr/share/badem/config
RUN chmod +x /entry.sh
CMD ["/bin/bash",  "/entry.sh"]