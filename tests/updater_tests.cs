using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Threading.Tasks;

internal static class UpdaterTests
{
    static int assertions;
    static void Require(bool value, string message)
    {
        ++assertions;
        if (!value) throw new Exception(message);
    }
    static void Reject(Action action, string message)
    {
        try { action(); } catch (InvalidDataException) { ++assertions; return; }
        throw new Exception(message);
    }
    static string Package(string work, string version, bool corrupt)
    {
        string folder = Path.Combine(work, "package");
        Directory.CreateDirectory(Path.Combine(folder, "assets", "config"));
        File.WriteAllText(Path.Combine(folder, "RelayCore.exe"), "test game");
        File.WriteAllText(Path.Combine(folder, "RelayCoreUpdater.exe"), "test updater");
        File.WriteAllText(Path.Combine(folder, "updates.json"), "{\"schema\":1,\"version\":\"" + version + "\",\"repository\":\"owner/game\"}");
        File.WriteAllText(Path.Combine(folder, "assets", "config", "engine.toml"), "test config");
        var files = Directory.GetFiles(folder, "*", SearchOption.AllDirectories).Select(path => new {
            path = path.Substring(folder.Length + 1).Replace('\\', '/'), bytes = new FileInfo(path).Length, sha256 = Updates.Hash(path)
        }).ToArray();
        File.WriteAllText(Path.Combine(folder, "BUILD_MANIFEST.json"), Updates.Json.Serialize(new { files = files }));
        if (corrupt) File.WriteAllText(Path.Combine(folder, "RelayCore.exe"), "bad game!");
        string archive = Path.Combine(work, "game.zip");
        using (var zip = ZipFile.Open(archive, ZipArchiveMode.Create))
            foreach (string path in Directory.GetFiles(folder, "*", SearchOption.AllDirectories))
                zip.CreateEntryFromFile(path, "package/" + path.Substring(folder.Length + 1).Replace('\\', '/'));
        return archive;
    }
    public static int Main(string[] args)
    {
        string work = Path.Combine(Path.GetTempPath(), "RelayCore-updater-test-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(work);
        try
        {
            if (args.Length == 1)
            {
                // A real packaged game must signal readiness, not merely remain alive after Process.Start.
                using (var zip = ZipFile.OpenRead(args[0]))
                {
                    var entry = zip.Entries.Single(e => e.FullName.EndsWith("/updates.json"));
                    Dictionary<string, object> config;
                    using (var reader = new StreamReader(entry.Open())) config = Updates.Object(reader.ReadToEnd());
                    string version = Updates.Text(config, "version"), repo = Updates.Text(config, "repository");
                    string current = Updates.ExtractVerified(args[0], Path.Combine(work, "old"), version, repo);
                    string next = Updates.ExtractVerified(args[0], Updates.StagePath(current), version, repo);
                    File.WriteAllText(Path.Combine(current, "old-only.txt"), "old asset");
                    string backup = Updates.Replace(current, next, Updates.StartAndVerify).GetAwaiter().GetResult();
                    Require(!File.Exists(Path.Combine(current, "old-only.txt")), "Removed assets survived replacement");
                    Require(File.Exists(Path.Combine(backup, "old-only.txt")), "Backup was not retained");
                    foreach (var process in Process.GetProcessesByName("RelayCore"))
                    using (process)
                    {
                        if (!String.Equals(process.MainModule.FileName, Path.Combine(current, "RelayCore.exe"), StringComparison.OrdinalIgnoreCase)) continue;
                        Require(process.Handle != IntPtr.Zero, "Could not retain the test process handle through shutdown");
                        Require(process.CloseMainWindow(), "Could not close the test game's SDL window");
                        Require(process.WaitForExit(10000) && process.ExitCode == 0, "Updated game did not shut down cleanly");
                    }
                    Console.WriteLine("real_update=passed; installation=" + current + "; backup=" + backup);
                }
                return 0;
            }
            foreach (string bad in new[] { "../outside", "/absolute", "C:/outside", "folder/../../outside", "folder\\outside", "file:stream", "CON.txt", "a/NUL", "trailing./x", "space /x" })
                Reject(() => Updates.SafePath(work, bad), "Accepted unsafe path: " + bad);
            foreach (string bad in new[] { "1.0", "1.0.0-beta", "999999999.0.0", "../1.0.0" })
                Reject(() => Updates.VersionOf(bad), "Accepted malformed version");
            Require(Updates.VersionOf("1.10.0") > Updates.VersionOf("1.9.0"), "Version comparison is lexical");
            Reject(() => Updates.RequireHttps(new Uri("http://github.com/file")), "Accepted HTTP");
            Reject(() => Updates.RequireHttps(new Uri("https://github.com.evil.test/file")), "Accepted foreign host");
            Reject(() => Updates.RequireHttps(new Uri("https://github.com:444/file")), "Accepted foreign port");
            var release = Updates.Object("{\"schema\":1,\"channel\":\"stable\",\"platform\":\"windows-x64\",\"repository\":\"owner/game\",\"version\":\"1.3.1\",\"notes\":\"Notes\",\"bytes\":100,\"sha256\":\"" + new string('a', 64) + "\",\"url\":\"https://github.com/owner/game/releases/download/v1.3.1/RelayCore-win-x64.zip\"}");
            Require(Updates.ParseRelease(release, "owner/game").Version == "1.3.1", "Valid release rejected");
            Reject(() => Updates.ParseRelease(release, "other/game"), "Accepted foreign repository");
            release["channel"] = "preview";
            Reject(() => Updates.ParseRelease(release, "owner/game"), "Accepted prerelease");
            string archive = Package(Path.Combine(work, "valid"), "1.3.1", false);
            var download = new Updates.Release { Bytes = new FileInfo(archive).Length, Hash = Updates.Hash(archive) };
            Updates.VerifyArchive(archive, download);
            download.Hash = new string('0', 64);
            Reject(() => Updates.VerifyArchive(archive, download), "Accepted wrong download hash");
            download.Bytes++;
            Reject(() => Updates.VerifyArchive(archive, download), "Accepted truncated download");
            string prepared = Updates.ExtractVerified(archive, Path.Combine(work, "stage"), "1.3.1", "owner/game");
            Require(File.Exists(Path.Combine(prepared, "RelayCore.exe")), "Valid package rejected");
            Reject(() => Updates.ExtractVerified(archive, Path.Combine(work, "wrong-version"), "1.3.2", "owner/game"), "Accepted different version");
            Reject(() => Updates.ExtractVerified(archive, Path.Combine(work, "wrong-channel"), "1.3.1", "other/game"), "Accepted different channel");
            string corrupt = Package(Path.Combine(work, "corrupt"), "1.3.1", true);
            Reject(() => Updates.ExtractVerified(corrupt, Path.Combine(work, "bad-stage"), "1.3.1", "owner/game"), "Accepted corrupted payload");
            foreach (string name in new[] { "package/../escape", "package/C:/escape" })
            {
                string attack = Path.Combine(work, Guid.NewGuid().ToString("N") + ".zip");
                using (var zip = ZipFile.Open(attack, ZipArchiveMode.Create)) using (var writer = new StreamWriter(zip.CreateEntry(name).Open())) writer.Write("bad");
                Reject(() => Updates.ExtractVerified(attack, Path.Combine(work, Guid.NewGuid().ToString("N")), "1.3.1", "owner/game"), "Zip traversal accepted");
            }
            string duplicate = Path.Combine(work, "duplicate.zip");
            using (var zip = ZipFile.Open(duplicate, ZipArchiveMode.Create))
            { zip.CreateEntry("package/file"); zip.CreateEntry("package/FILE"); }
            Reject(() => Updates.ExtractVerified(duplicate, Path.Combine(work, "duplicates"), "1.3.1", "owner/game"), "Case-folding duplicate accepted");
            string currentRoot = Path.Combine(work, "installed");
            Directory.CreateDirectory(currentRoot);
            File.WriteAllText(Path.Combine(currentRoot, "old.txt"), "saved old version");
            bool rolledBack = false;
            try { Updates.Replace(currentRoot, prepared, async root => { await Task.Yield(); throw new IOException("simulated startup failure"); }).GetAwaiter().GetResult(); }
            catch (IOException) { rolledBack = true; }
            Require(rolledBack && File.ReadAllText(Path.Combine(currentRoot, "old.txt")) == "saved old version", "Startup failure did not restore old files");
            Require(!File.Exists(Path.Combine(currentRoot, "RelayCore.exe")), "Rollback left mixed versions");
            string nextRoot = Updates.ExtractVerified(archive, Path.Combine(work, "success"), "1.3.1", "owner/game");
            string previous = Updates.Replace(currentRoot, nextRoot, root => Task.FromResult(0)).GetAwaiter().GetResult();
            Require(File.Exists(Path.Combine(previous, "old.txt")) && !File.Exists(Path.Combine(currentRoot, "old.txt")), "Old and new versions were mixed");
            Require(Updates.Installation(currentRoot) == currentRoot, "Installation rejected");
            Directory.CreateDirectory(Path.Combine(currentRoot, ".git"));
            Reject(() => Updates.Installation(currentRoot), "Source checkout accepted as installation");
            Console.WriteLine("updater=passed; assertions=" + assertions);
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
        finally
        {
            // The real startup test leaves its package and backup for inspection.
            if (args.Length == 0 && Directory.Exists(work)) Directory.Delete(work, true);
        }
    }
}
