# Minimal DebugView: appends OutputDebugString output (Wh_Log) to vm-share\debug.log.
# Started detached inside the VM by the agent:  Start-Process powershell -WindowStyle Hidden -ArgumentList '-File \\VBoxSvr\project\tools\vm-dbgview.ps1'
Add-Type @'
using System; using System.IO; using System.Text; using System.Threading;
using System.Runtime.InteropServices; using System.IO.MemoryMappedFiles;
public static class Dbg {
    public static void Run(string path) {
        var ready = new EventWaitHandle(false, EventResetMode.AutoReset, "DBWIN_BUFFER_READY");
        var data = new EventWaitHandle(false, EventResetMode.AutoReset, "DBWIN_DATA_READY");
        var mmf = MemoryMappedFile.CreateOrOpen("DBWIN_BUFFER", 4096);
        var view = mmf.CreateViewAccessor();
        var buf = new byte[4092];
        while (true) {
            ready.Set();
            if (!data.WaitOne()) continue;
            int pid = view.ReadInt32(0);
            view.ReadArray(4, buf, 0, buf.Length);
            int n = Array.IndexOf(buf, (byte)0); if (n < 0) n = buf.Length;
            string s = Encoding.Default.GetString(buf, 0, n).TrimEnd();
            if (s.Contains("[WH]") || s.Contains("mac-style"))
                // the host reads this file over the share while we append: retry instead of dying on a sharing error
                for (int i = 0; i < 20; i++) {
                    try { File.AppendAllText(path, DateTime.Now.ToString("HH:mm:ss.fff") + " " + pid + " " + s + "\r\n"); break; }
                    catch (IOException) { Thread.Sleep(25); }
                }
        }
    }
}
'@
[Dbg]::Run('\\VBoxSvr\project\vm-share\debug.log')
