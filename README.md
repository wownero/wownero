# ~~Mo~~Wownero -  Such privacy! Many coins! Wow! 🐕

Copyright (c) 2014-2019 The Monero Project.   
Portions Copyright (c) 2012-2013 The Cryptonote developers.

# CyberWOW
An Android pruned full node for Wownero

[<img src="https://f-droid.org/badge/get-it-on.png"
      alt="Get it on F-Droid"
      height="80">](https://f-droid.org/en/packages/org.wownero.cyberwow/)
<a href='https://play.google.com/store/apps/details?id=org.wownero.cyberwow'><img alt='Get it on Google Play' src='https://play.google.com/intl/en_us/badges/images/generic/en_badge_web_generic.png' height='80'/></a>

# Wownerujo
An Android Wallet for Wownero

[<img src="https://f-droid.org/badge/get-it-on.png"
      alt="Get it on F-Droid"
      height="80">](https://f-droid.org/packages/com.wownero.wownerujo/)
<a href='https://play.google.com/store/apps/details?id=com.wownero.wownerujo'><img alt='Get it on Google Play' src='https://play.google.com/intl/en_us/badges/images/generic/en_badge_web_generic.png' height='80'/></a>

## Resources

- Web: [wownero.org](http://wownero.org)
- Twitter: [@w0wn3r0](https://twitter.com/w0wn3r0)
- Reddit: [/r/wownero](https://www.reddit.com/r/wownero)
- Mail: [wownero@protonmail.com](mailto:wownero@protonmail.com)
- GitHub: [https://github.com/wownero/wownero](https://github.com/wownero/wownero)
- IRC: [#wownero on Freenode](https://kiwiirc.com/client/irc.freenode.net/?nick=suchchatter|?#wownero)
- Bitmessage Chan: wownero (`BM-2cSzWtrj2pzLva9GF1Jp2TYsnLjrnJpvba`)
- Wownero Funding System: [https://funding.wownero.com](https://funding.wownero.com)
- Keybase Chat Group: [https://keybase.io/team/wownero](https://keybase.io/team/wownero)

Blockchain Explorers
- https://explore.wownero.com
- https://explorer.wowkira.com
- http://wow5eqtzqvsg5jctqzg5g7uk3u62sfqiacj5x6lo4by7bvnj6jkvubyd.onion

Free Public Nodes
- wow7dhbgiljnkspkzpjyy66auegbrye2ptfv4gucgbhireg5rrjza5ad.onion:34568
- wow.aluisyo.network:34568

Tor Peers
- wowp2p5gelm6vhl2d5tvfqills63jilgy6hkvlrqljooov5ktaxgqdad.onion
- f3moshycuklu3mb3wnlfjwn26nsgzreqtfzfuyjjk46u7jqxlhf7d5id.onion

## Introduction

Wownero is a privacy-centric memecoin that was fairly launched on April 1, 2018 with no pre-mine, stealth-mine or ICO. Wownero has a maximum supply of around 184 million WOW with a slow and steady emission over 50 years. It is a fork of Monero, but with its own genesis block, so there is no degradation of privacy due to ring signatures using different participants for the same tx outputs on opposing forks.

## Scheduled software upgrades

Wownero uses a fixed-schedule software upgrade (hard fork) mechanism to implement new features. This means that users of Wownero (end users and service providers) should run current versions and upgrade their software on a regular schedule. The required software for these upgrades will be available prior to the scheduled date. Please check the repository prior to this date for the proper Wownero software version. Below is the historical schedule and the projected schedule for the next upgrade.
Dates are provided in the format YYYY-MM-DD. 

| Software upgrade block height | Date       | Release Name | Minimum Wownero version | Recommended Wownero version | Details                                                                            |  
| ------------------------------ | -----------| ----------------- | ---------------------- | -------------------------- | ---------------------------------------------------------------------------------- |
| 1                              | 2018-04-01 | Awesome Akita                | v0.1.0.0               | v0.1.0.0                  | Cryptonight variant 1, ringsize >= 8, sorted inputs
| 6969                           | 2018-04-24 | Busty Brazzers                | v0.2.0.0               | v0.2.0.0                  | Bulletproofs, LWMA difficulty algorithm, ringsize >= 10, reduce unlock to 4
| 53666                          | 2018-10-06 | Cool Cage                | v0.3.0.0               | v0.3.1.3                  | Cryptonight variant 2, LWMA v2, ringsize = 22, MMS
| 63469                          | 2018-11-11 | Dank Doge               | v0.4.0.0               | v0.4.0.0                  | LWMA v4
| 81769                          | 2019-02-19 | Erotic EggplantEmoji    | v0.5.0.0               | v0.5.0.2                  | Cryptonight/wow, LWMA v1 with N=144, Updated Bulletproofs, Fee Per Byte, Auto-churn
| 114969                          | 2019-06-14 | F For Fappening    | v0.6.1.0               | v0.6.1.2                  | RandomWOW, new block weight algorithm, slightly more efficient RingCT format
| 160777                          | 2019-11-20 | Gaping Goatse    | v0.7.0.0               | v0.7.1.0                  | Only allow >= 2 outputs, change to the block median used to calculate penalty, rct sigs in coinbase forbidden, 4 unlock time as protocol rule
| XXXXX                       | 2020-04-XX | XXXX  | v0.8.0.0               | v0.8.0.0                  | Dandelion++ support

X's indicate that these details have not been determined as of commit date.

\* indicates estimate as of commit date

## Release staging and Contributing

**Anyone is welcome to contribute to Wownero's codebase!** 

If you have a fix or code change, feel free to submit it as a pull request. Ahead of a scheduled software upgrade, a development branch will be created with the new release version tag. Pull requests that address bugs should be made to Master. Pull requests that require review and testing (generally, optimizations and new features) should be made to the development branch. All pull requests will be considered safe until the US dollar valuation of 1 Wownero equals $1000. After this valuation has been reached, more research will be needed to introduce experimental cryptography and/or code into the codebase.  

## Installing from a package

Packages are available for

* Arch Linux/Manjaro

        yay -S wownero-git

* NixOS

        nix-shell -p wownero

* Ubuntu 18.04/Ubuntu 16.04/Debian 9/Debian 8 (amd64)

        sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys B09DF0E4B0C56A94
        sudo add-apt-repository "deb http://ppa.wownero.com/ bionic main"
        sudo apt-get update
        sudo apt-get install wownero

Packaging for your favorite distribution would be a welcome contribution!

**DISCLAIMER: These packages are not part of this repository, and as such, do not go through the same review process to ensure their trustworthiness and security.**


## Building from Source

* Arch Linux/Manjaro

        sudo pacman -Syu && sudo pacman -S base-devel cmake boost openssl zeromq libpgm unbound libsodium git
        git clone https://github.com/wownero/wownero
        cd wownero
        make


* Debian/Ubuntu

        sudo apt update && sudo apt install build-essential cmake pkg-config libboost-all-dev libssl-dev libzmq3-dev libunbound-dev libsodium-dev libpgm-dev git
        git clone https://github.com/wownero/wownero
        cd wownero
        make


## Running Binaries

The build places the binary in `bin/` sub-directory within the build directory
from which cmake was invoked (repository root by default). To run in
foreground:

    ./bin/wownerod

To list all available options, run `./bin/wownerod --help`.  Options can be
specified either on the command line or in a configuration file passed by the
`--config-file` argument.  To specify an option in the configuration file, add
a line with the syntax `argumentname=value`, where `argumentname` is the name
of the argument without the leading dashes, for example `log-level=1`.

To run in background:

    ./bin/wownerod --log-file wownerod.log --detach

To run as a systemd service, copy
[wownerod.service](utils/systemd/wownerod.service) to `/etc/systemd/system/` and
[wownerod.conf](utils/conf/wownerod.conf) to `/etc/`. The [example
service](utils/systemd/wownerod.service) assumes that the user `wownero` exists
and its home is the data directory specified in the [example
config](utils/conf/wownerod.conf).

Once node is synced to network, run the CLI wallet by entering:

    ./bin/wownero-wallet-cli

Type `help` in CLI wallet to see standard commands (for advanced options, type `help_advanced`).

## Tor Anonymity Network

* Install [Tor Browser](https://www.torproject.org/download/)
* Open `torrc` file in a text editor ([installation directory]/Browser/TorBrowser/Data/Tor/torrc) and add hidden service information as follows:

```
HiddenServiceDir [installation directory]/Browser/TorBrowser/Data/Tor/wow_node
HiddenServiceVersion 3
HiddenServicePort 44568 127.0.0.1:44568
```
* Save `torrc` file and restart Tor Browser (keep open)
* Change directory to the `wow_node` folder, open `hostname` file, and copy your node's ".onion" address
* Start wownerod with the following parameters:

```
./wownerod --tx-proxy tor,127.0.0.1:9150,10 --add-peer hdps3qwnusz64r7odvynmae6myc2uyvrsc2emap6636qeuzll72eouid.onion:44568 --anonymous-inbound YOUR_NODE_ADDRESS.onion:44568,127.0.0.1:44568,25
```

### Access remote Tor node from CLI wallet

```
./wownero-wallet-cli --proxy 127.0.0.1:9150 --daemon-address wow7dhbgiljnkspkzpjyy66auegbrye2ptfv4gucgbhireg5rrjza5ad.onion:34568
```

Use port `9050` instead of `9150` if you installed Tor as a standalone daemon. For more information, check out [ANONYMITY_NETWORKS](https://github.com/wownero/wownero/blob/master/ANONYMITY_NETWORKS.md).

## Donating to Wownero Project

Developers are volunteers doing this mostly for shits and giggles. If you would like to support our shenanigans and stimulant addictions, please consider donating to [WFS proposals](https://funding.wownero.com/proposals) or the [dev slush fund](https://dev-funding.webui.wowkira.com).

Donations may also be sent to: 

XMR: `44SQVPGLufPasUcuUQSZiF5c9BFzjcP8ucDxzzFDgLf1VkCEFaidJ3u2AhSKMhPLKA3jc2iS8wQHFcaigM6fXmo6AnFRn5B`

BTC: `bc1qcw9zglp3fxyl25zswemw7jczlqryms2lsmu464`
