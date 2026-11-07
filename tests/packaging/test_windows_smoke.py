"""Guard the reuse path: missing artifacts never start compilation or packaging."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

import yaml

ROOT = Path(__file__).resolve().parents[2]
MOCK = '''#!/usr/bin/env python3
import os, pathlib, sys
args = sys.argv[1:]
state = pathlib.Path(os.environ["TEST_STATE"])
with (state / "calls").open("a") as log:
    log.write(pathlib.Path(sys.argv[0]).name + " " + " ".join(args) + "\\n")
if pathlib.Path(sys.argv[0]).name == "git":
    if args[:2] == ["branch", "--show-current"]: print("main")
    elif args[:2] == ["rev-parse", "HEAD"]: print("a" * 40)
else:
    if args[:2] == ["repo", "view"]: print("XylarJava/XylarJava")
    elif args[:1] == ["api"] and not os.environ.get("TEST_MISSING"): print("123")
    elif args[:2] == ["run", "list"]: print("456")
    elif args[:2] == ["run", "download"]:
        output = pathlib.Path(args[args.index("--dir") + 1]); output.mkdir(parents=True, exist_ok=True)
        if args[args.index("--name") + 1] == "release-windows":
            if (output / "XylarJava.exe").exists(): sys.exit("file exists")
            (output / "XylarJava.exe").write_bytes(b"unchanged package")
'''


class WindowsSmokeTest(unittest.TestCase):
    def test_smoke_job_contains_no_build_steps_and_can_run_with_build_skipped(self):
        workflow = yaml.safe_load((ROOT / ".github/workflows/release.yml").read_text())
        jobs = workflow["jobs"]
        for job in ("client", "windows", "linux"):
            self.assertIn("inputs.smoke-run-id == ''", jobs[job]["if"])
        self.assertIn("always()", jobs["windows-smoke"]["if"])
        self.assertIn("inputs.smoke-run-id != ''", jobs["windows-smoke"]["if"])
        commands = "\n".join(step.get("run", "") for step in jobs["windows-smoke"]["steps"])
        self.assertIn("test-windows-package.ps1", commands)
        self.assertNotIn("cmake", commands)
        self.assertNotIn("dotnet publish", commands)
        self.assertNotIn("build-windows-single-exe", commands)
        save = jobs["windows"]["steps"][-1]
        self.assertEqual(save["with"]["name"], "windows-package")
        self.assertEqual(save["with"]["if-no-files-found"], "error")

    def run_command(self, missing=False, existing=False):
        temporary = tempfile.TemporaryDirectory(prefix="xylar-smoke-command-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / "scripts").mkdir()
        shutil.copy(ROOT / "scripts/test-windows.sh", root / "scripts/test-windows.sh")
        if existing:
            output = root / "dist-single/XylarJava-Windows/XylarJava.exe"
            output.parent.mkdir(parents=True)
            output.write_bytes(b"previous download")
        tools = root / "tools"; tools.mkdir()
        for name in ("git", "gh"):
            path = tools / name; path.write_text(MOCK); path.chmod(0o755)
        env = dict(os.environ, PATH=str(tools) + os.pathsep + os.environ["PATH"], TEST_STATE=str(root))
        if missing: env["TEST_MISSING"] = "1"
        result = subprocess.run(["bash", str(root / "scripts/test-windows.sh"), "42376892443"], env=env, capture_output=True, text=True)
        return root, result

    def test_missing_package_stops_before_dispatch_or_push(self):
        root, result = self.run_command(missing=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No build, packaging or smoke test was started", result.stderr)
        calls = (root / "calls").read_text()
        self.assertNotIn("workflow run", calls)
        self.assertNotIn("git push", calls)

    def test_reuses_selected_artifact_and_downloads_verified_exe(self):
        root, result = self.run_command(existing=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        calls = (root / "calls").read_text()
        self.assertIn("-f smoke-run-id=42376892443 -f smoke-artifact=windows-package", calls)
        self.assertNotIn("cmake", calls)
        self.assertNotIn("publish", calls)
        self.assertEqual((root / "dist-single/XylarJava-Windows/XylarJava.exe").read_bytes(), b"unchanged package")


if __name__ == "__main__":
    unittest.main()
