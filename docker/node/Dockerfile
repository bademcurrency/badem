FROM ubuntu:16.04

ENV BOOST_ROOT=/tmp/boost_install \
    BOOST_BASENAME=boost_1_66_0 \
    BOOST_URL=https://sourceforge.net/projects/boost/files/boost/1.66.0/boost_1_66_0.tar.gz/download

WORKDIR /

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

RUN git clone https://github.com/bademcurrency/badem.git badem \
    && cd badem \
    && git checkout master \
    && git submodule init \
    && git submodule update \
    && mkdir build \
    && cd build \
    && cmake -DBOOST_ROOT=${BOOST_ROOT}-G "Unix Makefiles" .. \
    && make badem_node \
    && mv badem_node / \
    && rm -rf badem

EXPOSE 7075 7075/udp 7076

CMD ["/badem_node", "--daemon"]