"""Regression tests for source admission and artifact extraction."""

import contextlib
import io
import json
import os
from pathlib import Path
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import fcmp_testnet_artifacts as artifacts


class ArtifactTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = "1" * 40
        self.pin = "2" * 40
        self.enterContext(patch.object(artifacts, "identity", return_value=(self.source, "aarch64")))
        self.enterContext(patch.object(artifacts, "command", return_value=self.pin))
        self.enterContext(contextlib.redirect_stdout(io.StringIO()))

    def fixture(self):
        root = self.root / "package"
        root.mkdir()
        manifest = {"schema": 1, "source": self.source, "architecture": "aarch64",
                    "randomwow": self.pin, "files": {}}
        for name in artifacts.PAYLOAD:
            target = root / name
            target.parent.mkdir(exist_ok=True)
            target.write_bytes(b"test artifact")
            target.chmod(0o755)
            manifest["files"][name] = artifacts.digest(target)
        (root / "manifest.json").write_text(json.dumps(manifest))
        (root / "SHA256SUMS").write_text(artifacts.sums(root, artifacts.PAYLOAD | {"manifest.json"}))
        return root

    def test_round_trip(self):
        root = self.fixture()
        archive = self.root / "package.tar.gz"
        with tarfile.open(archive, "w:gz") as stream:
            stream.add(root, arcname=".")
        restored = self.root / "restored"
        artifacts.unpack(archive, restored)
        artifacts.verify(restored)

    def test_changed_binary_is_refused(self):
        root = self.fixture()
        (root / "bin/wownero-fcmp-testnetd").write_bytes(b"changed binary")
        with self.assertRaises(ValueError):
            artifacts.verify(root)

    def test_stale_source_or_architecture_is_refused(self):
        root = self.fixture()
        with patch.object(artifacts, "identity", return_value=("3" * 40, "aarch64")):
            with self.assertRaises(ValueError):
                artifacts.verify(root)
        with patch.object(artifacts, "identity", return_value=(self.source, "x86_64")):
            with self.assertRaises(ValueError):
                artifacts.verify(root)

    def test_unsafe_archive_is_refused_before_extraction(self):
        for name, kind in (("../escape", tarfile.REGTYPE), ("/escape", tarfile.REGTYPE),
                           ("bin/wownero-fcmp-testnetd", tarfile.SYMTYPE),
                           ("bin/wownero-fcmp-testnetd", tarfile.LNKTYPE)):
            with self.subTest(name=name, kind=kind):
                archive = self.root / "unsafe.tar.gz"
                with tarfile.open(archive, "w:gz") as stream:
                    entry = tarfile.TarInfo(name)
                    entry.type = kind
                    entry.linkname = "../escape"
                    entry.size = 1 if kind == tarfile.REGTYPE else 0
                    stream.addfile(entry, io.BytesIO(b"x"))
                destination = self.root / "unsafe"
                with self.assertRaises(ValueError):
                    artifacts.unpack(archive, destination)
                self.assertFalse(destination.exists())
                self.assertFalse((self.root / "escape").exists())

    def test_missing_and_duplicate_members_are_refused(self):
        archive = self.root / "invalid.tar.gz"
        for duplicate in (False, True):
            with self.subTest(duplicate=duplicate):
                with tarfile.open(archive, "w:gz") as stream:
                    for _ in range(2 if duplicate else 1):
                        entry = tarfile.TarInfo("manifest.json")
                        entry.size = 2
                        stream.addfile(entry, io.BytesIO(b"{}"))
                with self.assertRaises(ValueError):
                    artifacts.unpack(archive, self.root / "invalid")

    def test_existing_destination_is_preserved(self):
        root = self.fixture()
        with self.assertRaises(ValueError):
            artifacts.unpack(self.root / "absent.tar.gz", root)
        artifacts.verify(root)

    def test_profile_requires_explicit_selectors(self):
        cache = self.root / "CMakeCache.txt"
        selectors = {"WOWNERO_FCMP_TESTNET": "ON", "BUILD_WOW_MINER_PROOF_EXPERIMENT": "ON",
                     "BUILD_TESTS": "ON", "CMAKE_BUILD_TYPE": "Release"}
        cache.write_text("\n".join(key + ":STRING=" + value for key, value in selectors.items()))
        artifacts.profile(self.root)
        selectors["WOWNERO_FCMP_TESTNET"] = "OFF"
        cache.write_text("\n".join(key + ":STRING=" + value for key, value in selectors.items()))
        with self.assertRaises(ValueError):
            artifacts.profile(self.root)


class IdentityTests(unittest.TestCase):
    def test_native_source_admission(self):
        source = "1" * 40
        with patch.dict(os.environ, {"GITHUB_SHA": source, "WOW_ARCHITECTURE": "aarch64"}), \
                patch.object(artifacts.platform, "system", return_value="Linux"), \
                patch.object(artifacts.platform, "machine", return_value="aarch64"), \
                patch.object(artifacts, "command", return_value=source):
            self.assertEqual(artifacts.identity(), (source, "aarch64"))
            with patch.object(artifacts.platform, "machine", return_value="x86_64"):
                with self.assertRaises(ValueError):
                    artifacts.identity()
            with patch.object(artifacts, "command", return_value="2" * 40):
                with self.assertRaises(ValueError):
                    artifacts.identity()


if __name__ == "__main__":
    unittest.main()
