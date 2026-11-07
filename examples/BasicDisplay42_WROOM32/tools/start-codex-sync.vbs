Set shell = CreateObject("WScript.Shell")
Set fs = CreateObject("Scripting.FileSystemObject")
folder = fs.GetParentFolderName(WScript.ScriptFullName)
python = "pythonw.exe"
If WScript.Arguments.Count > 0 Then python = WScript.Arguments(0)
shell.Run """" & python & """ """ & folder & "\codex_usage_sync.py"" --config """ & folder & "\codex-sync.json""", 0, False
