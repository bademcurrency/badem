FROM ubuntu:16.04

ENV BOOST_ROOT=/tmp/boost_install \
    BOOST_BASENAME=boost_1_66_0 \
    BOOST_URL=https://sourceforge.net/projects/boost/files/boost/1.66.0/boost_1_66_0.tar.gz/download

WORKDIR /

RUN apt-get update -qq && apt-get install -yqq \
    build-essential \
    cmake \
    g++ \
    git \
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

RUN git clone https://github.com/bademcurrency/badem.git badem \
    && cd badem \
    && git submodule init \
    && git submodule update \
    && git checkout numbers \
    && cmake -DBOOST_ROOT=${BOOST_ROOT} -G "Unix Makefiles" \
    && make badem_node \
    && mv badem_node / \
    && rm -rf badem

EXPOSE 2224 2224/udp 2225

CMD ["/badem_node", "--daemon"]