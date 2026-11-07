"""Exercise the installed menu entry with GLib, including unusual download paths."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipIf(os.getuid() == 0, "Desktop integration is per-user")
@unittest.skipUnless(shutil.which("gio") and shutil.which("desktop-file-validate"), "Requires GLib and desktop-file-utils")
class LinuxIntegrationTest(unittest.TestCase):
    def test_menu_entry_survives_removed_download_and_launches_with_arguments(self):
        for directory in ["normal", "with spaces", 'with $cash `tick` %percent "quotes" \\slashes']:
            with self.subTest(directory=directory), tempfile.TemporaryDirectory(prefix="xylar-menu-") as temporary:
                root = Path(temporary)
                home = root / "home"
                home.mkdir()
                data = home / directory
                env = dict(os.environ, HOME=str(home), XDG_DATA_HOME=str(data))
                appdir = root / "AppDir"
                icon = appdir / "share/icons/hicolor/256x256/apps/io.xylarjava.XylarJava.png"
                icon.parent.mkdir(parents=True)
                icon.write_bytes((ROOT / "program_info/io.xylarjava.XylarJava_256.png").read_bytes())
                download = root / "download.AppImage"
                download.write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$HOME/launch-args"\n')
                download.chmod(0o755)
                for _ in range(2):
                    subprocess.run(["sh", str(ROOT / "scripts/integrate-linux.sh"), str(download), str(appdir)], env=env, check=True)
                download.unlink()
                desktop = data / "applications/io.xylarjava.XylarJava.desktop"
                subprocess.run(["desktop-file-validate", str(desktop)], check=True)
                resource = root / "a modpack with spaces.mrpack"
                resource.touch()
                subprocess.run(["gio", "launch", str(desktop), str(resource)], env=env, check=True)
                result = home / "launch-args"
                for _ in range(100):
                    if result.exists():
                        break
                    time.sleep(0.02)
                self.assertEqual(result.read_text().splitlines(), ["--appimage-extract-and-run", str(resource)])
                self.assertTrue(os.access(data / "XylarJava/application/XylarJava.AppImage", os.X_OK))

    def test_native_bundle_keeps_a_working_copy(self):
        with tempfile.TemporaryDirectory(prefix="xylar-native-") as temporary:
            root = Path(temporary)
            app = root / "download"
            icon = app / "share/icons/hicolor/256x256/apps/io.xylarjava.XylarJava.png"
            icon.parent.mkdir(parents=True)
            icon.write_bytes(b"test icon")
            (app / "bin").mkdir()
            (app / "bin/XylarJava").write_bytes(b"test binary")
            (app / "AppRun").write_text('#!/bin/sh\nprintf "%s\\n" "$@" > "$HOME/launch-args"\n')
            (app / "AppRun").chmod(0o755)
            data = root / "data"
            env = dict(os.environ, HOME=str(root), XDG_DATA_HOME=str(data))
            subprocess.run(["sh", str(ROOT / "scripts/integrate-linux.sh"), str(app / "AppRun"), str(app), "native"], check=True, env=env)
            shutil.rmtree(app)
            desktop = data / "applications/io.xylarjava.XylarJava.desktop"
            subprocess.run(["desktop-file-validate", str(desktop)], check=True)
            resource = root / "example.mrpack"
            resource.touch()
            subprocess.run(["gio", "launch", str(desktop), str(resource)], check=True, env=env)
            result = root / "launch-args"
            for _ in range(100):
                if result.exists():
                    break
                time.sleep(0.02)
            self.assertEqual(result.read_text().splitlines(), [str(resource)])


if __name__ == "__main__":
    unittest.main()
