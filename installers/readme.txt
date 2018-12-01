Installer notes

So, at this point I just have one installer, for the cli, on windows. What do you need
to know ..

1. It uses NSIS, and expects to find makensis.exe on your PATH.

2. It expects built products to be found at:

        wownero/build/release/bin

3. It currently includes the daemon, wallet, rpc, and gen-trusted-multisig. I note that
   releases on the wownero github have had different files (static libs, pthreads dll)
   than I end up with in my /bin dir when I build, so you may or may not need to change
   which files you include. At any rate, it is easy to do, in the WriteFiles function
   in installer.nsi.

4. Provided everything is ok, it should be as simple as executing build-cli-win.py; built
   installer, versioned according to src/version.cpp.in, will be depostied here:

        wownero/build/installers/Wownero-CLI-<version>-win.exe

5. The installer includes an uninstaller, shortcuts on desktop & start menu, and it
   registers itself with windows, so it shows up as expected in programs & features.

6. I have tried to keep the installer script pretty generic, so it can be reused later,
   for gui, or whatever. It installs to a <program files>/Wownero/cli subdirectory, so
   that other applications can eventually be installed alongside. NOTE: if you copy the
   installer.nsi file to use for another project, you will need to make a new GUID for
   it, and put it in the REG_KEY define.

