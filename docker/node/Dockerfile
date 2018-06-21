FROM ubuntu:16.04

RUN apt-get update -qq && apt-get install -yqq \
    build-essential \
    cmake \
    g++ \
    wget && \

ENV BOOST_URL=https://netix.dl.sourceforge.net/project/boost/boost/1.66.0/boost_1_66_0.tar.gz
    BOOST_ROOT=/tmp/boost_install \

WORKDIR /

RUN wget -O boost.tar.gz ${BOOST_URL} \
    && mkdir boost \
    && tar xzf boost.tar.gz -C boost --strip-components 1 \
    && rm boost.tar.gz \
    && cd boost \
    && ./bootstrap.sh \
    && ./b2 -d0 --prefix=${BOOST_ROOT} link=static install \
    && cd .. && rm -rf boost

ENV BADEM_BRANCH=master \
    BADEM_URL=https://github.com/bademcurrency/badem.git

RUN git clone ${BADEM_URL} badem \
    && cd badem \
    && git checkout ${BADEM_BRANCH} \
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