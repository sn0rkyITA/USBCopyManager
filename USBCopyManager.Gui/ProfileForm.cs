namespace USBCopyManager.Gui;

/// <summary>Finestra di configurazione di un profilo dispositivo.</summary>
public sealed class ProfileForm : Form
{
    private sealed record DeviceItem(UcmDeviceInfo Info)
    {
        public override string ToString() =>
            $"{Info.Serial}  —  {Info.Model}" + (string.IsNullOrEmpty(Info.DriveLetter) ? "" : $"  ({Info.DriveLetter})");
    }

    private readonly TextBox _name = new() { Dock = DockStyle.Fill };
    private readonly ComboBox _serial = new() { Dock = DockStyle.Fill, DropDownStyle = ComboBoxStyle.DropDown };
    private readonly TextBox _dest = new() { Dock = DockStyle.Fill };
    private readonly TextBox _prefix = new() { Dock = DockStyle.Left, Width = 80 };
    private readonly NumericUpDown _digits = new() { Minimum = 1, Maximum = 9, Value = 5, Width = 60 };
    private readonly ComboBox _mode = new() { Dock = DockStyle.Left, Width = 180, DropDownStyle = ComboBoxStyle.DropDownList };
    private readonly NumericUpDown _last = new() { Minimum = 0, Maximum = 999999999, Width = 120 };
    private readonly CheckBox _verify = new() { Text = "Verifica hash dopo la copia (rilettura della destinazione)", AutoSize = true, Checked = true };
    private readonly CheckBox _auto = new() { Text = "Avvia la copia automaticamente al collegamento", AutoSize = true };

    private UcmProfile _profile;
    public UcmProfile Result => _profile;

    public ProfileForm(UcmCore core, UcmProfile? existing)
    {
        Text = existing is null ? "Nuovo profilo" : "Modifica profilo";
        FormBorderStyle = FormBorderStyle.FixedDialog;
        MaximizeBox = MinimizeBox = false;
        StartPosition = FormStartPosition.CenterParent;
        ClientSize = new Size(640, 360);

        _mode.Items.AddRange(new object[] { "Synchronized", "Brutal" });
        _mode.SelectedIndex = 0;

        foreach (var d in core.EnumerateDevices()) _serial.Items.Add(new DeviceItem(d));

        var grid = new TableLayoutPanel { Dock = DockStyle.Top, ColumnCount = 3, AutoSize = true, Padding = new Padding(12) };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));

        void Row(string label, Control c, Control? extra = null)
        {
            int r = grid.RowCount++;
            grid.Controls.Add(new Label { Text = label, AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 8, 8, 8) }, 0, r);
            grid.Controls.Add(c, 1, r);
            if (extra != null) grid.Controls.Add(extra, 2, r);
        }

        var browse = new Button { Text = "Sfoglia…", AutoSize = true };
        browse.Click += (_, _) =>
        {
            using var dlg = new FolderBrowserDialog { SelectedPath = _dest.Text, UseDescriptionForTitle = true, Description = "Cartella di destinazione" };
            if (dlg.ShowDialog(this) == DialogResult.OK) _dest.Text = dlg.SelectedPath;
        };

        var naming = new FlowLayoutPanel { Dock = DockStyle.Fill, AutoSize = true, WrapContents = false };
        naming.Controls.Add(_prefix);
        naming.Controls.Add(new Label { Text = "cifre:", AutoSize = true, Margin = new Padding(12, 6, 4, 0) });
        naming.Controls.Add(_digits);

        Row("Nome", _name);
        Row("Numero di serie", _serial);
        Row("Destinazione", _dest, browse);
        Row("Naming work", naming);
        Row("Modalità", _mode);
        Row("LastCopiedWork", _last);
        grid.RowCount++;
        grid.Controls.Add(_verify, 1, grid.RowCount - 1);
        grid.RowCount++;
        grid.Controls.Add(_auto, 1, grid.RowCount - 1);

        var ok = new Button { Text = "Salva", DialogResult = DialogResult.None, Width = 90 };
        var cancel = new Button { Text = "Annulla", DialogResult = DialogResult.Cancel, Width = 90 };
        ok.Click += (_, _) => { if (Collect()) DialogResult = DialogResult.OK; };
        var buttons = new FlowLayoutPanel { Dock = DockStyle.Bottom, FlowDirection = FlowDirection.RightToLeft, Height = 48, Padding = new Padding(8) };
        buttons.Controls.Add(cancel);
        buttons.Controls.Add(ok);

        Controls.Add(grid);
        Controls.Add(buttons);
        AcceptButton = ok;
        CancelButton = cancel;

        _profile = existing ?? new UcmProfile { Id = "", Name = "", Serial = "", Destination = "", Prefix = "SN_", Digits = 5, Verify = 1 };
        _name.Text = _profile.Name;
        _serial.Text = _profile.Serial;
        _dest.Text = _profile.Destination;
        _prefix.Text = string.IsNullOrEmpty(_profile.Prefix) ? "SN_" : _profile.Prefix;
        _digits.Value = Math.Clamp(_profile.Digits, 1, 9);
        _mode.SelectedIndex = _profile.Mode == Ucm.ModeBrutal ? 1 : 0;
        _last.Value = Math.Max(0, _profile.LastCopiedWork);
        _verify.Checked = _profile.Verify != 0;
        _auto.Checked = _profile.AutoStart != 0;
    }

    private bool Collect()
    {
        string serial = _serial.SelectedItem is DeviceItem di ? di.Info.Serial : _serial.Text.Trim();
        if (string.IsNullOrWhiteSpace(_name.Text) || string.IsNullOrWhiteSpace(serial) || string.IsNullOrWhiteSpace(_dest.Text) || string.IsNullOrWhiteSpace(_prefix.Text))
        {
            MessageBox.Show(this, "Nome, numero di serie, destinazione e prefisso sono obbligatori.", Text, MessageBoxButtons.OK, MessageBoxIcon.Warning);
            return false;
        }
        _profile.Name = _name.Text.Trim();
        _profile.Serial = serial;
        _profile.Destination = _dest.Text.Trim();
        _profile.Prefix = _prefix.Text.Trim();
        _profile.Digits = (int)_digits.Value;
        _profile.Mode = _mode.SelectedIndex == 1 ? Ucm.ModeBrutal : Ucm.ModeSynchronized;
        _profile.LastCopiedWork = (int)_last.Value;
        _profile.Verify = _verify.Checked ? 1 : 0;
        _profile.AutoStart = _auto.Checked ? 1 : 0;
        return true;
    }
}
