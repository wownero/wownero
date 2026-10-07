#!/usr/bin/env python3
"""Package and verify native Wownero FCMP++ research artifacts."""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import platform
import re
import shutil
import subprocess
import tarfile


BINARIES = {
    "bin/wownero-fcmp-testnetd": "bin/wownero-fcmp-testnetd",
    "bin/wownero-fcmp-testnet-wallet-cli": "bin/wownero-fcmp-testnet-wallet-cli",
    "bin/wownero-fcmp-testnet-wallet-rpc": "bin/wownero-fcmp-testnet-wallet-rpc",
    "libexec/wow_local_network": "tests/wow_miner_proof/wow_local_network",
    "libexec/wow_pow_vectors": "tests/wow_miner_proof/wow_pow_vectors",
}
LICENSES = {"LICENSE": "LICENSE", "RandomWOW.LICENSE": "external/randomwow/LICENSE"}
PAYLOAD = set(BINARIES) | set(LICENSES)
METADATA = {"manifest.json", "SHA256SUMS"}
MAX_BYTES = 2 * 1024**3


def require(condition, cause):
    if not condition:
        raise ValueError(cause)


def command(*args):
    return subprocess.check_output(args, text=True).strip()


def digest(path):
    with path.open("rb") as stream:
        result = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def identity():
    source = os.environ.get("GITHUB_SHA", "")
    architecture = os.environ.get("WOW_ARCHITECTURE", "")
    require(re.fullmatch(r"[0-9a-f]{40}", source), "GITHUB_SHA must identify a source commit")
    require(architecture in ("aarch64", "x86_64"), "Unsupported or missing WOW_ARCHITECTURE")
    require(platform.system() == "Linux" and platform.machine() == architecture,
            "Artifact architecture does not match the native Linux runner")
    require(command("git", "rev-parse", "HEAD") == source, "Checkout does not match GITHUB_SHA")
    return source, architecture


def profile(build):
    values = {}
    for line in (build / "CMakeCache.txt").read_text().splitlines():
        if "=" in line and ":" in line.split("=", 1)[0]:
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    expected = {"WOWNERO_FCMP_TESTNET": "ON", "BUILD_WOW_MINER_PROOF_EXPERIMENT": "ON",
                "BUILD_TESTS": "ON", "CMAKE_BUILD_TYPE": "Release"}
    for name, value in expected.items():
        require(values.get(name) == value, "Build profile requires " + name + "=" + value)


def sums(root, names):
    return "".join(digest(root / name) + "  " + name + "\n" for name in sorted(names))


def package(build, destination):
    source, architecture = identity()
    profile(build)
    require(not destination.exists(), "Package destination already exists")
    for name in BINARIES.values():
        require((build / name).is_file(), "Missing build artifact: " + name)
    pin = command("git", "rev-parse", "HEAD:external/randomwow")
    require(command("git", "-C", "external/randomwow", "rev-parse", "HEAD") == pin,
            "RandomWOW checkout differs from the source pin")
    require(not command("git", "status", "--porcelain", "--untracked-files=no"),
            "Tracked source or submodules differ from the source commit")
    manifest = {"schema": 1, "source": source, "architecture": architecture,
                "randomwow": pin, "files": {}}
    destination.mkdir(parents=True)
    for name, origin in BINARIES.items():
        target = destination / name
        target.parent.mkdir(exist_ok=True)
        shutil.copyfile(build / origin, target)
        target.chmod(0o755)
        manifest["files"][name] = digest(target)
    for name, origin in LICENSES.items():
        shutil.copyfile(origin, destination / name)
        manifest["files"][name] = digest(destination / name)
    (destination / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    (destination / "SHA256SUMS").write_text(sums(destination, PAYLOAD | {"manifest.json"}))
    print("Packaged research artifacts for " + source + " (" + architecture + ")")


def unpack(archive, destination):
    require(not destination.exists(), "Extraction destination already exists")
    with tarfile.open(archive, "r:gz") as stream:
        members, files, total = [], set(), 0
        # Only the fixed package surface is accepted, including directory entries.
        for member in stream:
            require(len(members) < 32, "Archive contains excessive entries")
            path = PurePosixPath(member.name)
            require(not path.is_absolute() and ".." not in path.parts,
                    "Archive contains an unsafe path")
            name = str(path)
            if member.isdir():
                require(name in (".", "bin", "libexec"), "Unexpected archive directory: " + name)
            else:
                require(member.isfile(), "Archive contains a link or special file")
                require(name in PAYLOAD | METADATA, "Unexpected archive file: " + name)
                require(name not in files, "Duplicate archive file: " + name)
                require(member.size > 0, "Empty archive file: " + name)
                if name in METADATA:
                    require(member.size <= 16384, "Oversized artifact metadata")
                files.add(name)
                total += member.size
                require(total <= MAX_BYTES, "Archive exceeds the extraction size limit")
            members.append((member, name))
        require(files == PAYLOAD | METADATA, "Archive is missing required artifacts")
        destination.mkdir(parents=True)
        for member, name in members:
            if member.isfile():
                target = destination / name
                target.parent.mkdir(exist_ok=True)
                with stream.extractfile(member) as source, target.open("xb") as output:
                    shutil.copyfileobj(source, output)
                target.chmod(0o755 if name in BINARIES else 0o644)


def verify(destination):
    source, architecture = identity()
    require(not destination.is_symlink(), "Package directory must not be a link")
    files = set()
    for path in destination.rglob("*"):
        require(not path.is_symlink(), "Package contains a symbolic link")
        if path.is_file():
            files.add(path.relative_to(destination).as_posix())
        else:
            require(path.is_dir(), "Package contains a special file")
    require(files == PAYLOAD | METADATA, "Package files differ from the required artifacts")
    manifest = json.loads((destination / "manifest.json").read_text())
    expected = {"schema": 1, "source": source, "architecture": architecture,
                "randomwow": command("git", "rev-parse", "HEAD:external/randomwow")}
    for name, value in expected.items():
        require(manifest.get(name) == value, "Artifact metadata mismatch: " + name)
    require(isinstance(manifest.get("files"), dict) and set(manifest["files"]) == PAYLOAD,
            "Manifest does not identify the required artifacts")
    for name in PAYLOAD:
        require(name not in BINARIES or os.access(destination / name, os.X_OK), "Artifact is not executable: " + name)
        require(digest(destination / name) == manifest["files"][name], "Artifact digest mismatch: " + name)
    require((destination / "SHA256SUMS").read_text() == sums(destination, PAYLOAD | {"manifest.json"}),
            "SHA256SUMS does not match the package")
    print("Verified research artifacts for " + source + " (" + architecture + ")")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    pack = commands.add_parser("package")
    pack.add_argument("build", type=Path)
    pack.add_argument("destination", type=Path)
    extract = commands.add_parser("unpack")
    extract.add_argument("archive", type=Path)
    extract.add_argument("destination", type=Path)
    check = commands.add_parser("verify")
    check.add_argument("destination", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "package":
            package(args.build, args.destination)
        elif args.command == "unpack":
            unpack(args.archive, args.destination)
        else:
            verify(args.destination)
    except (OSError, ValueError, KeyError, tarfile.TarError, subprocess.CalledProcessError) as error:
        parser.exit(1, "Artifact operation refused: " + str(error) + "\n")


if __name__ == "__main__":
    main()
