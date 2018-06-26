FROM ubuntu:16.04

ARG NETWORK=live

ENV BOOST_ROOT=/tmp/boost_install

ADD ci /tmp/ci

RUN apt-get update -qq && apt-get install -yqq \
    build-essential \
    cmake \
    g++ \
    git \
    wget && \
    chmod +x /tmp/ci/bootstrap_boost.sh
    /tmp/ci/bootstrap_boost.sh -m

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