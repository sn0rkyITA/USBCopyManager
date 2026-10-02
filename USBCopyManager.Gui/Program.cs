namespace USBCopyManager.Gui;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        using var mutex = new Mutex(true, @"Local\USBCopyManager.Gui.SingleInstance", out bool first);
        if (!first)
        {
            MessageBox.Show("USBCopyManager è già in esecuzione (controlla l'area di notifica).", "USBCopyManager",
                MessageBoxButtons.OK, MessageBoxIcon.Information);
            return;
        }
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
