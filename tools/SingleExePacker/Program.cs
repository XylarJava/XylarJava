using System.Diagnostics;
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
                try { mutex.WaitOne(); }
                catch (AbandonedMutexException) { /* The preceding installer exited unexpectedly. */ }
                try
                {
                    Directory.CreateDirectory(versionDir);
                    if (!File.Exists(exePath))
                        ExtractPayload(appDir);

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
            return proc.ExitCode;
        }
        catch (Exception ex)
        {
            System.Windows.Forms.MessageBox.Show("XylarJava failed to start:\n" + ex.Message, "XylarJava",
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
        using var stream = OpenPayloadStream();
        return Convert.ToHexString(SHA256.HashData(stream));
    }

    private static Stream OpenPayloadStream() => Assembly.GetExecutingAssembly()
        .GetManifestResourceStream("XylarJava.Payload.zip") ?? throw new InvalidOperationException("Embedded payload missing.");

    private static void ExtractPayload(string destination)
    {
        var temp = destination + "." + Guid.NewGuid().ToString("N");
        Directory.CreateDirectory(temp);
        try
        {
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
