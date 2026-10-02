using System.Text;

namespace USBCopyManager.Gui;

/// <summary>Presentazione: nessuna logica di copia, tutto passa dall'API C del Core.</summary>
public sealed class MainForm : Form
{
    private readonly UcmCore _core = new();
    private readonly ListView _list = new() { Dock = DockStyle.Fill, View = View.Details, FullRowSelect = true, GridLines = true, MultiSelect = false, HideSelection = false };
    private readonly Label _lblWork = new() { AutoSize = true, Font = new Font(SystemFonts.DefaultFont, FontStyle.Bold), Text = "Nessun job" };
    private readonly Label _lblFile = new() { AutoSize = true, Text = "—" };
    private readonly ProgressBar _pbFile = new() { Dock = DockStyle.Fill, Maximum = 1000, Height = 18 };
    private readonly ProgressBar _pbTotal = new() { Dock = DockStyle.Fill, Maximum = 1000, Height = 18 };
    private readonly Label _lblStats = new() { AutoSize = true, Text = "" };
    private readonly TextBox _log = new() { Dock = DockStyle.Fill, Multiline = true, ReadOnly = true, ScrollBars = ScrollBars.Vertical, Font = new Font("Consolas", 9f) };
    private readonly Button _btnStart = new() { Text = "Avvia copia", AutoSize = true };
    private readonly Button _btnCancel = new() { Text = "Annulla copia", AutoSize = true };
    private readonly NotifyIcon _tray = new() { Icon = SystemIcons.Application, Text = "USBCopyManager", Visible = true };
    private readonly System.Windows.Forms.Timer _timer = new() { Interval = 500 };
    private bool _exiting;

    public MainForm()
    {
        Text = "USBCopyManager";
        ClientSize = new Size(980, 700);
        StartPosition = FormStartPosition.CenterScreen;
        Icon = SystemIcons.Application;

        foreach (var (title, w) in new[] { ("Profilo", 170), ("Seriale", 120), ("Stato dispositivo", 130), ("Modalità", 100), ("Last work", 80), ("Job", 110), ("Destinazione", 260) })
            _list.Columns.Add(title, w);
        _list.SelectedIndexChanged += (_, _) => UpdateJobPanel();
        _list.DoubleClick += (_, _) => EditSelected();

        var btnNew = new Button { Text = "Nuovo…", AutoSize = true };
        var btnEdit = new Button { Text = "Modifica…", AutoSize = true };
        var btnDel = new Button { Text = "Elimina", AutoSize = true };
        btnNew.Click += (_, _) => NewProfile();
        btnEdit.Click += (_, _) => EditSelected();
        btnDel.Click += (_, _) => DeleteSelected();
        _btnStart.Click += (_, _) => StartSelected();
        _btnCancel.Click += (_, _) => CancelSelected();
        var bar = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, Padding = new Padding(6) };
        bar.Controls.AddRange(new Control[] { btnNew, btnEdit, btnDel, _btnStart, _btnCancel });

        var job = new GroupBox { Text = "Job corrente", Dock = DockStyle.Fill, AutoSize = true, Padding = new Padding(8) };
        var jt = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, AutoSize = true };
        jt.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        jt.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        jt.Controls.Add(_lblWork, 0, 0); jt.SetColumnSpan(_lblWork, 2);
        jt.Controls.Add(_lblFile, 0, 1); jt.SetColumnSpan(_lblFile, 2);
        jt.Controls.Add(new Label { Text = "File", AutoSize = true, Margin = new Padding(0, 4, 8, 0) }, 0, 2); jt.Controls.Add(_pbFile, 1, 2);
        jt.Controls.Add(new Label { Text = "Totale", AutoSize = true, Margin = new Padding(0, 4, 8, 0) }, 0, 3); jt.Controls.Add(_pbTotal, 1, 3);
        jt.Controls.Add(_lblStats, 0, 4); jt.SetColumnSpan(_lblStats, 2);
        job.Controls.Add(jt);

        var root = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 4 };
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 45));
        root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 55));
        root.Controls.Add(bar, 0, 0);
        root.Controls.Add(_list, 0, 1);
        root.Controls.Add(job, 0, 2);
        root.Controls.Add(_log, 0, 3);
        Controls.Add(root);

        var menu = new ContextMenuStrip();
        menu.Items.Add("Apri", null, (_, _) => ShowMain());
        menu.Items.Add("Esci", null, (_, _) => { _exiting = true; Close(); });
        _tray.ContextMenuStrip = menu;
        _tray.DoubleClick += (_, _) => ShowMain();

        _core.LogReceived += (lvl, msg) => OnUi(() => AppendLog(lvl, msg));
        _core.EventReceived += (evt, pid, info) => OnUi(() => OnCoreEvent(evt, pid, info));
        _timer.Tick += (_, _) => RefreshAll();
        Load += (_, _) =>
        {
            RefreshAll();
            _timer.Start();
            _core.StartMonitoring(2000);      // rilevamento automatico + avvio automatico dei profili che lo prevedono
        };
        FormClosing += OnFormClosing;
    }

    // ---------- thread marshaling ----------
    private void OnUi(Action a)
    {
        if (IsDisposed || !IsHandleCreated) return;
        try { BeginInvoke(a); } catch (ObjectDisposedException) { } catch (InvalidOperationException) { }
    }

    private void ShowMain() { Show(); WindowState = FormWindowState.Normal; Activate(); }

    private void OnFormClosing(object? sender, FormClosingEventArgs e)
    {
        if (!_exiting && e.CloseReason == CloseReason.UserClosing)
        {
            e.Cancel = true;
            Hide();
            _tray.ShowBalloonTip(2000, "USBCopyManager", "L'applicazione resta attiva nell'area di notifica.", ToolTipIcon.Info);
            return;
        }
        _timer.Stop();
        _tray.Visible = false;
        _tray.Dispose();
        _core.Dispose();
    }

    // ---------- azioni ----------
    private string? SelectedId => _list.SelectedItems.Count > 0 ? (string)_list.SelectedItems[0].Tag! : null;

    private void NewProfile()
    {
        using var f = new ProfileForm(_core, null);
        if (f.ShowDialog(this) != DialogResult.OK) return;
        try { _core.SaveProfile(f.Result); } catch (Exception ex) { MessageBox.Show(this, ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error); }
        RefreshAll();
    }

    private void EditSelected()
    {
        var id = SelectedId; if (id is null) return;
        var p = _core.GetProfiles().FirstOrDefault(x => x.Id == id);
        if (string.IsNullOrEmpty(p.Id)) return;
        using var f = new ProfileForm(_core, p);
        if (f.ShowDialog(this) != DialogResult.OK) return;
        try { _core.SaveProfile(f.Result); } catch (Exception ex) { MessageBox.Show(this, ex.Message, Text, MessageBoxButtons.OK, MessageBoxIcon.Error); }
        RefreshAll();
    }

    private void DeleteSelected()
    {
        var id = SelectedId; if (id is null) return;
        if (MessageBox.Show(this, "Eliminare il profilo selezionato? (i dati copiati non vengono toccati)", Text, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes) return;
        _core.DeleteProfile(id);
        RefreshAll();
    }

    private void StartSelected()
    {
        var id = SelectedId; if (id is null) return;
        int r = _core.StartCopy(id);
        if (r != Ucm.Ok) MessageBox.Show(this, UcmCore.Text(r), Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
    }

    private void CancelSelected()
    {
        var id = SelectedId; if (id is null) return;
        _core.Cancel(id);
    }

    // ---------- aggiornamento vista ----------
    private static string StateText(int s) => s switch
    {
        Ucm.JobRunning => "In copia",
        Ucm.JobCompleted => "Completato",
        Ucm.JobCancelled => "Annullato",
        Ucm.JobInterrupted => "Interrotto",
        Ucm.JobFailed => "Errore",
        _ => "Inattivo"
    };

    private static string Bytes(double b)
    {
        string[] u = { "B", "KB", "MB", "GB", "TB" };
        int i = 0;
        while (b >= 1024 && i < u.Length - 1) { b /= 1024; i++; }
        return $"{b:0.#} {u[i]}";
    }

    private void RefreshAll()
    {
        var profiles = _core.GetProfiles();
        var seen = new HashSet<string>();
        _list.BeginUpdate();
        foreach (var p in profiles)
        {
            seen.Add(p.Id);
            var dev = _core.FindDevice(p.Serial);
            var job = _core.GetJob(p.Id);
            var item = _list.Items.Cast<ListViewItem>().FirstOrDefault(i => (string?)i.Tag == p.Id);
            if (item is null)
            {
                item = new ListViewItem(Enumerable.Repeat("", 7).ToArray()) { Tag = p.Id };
                _list.Items.Add(item);
            }
            string status = dev is { } d && !string.IsNullOrEmpty(d.DriveLetter) ? $"● Connesso ({d.DriveLetter})" : "○ Non collegato";
            string[] cols = { p.Name, p.Serial, status, p.Mode == Ucm.ModeBrutal ? "Brutal" : "Synchronized",
                              p.LastCopiedWork > 0 ? $"{p.Prefix}{p.LastCopiedWork.ToString().PadLeft(p.Digits, '0')}" : "—",
                              job is { } j ? StateText(j.State) : "", p.Destination };
            for (int c = 0; c < cols.Length; c++) if (item.SubItems[c].Text != cols[c]) item.SubItems[c].Text = cols[c];
        }
        foreach (var gone in _list.Items.Cast<ListViewItem>().Where(i => !seen.Contains((string)i.Tag!)).ToList()) _list.Items.Remove(gone);
        _list.EndUpdate();
        if (_list.SelectedItems.Count == 0 && _list.Items.Count > 0) _list.Items[0].Selected = true;
        UpdateJobPanel();
    }

    private void UpdateJobPanel()
    {
        var id = SelectedId;
        UcmJobInfo? ji = id is null ? null : _core.GetJob(id);
        bool running = ji is { State: Ucm.JobRunning };
        _btnCancel.Enabled = running;
        _btnStart.Enabled = id is not null && !running;
        if (ji is not { } j || j.State == Ucm.JobIdle)
        {
            _lblWork.Text = "Nessun job"; _lblFile.Text = "—"; _lblStats.Text = ""; _pbFile.Value = 0; _pbTotal.Value = 0;
            return;
        }
        _lblWork.Text = $"{j.Work}   [{StateText(j.State)}]" + (j.State is Ucm.JobRunning or Ucm.JobIdle || string.IsNullOrEmpty(j.Message) ? "" : $" — {j.Message}");
        _lblFile.Text = string.IsNullOrEmpty(j.CurrentFile) ? "—" : j.CurrentFile;
        _pbFile.Value = j.FileSize == 0 ? 0 : (int)Math.Min(1000, j.FileCopied * 1000 / j.FileSize);
        _pbTotal.Value = j.TotalSize == 0 ? (j.State == Ucm.JobCompleted ? 1000 : 0) : (int)Math.Min(1000, j.TotalCopied * 1000 / j.TotalSize);
        var sb = new StringBuilder();
        sb.Append($"File: {Bytes(j.FileCopied)} / {Bytes(j.FileSize)}   |   Totale: {Bytes(j.TotalCopied)} / {Bytes(j.TotalSize)}");
        if (running) sb.Append($"   |   {Bytes(j.SpeedBytesPerSec)}/s");
        if (j.LastCompletedWork > 0) sb.Append($"   |   Ultimo work completato: {j.LastCompletedWork}");
        _lblStats.Text = sb.ToString();
    }

    private void AppendLog(int level, string msg)
    {
        string tag = level switch { 0 => "DBG", 2 => "WRN", 3 => "ERR", _ => "INF" };
        _log.AppendText($"{DateTime.Now:HH:mm:ss} [{tag}] {msg}{Environment.NewLine}");
        if (_log.TextLength > 200_000) _log.Text = _log.Text[^100_000..];
    }

    private void OnCoreEvent(int evt, string profileId, UcmJobInfo info)
    {
        if (evt == Ucm.EvtProgress) return;                       // la barra si aggiorna con il timer
        string name = _core.GetProfiles().FirstOrDefault(p => p.Id == profileId).Name ?? "Dispositivo";
        (string? title, string? text, ToolTipIcon icon) toast = evt switch
        {
            Ucm.EvtDeviceConnected => ("Dispositivo collegato", name, ToolTipIcon.Info),
            Ucm.EvtJobStarted => ("Copia avviata", name, ToolTipIcon.Info),
            Ucm.EvtWorkCompleted => ("Work completato", $"{name}: {info.Work}", ToolTipIcon.Info),
            Ucm.EvtJobCompleted => ("Copia completata", name, ToolTipIcon.Info),
            Ucm.EvtJobInterrupted => ("Copia interrotta", $"{name}: verrà ripresa al prossimo collegamento", ToolTipIcon.Warning),
            Ucm.EvtJobFailed => ("Copia fallita", $"{name}: {info.Message}", ToolTipIcon.Error),
            _ => (null, null, ToolTipIcon.None)
        };
        if (toast.title is not null) _tray.ShowBalloonTip(3000, toast.title, toast.text, toast.icon);
        RefreshAll();
    }
}
