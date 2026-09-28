using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

internal static class Updates
{
    internal const string AssetName = "RelayCore-win-x64.zip";
    internal static readonly JavaScriptSerializer Json = new JavaScriptSerializer { MaxJsonLength = 4 * 1024 * 1024 };
    internal sealed class Release
    {
        public string Version, Url, Hash, Notes;
        public long Bytes;
    }

    internal static Dictionary<string, object> Object(string text)
    {
        return Json.Deserialize<Dictionary<string, object>>(text);
    }
    internal static string Text(Dictionary<string, object> data, string key)
    {
        object value;
        if (!data.TryGetValue(key, out value) || !(value is string)) throw new InvalidDataException("Нет поля " + key);
        return (string)value;
    }
    internal static Version VersionOf(string text)
    {
        if (!Regex.IsMatch(text, @"^\d{1,5}\.\d{1,5}\.\d{1,5}$")) throw new InvalidDataException("Некорректная версия: " + text);
        return new Version(text);
    }
    internal static Dictionary<string, object> Config(string root)
    {
        var config = Object(File.ReadAllText(Path.Combine(root, "updates.json")));
        if (Convert.ToInt32(config["schema"]) != 1) throw new InvalidDataException("Неподдерживаемый формат обновлений.");
        VersionOf(Text(config, "version"));
        if (!Regex.IsMatch(Text(config, "repository"), @"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$"))
            throw new InvalidDataException("Некорректный адрес репозитория.");
        return config;
    }
    internal static void RequireHttps(Uri uri)
    {
        var hosts = new[] { "github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com" };
        if (uri.Scheme != "https" || uri.Port != 443 || uri.UserInfo.Length != 0 || !hosts.Contains(uri.Host))
            throw new InvalidDataException("Недоверенный адрес загрузки.");
    }
    static async Task<HttpResponseMessage> Get(HttpClient client, string url, CancellationToken cancel)
    {
        var uri = new Uri(url);
        for (int count = 0; count < 6; ++count)
        {
            RequireHttps(uri);
            var response = await client.GetAsync(uri, HttpCompletionOption.ResponseHeadersRead, cancel);
            if ((int)response.StatusCode >= 300 && (int)response.StatusCode < 400)
            {
                var next = response.Headers.Location;
                response.Dispose();
                if (next == null) throw new InvalidDataException("Пустое перенаправление загрузки.");
                uri = new Uri(uri, next);
                continue;
            }
            if (response.StatusCode == HttpStatusCode.NotFound)
            {
                response.Dispose();
                throw new IOException("Обновление пока не опубликовано или недоступно.");
            }
            if (!response.IsSuccessStatusCode)
            {
                var status = response.StatusCode;
                response.Dispose();
                throw new IOException("Сервер обновлений ответил: " + status + ". Попробуйте позже.");
            }
            return response;
        }
        throw new IOException("Слишком много перенаправлений.");
    }
    static HttpClient Client()
    {
        ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
        var client = new HttpClient(new HttpClientHandler { AllowAutoRedirect = false });
        client.Timeout = TimeSpan.FromSeconds(30);
        client.DefaultRequestHeaders.UserAgent.ParseAdd("RelayCore-Updater/1");
        return client;
    }
    internal static Release ParseRelease(Dictionary<string, object> data, string repository)
    {
        if (Convert.ToInt32(data["schema"]) != 1 || Text(data, "channel") != "stable"
            || Text(data, "platform") != "windows-x64" || Text(data, "repository") != repository)
            throw new InvalidDataException("Эта сборка не предназначена для обычного обновления.");
        var version = Text(data, "version");
        VersionOf(version);
        string url = Text(data, "url"), digest = Text(data, "sha256");
        if (url != "https://github.com/" + repository + "/releases/download/v" + version + "/" + AssetName
            || !Regex.IsMatch(digest, @"^[a-fA-F0-9]{64}$"))
            throw new InvalidDataException("Не удалось проверить источник архива.");
        long bytes = Convert.ToInt64(data["bytes"]);
        if (bytes <= 0 || bytes >= 2L * 1024 * 1024 * 1024) throw new InvalidDataException("Недопустимый размер архива.");
        string notes = Text(data, "notes");
        return new Release { Version = version, Url = url, Hash = digest, Bytes = bytes,
            Notes = notes.Length > 2000 ? notes.Substring(0, 2000) : notes };
    }
    internal static async Task<Release> Check(string root, string requestedVersion, CancellationToken cancel)
    {
        var config = Config(root);
        string repo = Text(config, "repository");
        if (requestedVersion != null) VersionOf(requestedVersion);
        string endpoint = requestedVersion == null ? "latest/download" : "download/v" + requestedVersion;
        using (var client = Client())
        using (var response = await Get(client, "https://github.com/" + repo + "/releases/" + endpoint + "/release.json", cancel))
        using (var stream = await response.Content.ReadAsStreamAsync())
        using (var memory = new MemoryStream())
        {
            var buffer = new byte[8192];
            int size;
            while ((size = await stream.ReadAsync(buffer, 0, buffer.Length, cancel)) > 0)
            {
                if (memory.Length + size > 4 * 1024 * 1024) throw new InvalidDataException("Ответ сервера слишком велик.");
                memory.Write(buffer, 0, size);
            }
            var release = ParseRelease(Object(Encoding.UTF8.GetString(memory.ToArray())), repo);
            if (requestedVersion != null && release.Version != requestedVersion)
                throw new InvalidDataException("Сервер вернул другую версию.");
            return release;
        }
    }
    internal static string Hash(string path)
    {
        using (var sha = SHA256.Create())
        using (var stream = File.OpenRead(path))
            return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
    }
    internal static async Task Download(Release release, string path, Action<int> progress, CancellationToken cancel)
    {
        using (var client = Client())
        using (var response = await Get(client, release.Url, cancel))
        using (var input = await response.Content.ReadAsStreamAsync())
        using (var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None))
        {
            var buffer = new byte[65536];
            long total = 0;
            int size;
            while ((size = await input.ReadAsync(buffer, 0, buffer.Length, cancel)) != 0)
            {
                total += size;
                if (total > release.Bytes) throw new InvalidDataException("Архив больше заявленного размера.");
                await output.WriteAsync(buffer, 0, size, cancel);
                progress((int)(total * 100 / release.Bytes));
            }
        }
        VerifyArchive(path, release);
    }
    internal static void VerifyArchive(string path, Release release)
    {
        if (new FileInfo(path).Length != release.Bytes) throw new InvalidDataException("Загрузка не завершена.");
        if (!String.Equals(Hash(path), release.Hash, StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Контрольная сумма не совпала. Игра не изменена.");
    }
    internal static string SafePath(string root, string relative)
    {
        if (String.IsNullOrEmpty(relative) || relative.Contains("\\") || relative.Contains(":") || relative.StartsWith("/"))
            throw new InvalidDataException("Недопустимый путь в архиве.");
        foreach (string part in relative.Split('/'))
            if (part.Length == 0 || part == "." || part == ".." || part.EndsWith(".") || part.EndsWith(" ")
                || part.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0
                || Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)", RegexOptions.IgnoreCase))
                throw new InvalidDataException("Недопустимое имя файла в архиве.");
        string path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
        if (!path.StartsWith(Path.GetFullPath(root).TrimEnd('\\') + "\\", StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("Путь выходит за папку установки.");
        return path;
    }
    internal static string StagePath(string root)
    {
        // Keep staging beside the installation without repeating its potentially long folder name.
        return Path.Combine(Path.GetDirectoryName(root), ".rc-update-" + Guid.NewGuid().ToString("N"));
    }
    internal static string ExtractVerified(string archive, string stage, string version, string repository)
    {
        Directory.CreateDirectory(stage);
        var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        string package = null;
        long total = 0;
        using (var zip = ZipFile.OpenRead(archive))
        {
            if (zip.Entries.Count > 10000) throw new InvalidDataException("Слишком много файлов в архиве.");
            foreach (var entry in zip.Entries)
            {
                // Releases contain one top-level package; never extract links or overwrite a duplicate path.
                string name = entry.FullName.TrimEnd('/');
                string target = SafePath(stage, name);
                if (!names.Add(name) || ((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000
                    || (entry.ExternalAttributes & (int)FileAttributes.ReparsePoint) != 0)
                    throw new InvalidDataException("Ссылка или повторяющийся путь в архиве.");
                string first = name.Split('/')[0];
                if (package == null) package = first;
                if (package != first) throw new InvalidDataException("В архиве несколько корневых папок.");
                total += entry.Length;
                if (total > 4L * 1024 * 1024 * 1024) throw new InvalidDataException("Распакованный архив слишком велик.");
                if (entry.FullName.EndsWith("/")) Directory.CreateDirectory(target);
                else
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(target));
                    using (var input = entry.Open())
                    using (var output = new FileStream(target, FileMode.CreateNew))
                    {
                        var buffer = new byte[65536];
                        long written = 0;
                        int count;
                        while ((count = input.Read(buffer, 0, buffer.Length)) != 0)
                        {
                            written += count;
                            if (written > entry.Length) throw new InvalidDataException("Размер файла ZIP не совпадает с заголовком.");
                            output.Write(buffer, 0, count);
                        }
                        if (written != entry.Length) throw new InvalidDataException("Неполный файл ZIP.");
                    }
                }
            }
        }
        if (package == null) throw new InvalidDataException("Пустой архив.");
        string root = SafePath(stage, package);
        var config = Config(root);
        if (Text(config, "version") != version || Text(config, "repository") != repository)
            throw new InvalidDataException("Версия или канал архива не совпадает с релизом.");
        var manifest = Object(File.ReadAllText(Path.Combine(root, "BUILD_MANIFEST.json")));
        var payload = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (Dictionary<string, object> file in (IEnumerable)manifest["files"])
        {
            string relative = Text(file, "path"), path = SafePath(root, relative);
            if (!payload.Add(relative) || !File.Exists(path) || new FileInfo(path).Length != Convert.ToInt64(file["bytes"])
                || !String.Equals(Hash(path), Text(file, "sha256"), StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("Повреждён файл: " + relative);
        }
        foreach (string required in new[] { "RelayCore.exe", "RelayCoreUpdater.exe", "updates.json", "assets/config/engine.toml" })
            if (!payload.Contains(required)) throw new InvalidDataException("В сборке нет " + required);
        if (Directory.GetFiles(root, "*", SearchOption.AllDirectories).Length != payload.Count + 1)
            throw new InvalidDataException("В архиве есть файлы вне manifest.");
        return root;
    }
    internal static string Installation(string path)
    {
        string root = Path.GetFullPath(path).TrimEnd(Path.DirectorySeparatorChar);
        if (Path.GetDirectoryName(root) == null || Directory.Exists(Path.Combine(root, ".git"))
            || !File.Exists(Path.Combine(root, "BUILD_MANIFEST.json")) || !File.Exists(Path.Combine(root, "RelayCore.exe")))
            throw new InvalidDataException("Обновлять можно только распакованную игровую сборку.");
        for (var dir = new DirectoryInfo(root); dir != null; dir = dir.Parent)
            if ((dir.Attributes & FileAttributes.ReparsePoint) != 0)
                throw new InvalidDataException("Установка через ссылки на папки не поддерживается.");
        Config(root);
        return root;
    }
    internal static async Task WaitForGameExit(string root)
    {
        for (int attempt = 0; attempt < 300; ++attempt)
        {
            bool running = false;
            foreach (var process in Process.GetProcessesByName("RelayCore"))
            using (process)
            {
                try
                {
                    if (!process.HasExited && String.Equals(process.MainModule.FileName, Path.Combine(root, "RelayCore.exe"), StringComparison.OrdinalIgnoreCase))
                        running = true;
                }
                catch (InvalidOperationException) { }
            }
            if (!running) return;
            await Task.Delay(100);
        }
        throw new IOException("Закройте все окна этой сборки игры и повторите обновление.");
    }
    internal static async Task StartAndVerify(string root)
    {
        string eventName = @"Local\RelayCoreUpdate_" + Guid.NewGuid().ToString("N");
        using (var ready = new EventWaitHandle(false, EventResetMode.ManualReset, eventName))
        {
            var start = new ProcessStartInfo(Path.Combine(root, "RelayCore.exe")) { WorkingDirectory = root, UseShellExecute = false, CreateNoWindow = true };
            start.EnvironmentVariables["RELAYCORE_UPDATE_READY_EVENT"] = eventName;
            start.EnvironmentVariables.Remove("RELAYCORE_ASSET_ROOT");
            using (var process = Process.Start(start))
            {
                for (int i = 0; i < 600; ++i)
                {
                    if (ready.WaitOne(0)) return;
                    if (process.HasExited) throw new IOException("Новая версия не смогла запуститься.");
                    await Task.Delay(100);
                }
                process.CloseMainWindow();
                for (int i = 0; i < 100 && !process.HasExited; ++i) await Task.Delay(100);
                throw new IOException("Новая версия не подтвердила запуск. Если окно осталось открыто, закройте его.");
            }
        }
    }
    internal static async Task<string> Replace(string root, string prepared, Func<string, Task> verify)
    {
        string backup = root + ".previous-" + Guid.NewGuid().ToString("N");
        Directory.Move(root, backup);
        try
        {
            Directory.Move(prepared, root);
            await verify(root);
        }
        catch (Exception error)
        {
            try
            {
                if (Directory.Exists(root)) Directory.Move(root, root + ".failed-" + Guid.NewGuid().ToString("N"));
                Directory.Move(backup, root);
            }
            catch (Exception rollback)
            {
                throw new IOException("Не удалось вернуть старую сборку. Она сохранена в " + backup + ". " + rollback.Message, error);
            }
            throw new IOException("Предыдущая сборка восстановлена. " + error.Message, error);
        }
        return backup;
    }
}

internal sealed class UpdateWindow : Form
{
    readonly Label status = new Label { Dock = DockStyle.Fill, Padding = new Padding(18), AutoEllipsis = true };
    readonly ProgressBar progress = new ProgressBar { Dock = DockStyle.Bottom, Height = 22 };
    readonly Button close = new Button { Dock = DockStyle.Bottom, Height = 40, Text = "Отмена" };
    readonly CancellationTokenSource cancel = new CancellationTokenSource(TimeSpan.FromMinutes(30));
    bool installing, finished;
    readonly string root, version, workspace;
    internal UpdateWindow(string root, string version, string workspace)
    {
        this.root = root; this.version = version; this.workspace = workspace;
        Text = "Обновление RelayCore"; ClientSize = new Size(590, 260); MinimumSize = new Size(490, 260);
        StartPosition = FormStartPosition.CenterScreen; Font = new Font("Segoe UI", 11);
        Controls.Add(status); Controls.Add(progress); Controls.Add(close);
        close.Click += delegate { if (finished) Close(); else cancel.Cancel(); };
        FormClosing += delegate(object sender, FormClosingEventArgs e) { if (!finished) { e.Cancel = true; if (!installing) cancel.Cancel(); } };
        Shown += async delegate { await Run(); };
    }
    async Task Run()
    {
        string stage = Updates.StagePath(root);
        string archive = Path.Combine(workspace, "download.zip");
        bool completed = false;
        try
        {
            status.Text = "Проверяем версию " + version + "…";
            var release = await Updates.Check(root, version, cancel.Token);
            var config = Updates.Config(root);
            if (Updates.VersionOf(version) <= Updates.VersionOf(Updates.Text(config, "version")))
                throw new InvalidDataException("Эта версия уже установлена или старее текущей.");
            status.Text = "Загружаем " + version + ". Текущая сборка пока остаётся на месте.";
            await Updates.Download(release, archive, value => progress.Value = value, cancel.Token);
            status.Text = "Проверяем файлы…";
            string prepared = await Task.Run(() => Updates.ExtractVerified(archive, stage, version, Updates.Text(config, "repository")));
            cancel.Token.ThrowIfCancellationRequested();
            await Updates.WaitForGameExit(root);
            cancel.Token.ThrowIfCancellationRequested();
            installing = true; close.Enabled = false;
            status.Text = "Устанавливаем и проверяем запуск игры…";
            string backup = await Updates.Replace(root, prepared, Updates.StartAndVerify);
            status.Text = "Версия " + version + " установлена. Игра запущена.\n\nПредыдущая сборка сохранена:\n" + backup;
            completed = true;
        }
        catch (OperationCanceledException) { status.Text = "Загрузка отменена или превышено время ожидания. Текущая игра не изменена."; }
        catch (Exception error)
        {
            status.Text = "Обновление не завершено.\n\n" + error.Message + "\n\nМожно закрыть это окно и запустить игру снова.";
            File.WriteAllText(Path.Combine(workspace, "error.log"), error.ToString());
        }
        finally
        {
            finished = true; close.Enabled = true; close.Text = "Закрыть";
            try
            {
                if (File.Exists(archive)) File.Delete(archive);
                // The uniquely created staging directory contains only validated archive entries, never user files.
                if (Directory.Exists(stage)) Directory.Delete(stage, true);
            }
            catch (IOException) { status.Text += "\nВременные файлы не удалось удалить: " + stage; }
            catch (UnauthorizedAccessException) { status.Text += "\nВременные файлы не удалось удалить: " + stage; }
        }
        if (completed) Close();
    }
}

internal static class Program
{
    static string Quote(string value) { return "\"" + value.TrimEnd('\\').Replace("\"", "\\\"") + "\""; }
    static void WriteResult(string text)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(text);
        using (var output = Console.OpenStandardOutput()) output.Write(bytes, 0, bytes.Length);
    }
    [STAThread]
    static int Main(string[] args)
    {
        try
        {
            if (args.Length == 2 && args[0] == "--check")
            {
                string root = Updates.Installation(args[1]);
                using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(20)))
                {
                    var release = Updates.Check(root, null, timeout.Token).GetAwaiter().GetResult();
                    bool available = Updates.VersionOf(release.Version) > Updates.VersionOf(Updates.Text(Updates.Config(root), "version"));
                    WriteResult((available ? "available" : "current") + "\n" + release.Version + "\n" + release.Bytes + "\n" + release.Notes);
                }
                return 0;
            }
            if (args.Length != 3 || (args[0] != "--install" && args[0] != "--worker")) throw new ArgumentException("Запускайте обновление из меню игры.");
            string installation = Updates.Installation(args[1]);
            Updates.VersionOf(args[2]);
            if (args[0] == "--install")
            {
                string temp = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "RelayCore", "updater", Guid.NewGuid().ToString("N"));
                Directory.CreateDirectory(temp);
                string worker = Path.Combine(temp, "RelayCoreUpdater.exe");
                File.Copy(Application.ExecutablePath, worker);
                Process.Start(new ProcessStartInfo(worker, "--worker " + Quote(installation) + " " + Quote(args[2])) { UseShellExecute = false });
                return 0;
            }
            string mutexName;
            using (var sha = SHA256.Create()) mutexName = @"Local\RelayCoreUpdater_" + BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(installation.ToUpperInvariant()))).Replace("-", "");
            bool owned;
            using (var mutex = new Mutex(true, mutexName, out owned))
            {
                if (!owned) throw new IOException("Обновление этой игры уже запущено.");
                try
                {
                    Application.EnableVisualStyles();
                    Application.Run(new UpdateWindow(installation, args[2], AppDomain.CurrentDomain.BaseDirectory));
                }
                finally { mutex.ReleaseMutex(); }
            }
            return 0;
        }
        catch (Exception error)
        {
            if (args.Length > 0 && args[0] == "--check") WriteResult(error.Message);
            else MessageBox.Show(error.Message, "RelayCore: обновление", MessageBoxButtons.OK, MessageBoxIcon.Error);
            return 1;
        }
    }
}
