FROM debian:stable-slim

WORKDIR /data

RUN echo "\e[33mThis will take some time. Go and get a cup of coffee. We could be here all night.\e[39m"

#su-exec
ARG SUEXEC_VERSION=v0.2
ARG SUEXEC_HASH=f85e5bde1afef399021fbc2a99c837cf851ceafa
#Cmake
ARG CMAKE_VERSION=3.14.0
ARG CMAKE_VERSION_DOT=v3.14
ARG CMAKE_HASH=aa76ba67b3c2af1946701f847073f4652af5cbd9f141f221c97af99127e75502
## Boost
ARG BOOST_VERSION=1_69_0
ARG BOOST_VERSION_DOT=1.69.0
ARG BOOST_HASH=8f32d4617390d1c2d16f26a27ab60d97807b35440d45891fa340fc2648b04406

ENV CFLAGS '-fPIC -O2 -g'
ENV CXXFLAGS '-fPIC -O2 -g'
ENV LDFLAGS '-static-libstdc++'

ENV BASE_DIR /usr/local

RUN apt-get update -qq && apt-get --no-install-recommends -yqq install \
        ca-certificates \
        g++ \
        make \
        pkg-config \
        git \
        curl \
        libtool-bin \
        autoconf \
        automake \
        bzip2 \
        xsltproc \
        gperf \
        unzip > /dev/null \
        g++-aarch64-linux-gnu \
        apt-utils \
    && cd /data || exit 1 \
    && echo "\e[32mbuilding: su-exec\e[39m" \
    && git clone --branch ${SUEXEC_VERSION} --single-branch --depth 1 https://github.com/ncopa/su-exec.git su-exec.git > /dev/null \
    && cd su-exec.git || exit 1 \
    && test `git rev-parse HEAD` = ${SUEXEC_HASH} || exit 1 \
    && make > /dev/null \
    && cp su-exec /data \
    && cd /data || exit 1 \
    && rm -rf /data/su-exec.git \
    && echo "\e[32mbuilding: Cmake\e[39m" \
    && set -ex \
    && curl -s -O https://cmake.org/files/${CMAKE_VERSION_DOT}/cmake-${CMAKE_VERSION}.tar.gz > /dev/null \
    && echo "${CMAKE_HASH}  cmake-${CMAKE_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf cmake-${CMAKE_VERSION}.tar.gz > /dev/null \
    && cd cmake-${CMAKE_VERSION} || exit 1 \
    && ./configure --prefix=$BASE_DIR > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/cmake-${CMAKE_VERSION} \
    && rm -rf /data/cmake-${CMAKE_VERSION}.tar.gz \
    && echo "\e[32mbuilding: Boost\e[39m" \
    && set -ex \
    && curl -s -L -o  boost_${BOOST_VERSION}.tar.bz2 https://dl.bintray.com/boostorg/release/${BOOST_VERSION_DOT}/source/boost_${BOOST_VERSION}.tar.bz2 > /dev/null \
    && echo "${BOOST_HASH}  boost_${BOOST_VERSION}.tar.bz2" | sha256sum -c \
    && tar -xvf boost_${BOOST_VERSION}.tar.bz2 > /dev/null \
    && cd boost_${BOOST_VERSION} || exit 1 \
    && ./bootstrap.sh > /dev/null \
    && ./b2 -a install --prefix=$BASE_DIR --build-type=minimal link=static runtime-link=static --with-chrono --with-date_time --with-filesystem --with-program_options --with-regex --with-serialization --with-system --with-thread --with-locale threading=multi threadapi=pthread cflags="$CFLAGS" cxxflags="$CXXFLAGS" stage > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/boost_${BOOST_VERSION} \
    && rm -rf /data/boost_${BOOST_VERSION}.tar.bz2

RUN echo "\e[33mDid I ever tell you the story of when my cousin Nicolas boost a lambo in less than 60 seconds?\e[39m"

WORKDIR /data

ENV BASE_DIR /usr/local

# OpenSSL
ARG OPENSSL_VERSION=1.1.1b
ARG OPENSSL_HASH=5c557b023230413dfb0756f3137a13e6d726838ccd1430888ad15bfb2b43ea4b
# ZMQ
ARG ZMQ_VERSION=v4.3.1
ARG ZMQ_HASH=2cb1240db64ce1ea299e00474c646a2453a8435b
# zmq.hpp
ARG CPPZMQ_VERSION=v4.3.0
ARG CPPZMQ_HASH=213da0b04ae3b4d846c9abc46bab87f86bfb9cf4
# Readline
ARG READLINE_VERSION=8.0
ARG READLINE_HASH=e339f51971478d369f8a053a330a190781acb9864cf4c541060f12078948e461
# Sodium
ARG SODIUM_VERSION=1.0.17
ARG SODIUM_HASH=b732443c442239c2e0184820e9b23cca0de0828c

ENV CFLAGS '-fPIC -O2 -g'
ENV CXXFLAGS '-fPIC -O2 -g'
ENV LDFLAGS '-static-libstdc++'

RUN echo "\e[32mbuilding: Openssl\e[39m" \
    && set -ex \
    && curl -s -O https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz > /dev/null \
    && echo "${OPENSSL_HASH}  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf openssl-${OPENSSL_VERSION}.tar.gz > /dev/null \
    && cd openssl-${OPENSSL_VERSION} || exit 1 \
    && ./Configure --prefix=$BASE_DIR linux-x86_64 no-shared --static "$CFLAGS" > /dev/null \
    && make build_generated > /dev/null \
    && make libcrypto.a > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/openssl-${OPENSSL_VERSION} \
    && rm -rf /data/openssl-${OPENSSL_VERSION}.tar.gz \
    && echo "\e[32mbuilding: ZMQ\e[39m" \
    && set -ex \
    && git clone --branch ${ZMQ_VERSION} --single-branch --depth 1 https://github.com/zeromq/libzmq.git > /dev/null \
    && cd libzmq || exit 1 \
    && test `git rev-parse HEAD` = ${ZMQ_HASH} || exit 1 \
    && ./autogen.sh > /dev/null \
    && ./configure --prefix=$BASE_DIR --enable-libunwind=no --enable-static --disable-shared > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && ldconfig > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/libzmq \
    && echo "\e[32mbuilding: zmq.hpp\e[39m" \
    && set -ex \
    && git clone --branch ${CPPZMQ_VERSION} --single-branch --depth 1 https://github.com/zeromq/cppzmq.git > /dev/null \
    && cd cppzmq || exit 1 \
    && test `git rev-parse HEAD` = ${CPPZMQ_HASH} || exit 1 \
    && mv *.hpp $BASE_DIR/include \
    && cd /data || exit 1 \
    && rm -rf /data/cppzmq \
    && echo "\e[32mbuilding: Readline\e[39m" \
    && set -ex \
    && curl -s -O https://ftp.gnu.org/gnu/readline/readline-${READLINE_VERSION}.tar.gz > /dev/null \
    && echo "${READLINE_HASH}  readline-${READLINE_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf readline-${READLINE_VERSION}.tar.gz > /dev/null \
    && cd readline-${READLINE_VERSION} || exit 1 \
    && ./configure --prefix=$BASE_DIR > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/readline-${READLINE_VERSION} \
    && rm -rf readline-${READLINE_VERSION}.tar.gz \
    && echo "\e[32mbuilding: Sodium\e[39m" \
    && set -ex \
    && git clone --branch ${SODIUM_VERSION} --single-branch --depth 1 https://github.com/jedisct1/libsodium.git > /dev/null \
    && cd libsodium || exit 1 \
    && test `git rev-parse HEAD` = ${SODIUM_HASH} || exit 1 \
    && ./autogen.sh \
    && ./configure --prefix=$BASE_DIR > /dev/null \
    && make > /dev/null \
    && make check > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/libsodium

RUN echo "\e[33mOoh wee, that was close. I thought I broke something.\e[39m"

WORKDIR /data

ENV BASE_DIR /usr/local

# Udev
ARG UDEV_VERSION=v3.2.7
ARG UDEV_HASH=4758e346a14126fc3a964de5831e411c27ebe487
# Libusb
ARG USB_VERSION=v1.0.22
ARG USB_HASH=0034b2afdcdb1614e78edaa2a9e22d5936aeae5d
# Hidapi
ARG HIDAPI_VERSION=hidapi-0.8.0-rc1
ARG HIDAPI_HASH=40cf516139b5b61e30d9403a48db23d8f915f52c
# Protobuf
ARG PROTOBUF_VERSION=v3.7.0
ARG PROTOBUF_HASH=582743bf40c5d3639a70f98f183914a2c0cd0680

ENV CFLAGS '-fPIC -O2 -g'
ENV CXXFLAGS '-fPIC -O2 -g'
ENV LDFLAGS '-static-libstdc++'

RUN echo "\e[32mbuilding: Udev\e[39m" \
    && set -ex \
    && git clone --branch ${UDEV_VERSION} --single-branch --depth 1 https://github.com/gentoo/eudev > /dev/null \
    && cd eudev || exit 1 \
    && test `git rev-parse HEAD` = ${UDEV_HASH} || exit 1 \
    && ./autogen.sh \
    && ./configure --prefix=$BASE_DIR --disable-gudev --disable-introspection --disable-hwdb --disable-manpages --disable-shared > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/eudev \
    && echo "\e[32mbuilding: Libusb\e[39m" \
    && set -ex \
    && git clone --branch ${USB_VERSION} --single-branch --depth 1 https://github.com/libusb/libusb.git > /dev/null \
    && cd libusb || exit 1 \
    && test `git rev-parse HEAD` = ${USB_HASH} || exit 1 \
    && ./autogen.sh > /dev/null \
    && ./configure --prefix=$BASE_DIR --disable-shared > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/libusb \
    && echo "\e[32mbuilding: Hidapi\e[39m" \
    && set -ex \
    && git clone --branch ${HIDAPI_VERSION} --single-branch --depth 1 https://github.com/signal11/hidapi > /dev/null \
    && cd hidapi || exit 1 \
    && test `git rev-parse HEAD` = ${HIDAPI_HASH} || exit 1 \
    && ./bootstrap \
    && ./configure --prefix=$BASE_DIR --enable-static --disable-shared > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && cd /data || exit 1 \
    && rm -rf /data/hidapi \
    && echo "\e[32mbuilding: Protobuf\e[39m" \
    && set -ex \
    && git clone --branch ${PROTOBUF_VERSION}  --single-branch --depth 1 https://github.com/protocolbuffers/protobuf > /dev/null \
    && cd protobuf || exit 1 \
    && test `git rev-parse HEAD` = ${PROTOBUF_HASH} || exit 1 \
    && git submodule update --init --recursive > /dev/null \
    && ./autogen.sh > /dev/null \
    && ./configure --prefix=$BASE_DIR --enable-static --disable-shared > /dev/null \
    && make > /dev/null \
    && make install > /dev/null \
    && ldconfig \
    && cd /data || exit 1 \
    && rm -rf /data/protobuf

WORKDIR /data
# BUILD_PATH:
# Using 'USE_SINGLE_BUILDDIR=1 make' creates a unified build dir (/wownero.git/build/release/bin)

ARG PROJECT_URL=https://github.com/wownero/wownero.git
ARG BRANCH=master
ARG BUILD_PATH=/wownero.git/build/release/bin

ENV CFLAGS '-fPIC -O1'
ENV CXXFLAGS '-fPIC -O1'
ENV LDFLAGS '-static-libstdc++'

RUN echo "\e[33mNow we're getting somewhere.\e[39m"

RUN echo "\e[32mcloning: $PROJECT_URL on branch: $BRANCH\e[39m" \
    && git clone --branch "$BRANCH" --single-branch --recursive $PROJECT_URL wownero.git > /dev/null \
    && cd wownero.git || exit 1 \
    && echo "\e[32mbuilding static binaries\e[39m" \
    && apt-get update -qq && apt-get install -yqq --no-install-recommends \
        libreadline-dev \
    && USE_SINGLE_BUILDDIR=1 make release-static > /dev/null \
    && echo "\e[32mcopy and clean up\e[39m" \
    && mv /data$BUILD_PATH/wownerod /data/ \
    && chmod +x /data/wownerod \
    && mv /data$BUILD_PATH/wownero-wallet-rpc /data/ \
    && chmod +x /data/wownero-wallet-rpc \
    && mv /data$BUILD_PATH/wownero-wallet-cli /data/ \
    && chmod +x /data/wownero-wallet-cli \
    && cp /data/wownerod /usr/local/bin/wownerod \
    && cp /data/wownero-wallet-rpc /usr/local/bin/wownero-wallet-rpc \
    && cp /data/wownero-wallet-cli /usr/local/bin/wownero-wallet-cli \
    && cp /data/su-exec /usr/local/bin/su-exec \
    && rm /data/su-exec \
    && cd /data || exit 1 \
    && apt-get autoremove --purge -yqq > /dev/null \
    && apt-get clean > /dev/null \
    && rm -rf /var/tmp/* /tmp/* /var/lib/apt/* > /dev/null

VOLUME ["/data"]

ENV USER_ID 1000
ENV LOG_LEVEL 0
ENV DAEMON_HOST 127.0.0.1
ENV DAEMON_PORT 34568
ENV RPC_USER ""
ENV RPC_PASSWD ""
ENV RPC_LOGIN ""
ENV WALLET_PASSWD ""
ENV WALLET_ACCESS ""
ENV RPC_BIND_IP 0.0.0.0
ENV RPC_BIND_PORT 34568
ENV P2P_BIND_IP 0.0.0.0
ENV P2P_BIND_PORT 34567

RUN wownerod --version
RUN echo "\e[33mAll done.\e[39m"
