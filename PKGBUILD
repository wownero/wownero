# Maintainer: wowario <wowario[at]protonmail[dot]com>

pkgname=wownero-git
pkgver=0.8.0.0
pkgrel=1
pkgdesc="Wownero: a fairly launched privacy-centric meme coin with no premine and a finite supply"
license=('BSD')
arch=('x86_64')
url="https://wownero.org/"
depends=('boost-libs' 'libunwind' 'openssl' 'readline' 'zeromq' 'pcsclite' 'hidapi' 'protobuf')
makedepends=('git' 'cmake' 'boost')
source=(
    "${pkgname}"::"git+https://github.com/wownero/wownero#tag=v${pkgver}"
    "git+https://github.com/monero-project/unbound.git"
    "git+https://github.com/monero-project/miniupnp.git"
    "git+https://github.com/Tencent/rapidjson.git"
    "git+https://github.com/trezor/trezor-common.git"
    "git+https://github.com/wownero/RandomWOW.git"
    "wownero.sysusers"
    "wownero.tmpfiles")
sha512sums=('SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP'
            'SKIP')

prepare() {
  cd "${pkgname}"
  git submodule init
  git config submodule.external/unbound.url "$srcdir/unbound"
  git config submodule.external/miniupnp.url "$srcdir/miniupnp"
  git config submodule.external/rapidjson.url "$srcdir/rapidjson"
  git config submodule.external/trezor-common.url "$srcdir/trezor-common"
  git config submodule.external/RandomWOW.url "$srcdir/RandomWOW"
  git submodule update
}

build() {
  cd "${pkgname}"
  mkdir -p build && cd build
  cmake -D BUILD_TESTS=OFF -D CMAKE_BUILD_TYPE=release -D ARCH=default ../
  make
}

package() {
  backup=('etc/wownerod.conf')

  cd "${pkgname}"
  install -Dm644 "LICENSE" -t "${pkgdir}/usr/share/licenses/${pkgname}"

  install -Dm644 "utils/conf/wownerod.conf" "${pkgdir}/etc/wownerod.conf"
  install -Dm644 "utils/systemd/wownerod.service" "${pkgdir}/usr/lib/systemd/system/wownerod.service"
  install -Dm644 "../wownero.sysusers" "${pkgdir}/usr/lib/sysusers.d/wownero.conf"
  install -Dm644 "../wownero.tmpfiles" "${pkgdir}/usr/lib/tmpfiles.d/wownero.conf"

  install -Dm755 "build/bin/wownero-wallet-cli" \
                 "build/bin/wownero-wallet-rpc" \
                 "build/bin/wownerod" \
                 -t "${pkgdir}/usr/bin"
}

# vim: ts=2 sw=2 et:
