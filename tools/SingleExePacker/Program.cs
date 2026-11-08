using System.Diagnostics;
using System.ComponentModel;
using System.IO.Compression;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        try
        {
            var root = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "XylarJava");
            var hash = ComputeEmbeddedHash();
            var versionDir = Path.Combine(root, "versions", hash);
            var appDir = Path.Combine(versionDir, "app");
            var exePath = Path.Combine(appDir, "XylarJava.exe");

            // Versioned payloads keep updates from replacing files used by a running launcher.
            using (var mutex = new Mutex(false, @"Local\XylarJava.PackageInstallation"))
            {
                try { if (!mutex.WaitOne(TimeSpan.FromMinutes(2))) throw new TimeoutException("Another XylarJava installation is still running."); }
                catch (AbandonedMutexException) { /* The preceding installer exited unexpectedly. */ }
                try
                {
                    Directory.CreateDirectory(versionDir);
                    if (!File.Exists(exePath) || !File.Exists(Path.Combine(appDir, "platforms", "qwindows.dll")))
                        ExtractPayload(appDir, hash);

                    var launcher = Path.Combine(versionDir, "XylarJava.exe");
                    var self = Environment.ProcessPath ?? throw new InvalidOperationException("Cannot locate XylarJava.exe.");
                    if (!string.Equals(self, launcher, StringComparison.OrdinalIgnoreCase) && !File.Exists(launcher))
                        File.Copy(self, launcher);
                    try
                    {
                        CreateShortcut(Environment.SpecialFolder.DesktopDirectory, launcher);
                        CreateShortcut(Environment.SpecialFolder.Programs, launcher);
                    }
                    catch (Exception ex)
                    {
                        // A read-only desktop must not prevent the launcher from opening.
                        Debug.WriteLine("Could not create shortcuts: " + ex.Message);
                    }
                }
                finally { mutex.ReleaseMutex(); }
            }

            var psi = new ProcessStartInfo { FileName = exePath, WorkingDirectory = appDir, UseShellExecute = false };
            foreach (var arg in args)
                psi.ArgumentList.Add(arg);
            using var proc = Process.Start(psi) ?? throw new InvalidOperationException("Cannot start XylarJava.");
            proc.WaitForExit();
            if (proc.ExitCode != 0) WriteLog("Native launcher exited with code " + proc.ExitCode);
            return proc.ExitCode;
        }
        catch (Exception ex)
        {
            WriteLog(ex.ToString());
            string message = ex is Win32Exception win32 && (win32.NativeErrorCode == 225 || win32.NativeErrorCode == 226)
                ? "Windows security blocked a launcher file. Open Windows Security → Protection history to see the detection and file name. " +
                  "Download the latest release from github.com/XylarJava/XylarJava/releases and report the detection name and SHA-256 to the maintainers. " +
                  "Do not disable antivirus protection. Reinstalling game files will not resolve this block."
                : ex.Message;
            System.Windows.Forms.MessageBox.Show("XylarJava failed to start:\n" + message + "\n\nDetails: " + LogPath, "XylarJava",
                System.Windows.Forms.MessageBoxButtons.OK, System.Windows.Forms.MessageBoxIcon.Error);
            return 1;
        }
    }

    private static void CreateShortcut(Environment.SpecialFolder folder, string launcher)
    {
        var directory = Environment.GetFolderPath(folder);
        if (string.IsNullOrEmpty(directory)) return;
        Directory.CreateDirectory(directory);
        var shellType = Type.GetTypeFromProgID("WScript.Shell") ?? throw new InvalidOperationException("Windows shortcuts unavailable.");
        object? shell = null;
        object? shortcut = null;
        try
        {
            shell = Activator.CreateInstance(shellType)!;
            shortcut = shellType.InvokeMember("CreateShortcut", BindingFlags.InvokeMethod, null, shell,
                new object[] { Path.Combine(directory, "XylarJava.lnk") })!;
            var type = shortcut.GetType();
            if (string.Equals(type.InvokeMember("TargetPath", BindingFlags.GetProperty, null, shortcut, null) as string,
                launcher, StringComparison.OrdinalIgnoreCase)) return;
            type.InvokeMember("TargetPath", BindingFlags.SetProperty, null, shortcut, new object[] { launcher });
            type.InvokeMember("WorkingDirectory", BindingFlags.SetProperty, null, shortcut, new object[] { Path.GetDirectoryName(launcher)! });
            type.InvokeMember("IconLocation", BindingFlags.SetProperty, null, shortcut, new object[] { launcher + ",0" });
            type.InvokeMember("Description", BindingFlags.SetProperty, null, shortcut, new object[] { "XylarJava - Minecraft Launcher" });
            type.InvokeMember("Save", BindingFlags.InvokeMethod, null, shortcut, null);
        }
        finally
        {
            if (shortcut is not null) Marshal.FinalReleaseComObject(shortcut);
            if (shell is not null) Marshal.FinalReleaseComObject(shell);
        }
    }

    private static string ComputeEmbeddedHash()
    {
        // Generated at packaging time. Avoid reading and hashing the entire payload on every launch.
        using var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("XylarJava.Payload.sha256")
            ?? throw new InvalidDataException("Package checksum missing.");
        using var reader = new StreamReader(stream);
        var hash = reader.ReadToEnd().Trim().ToUpperInvariant();
        if (hash.Length != 64 || !hash.All(Uri.IsHexDigit)) throw new InvalidDataException("Invalid package checksum.");
        return hash;
    }

    private static string LogPath => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "XylarJava", "bootstrap.log");
    private static void WriteLog(string message)
    {
        try {
            Directory.CreateDirectory(Path.GetDirectoryName(LogPath)!);
            if (File.Exists(LogPath) && new FileInfo(LogPath).Length > 1024 * 1024) File.Delete(LogPath);
            File.AppendAllText(LogPath, DateTime.UtcNow.ToString("O") + " " + message + Environment.NewLine);
        } catch { /* Logging must not obscure the original startup failure. */ }
    }

    private static Stream OpenPayloadStream() => Assembly.GetExecutingAssembly()
        .GetManifestResourceStream("XylarJava.Payload.zip") ?? throw new InvalidOperationException("Embedded payload missing.");

    private static void ExtractPayload(string destination, string expectedHash)
    {
        var temp = destination + "." + Guid.NewGuid().ToString("N");
        Directory.CreateDirectory(temp);
        try
        {
            using (var check = OpenPayloadStream())
                if (!string.Equals(Convert.ToHexString(SHA256.HashData(check)), expectedHash, StringComparison.Ordinal))
                    throw new InvalidDataException("The embedded payload checksum is incorrect. Download the release again.");
            using (var stream = OpenPayloadStream())
            using (var zip = new ZipArchive(stream, ZipArchiveMode.Read))
                zip.ExtractToDirectory(temp);
            if (!File.Exists(Path.Combine(temp, "XylarJava.exe")) || !File.Exists(Path.Combine(temp, "platforms", "qwindows.dll")))
                throw new InvalidDataException("The package is missing the launcher or the Qt Windows plugin.");
            if (Directory.Exists(destination)) Directory.Delete(destination, true);
            Directory.Move(temp, destination);
        }
        finally
        {
            if (Directory.Exists(temp)) Directory.Delete(temp, true);
        }
    }
}
