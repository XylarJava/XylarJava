"""Verify dispatch, failure handling and download without contacting GitHub."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
MOCK = '''#!/usr/bin/env python3
import os, pathlib, sys
args = sys.argv[1:]
state = pathlib.Path(os.environ["TEST_STATE"])
with (state / "calls").open("a") as log:
    log.write(pathlib.Path(sys.argv[0]).name + " " + " ".join(args) + "\\n")
if pathlib.Path(sys.argv[0]).name == "git":
    if args[:2] == ["status", "--porcelain"]:
        print(" M dirty" if os.environ.get("TEST_DIRTY") else "", end="")
    elif args[:2] == ["branch", "--show-current"]:
        print("main")
    elif args[:2] == ["rev-parse", "HEAD"]:
        print("a" * 40)
else:
    if args[:2] == ["run", "list"]:
        print("123456")
    elif args[:2] == ["run", "watch"] and os.environ.get("TEST_FAILED"):
        sys.exit(1)
    elif args[:2] == ["run", "download"]:
        output = pathlib.Path(args[args.index("--dir") + 1])
        (output / "XylarJava.exe").write_bytes(b"MZ-test-package")
'''


class WindowsBuildCommandTest(unittest.TestCase):
    def run_build(self, **extra):
        temporary = tempfile.TemporaryDirectory(prefix="xylar-windows-command-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / "scripts").mkdir()
        shutil.copy(ROOT / "scripts/build-windows.sh", root / "scripts/build-windows.sh")
        tools = root / "tools"
        tools.mkdir()
        for name in ("git", "gh"):
            mock = tools / name
            mock.write_text(MOCK)
            mock.chmod(0o755)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"], TEST_STATE=str(root), **extra)
        result = subprocess.run(["bash", str(root / "scripts/build-windows.sh")], env=env, capture_output=True, text=True)
        return root, result

    def test_downloads_single_executable_after_success(self):
        root, result = self.run_build()
        self.assertEqual(result.returncode, 0, result.stderr)
        output = root / "dist-single/XylarJava-Windows/XylarJava.exe"
        self.assertEqual(output.read_bytes(), b"MZ-test-package")
        self.assertEqual(len(list(output.parent.iterdir())), 1)
        calls = (root / "calls").read_text()
        self.assertIn("-f target=windows -f request-id=windows-", calls)
        self.assertIn("run download 123456 --name release-windows", calls)

    def test_failed_build_does_not_download_or_publish(self):
        root, result = self.run_build(TEST_FAILED="1")
        self.assertNotEqual(result.returncode, 0)
        calls = (root / "calls").read_text()
        self.assertIn("run view 123456 --log-failed", calls)
        self.assertNotIn("run download", calls)
        self.assertNotIn("release create", calls)

    def test_uncommitted_changes_stop_before_dispatch(self):
        root, result = self.run_build(TEST_DIRTY="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("workflow run", (root / "calls").read_text())


if __name__ == "__main__":
    unittest.main()
