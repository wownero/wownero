# 
# This is a script for easily building the wownero cli installer in an automated build. Note
# that it is also possible to build the installer by dropping the .nsi file on the NSIS GUI,
# but that this will not work, as the script requires some defines, parsed from the wownero
# version file, to be passed on the command line.
#

import sys
sys.dont_write_bytecode = True

import os
import os.path
import subprocess

#
# Grab the dir of this .py
#
basedir = os.path.dirname(os.path.abspath(__file__))

#
# Try to find version.cpp.in.
#
version_file = os.path.join('..', 'src', 'version.cpp.in')
if not os.path.isfile(version_file):
    print('Version file not found: %s' % version_file)
    sys.exit(-1)

#
# Try to parse version.cpp.in.
#
version_string = None
release_name = None
with open(version_file, 'r') as fp:
    version_prefix = '#define DEF_MONERO_VERSION "'
    release_prefix = '#define DEF_MONERO_RELEASE_NAME "'
    for line in fp:
        if line.startswith(version_prefix):
            version_string = line.replace(version_prefix, '')[:-2]
        elif line.startswith(release_prefix):
            release_name = line.replace(release_prefix, '')[:-2]

if not version_string:
    print('Failed to parse version from: %s' % version_file)
    sys.exit(-1)

if not release_name:
    print('Failed to parse release name from: %s' % version_file)
    sys.exit(-1)

#
# Check that we got an expected version format.
#
version_parts = version_string.split('.')
if len(version_parts) != 4:
    print('Invalid version string: %s' % version_string)
    sys.exit(-1)

#
# Try to find makensis.
#
makensis = 'makensis.exe'
if not os.path.isfile(makensis):
    for dir in os.environ['PATH'].split(';'):
        test = os.path.join(dir, makensis)
        if os.path.isfile(test):
            makensis = test
            break

if not os.path.isfile(makensis):
    print('Failed to find makensis.exe')
    sys.exit(-1)

#
# Build & run makensis command line.
#
cmd  = '"%s"' % makensis
cmd += ' /V4'
cmd += ' /DVERSION_MAJOR=%s' % version_parts[0]
cmd += ' /DVERSION_MINOR=%s' % version_parts[1]
cmd += ' /DVERSION_BUILD=%s' % version_parts[2]
cmd += ' /DVERSION_REVISION=%s' % version_parts[3]
cmd += ' /DRELEASE_NAME="%s"' % release_name
cmd += ' "%s"' % os.path.join(basedir, 'cli-win', 'installer.nsi')

print("Calling makensis: %s" % cmd)

subprocess.call(cmd)

