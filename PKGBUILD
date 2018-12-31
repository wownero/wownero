# Maintainer: wowario <wowario at protonmail dot com>
# Contributor: wowario <wowario at protonmail dot com>

pkgbase="wownero"
pkgname=('wownero')
pkgver=0.5.0.0
pkgrel=1
pkgdesc="Wownero: a fairly launched privacy-centric meme coin with no premine and a finite supply"
license=('custom:Cryptonote')
arch=('x86_64')
url="http://wownero.org/"
depends=('boost-libs' 'openssl' 'zeromq' 'unbound')
makedepends=('git' 'cmake' 'boost')
provides=('wownero')

source=("${pkgname}"::"git+https://github.com/wownero/wownero")

sha256sums+=('SKIP')

_wownero="${pkgbase}"
_build="build"

build() {
  cd "${srcdir}/${_wownero}"
  git fetch && git checkout dev-v0.5
  CMAKE_FLAGS+=" -DCMAKE_BUILD_TYPE=Release "
  CMAKE_FLAGS+=" -DCMAKE_INSTALL_PREFIX=/usr "
  mkdir -p $_build && cd $_build
  cmake $CMAKE_FLAGS ../
  make
}

package_wownero() {
  install -Dm644 "${srcdir}/${_wownero}/LICENSE" "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
  install -Dm644 "${srcdir}/${_wownero}/utils/conf/wownerod.conf" "${pkgdir}/etc/wownerod.conf"
  install -Dm644 "${srcdir}/${_wownero}/utils/systemd/wownerod.service" "${pkgdir}/usr/lib/systemd/system/wownerod.service"
  install -Dm755 "${srcdir}/${_wownero}/build/bin/wownerod" "${pkgdir}/usr/bin/wownerod"
  install -Dm755 "${srcdir}/${_wownero}/build/bin/wownero-wallet-cli" "${pkgdir}/usr/bin/wownero-wallet-cli"
  install -Dm755 "${srcdir}/${_wownero}/build/bin/wownero-wallet-rpc" "${pkgdir}/usr/bin/wownero-wallet-rpc"
}
