# USBCopyManager

Soluzione Visual Studio 2026 (`USBCopyManager.sln`, x64) con due progetti:

| Progetto | Tipo | Contenuto |
|---|---|---|
| `USBCopyManager.Core` | C++20, DLL nativa (toolset v145) | Motore + **API C stabile** (`include/ucm_api.h`) |
| `USBCopyManager.Gui`  | C# WinForms, .NET 10 | Configurazione profili, monitoraggio, progress, log, tray |

La GUI chiama solo l'API C (P/Invoke in `NativeMethods.cs`); il Core non conosce la GUI.

## Build
Apri la `.sln`, configurazione **Debug|x64** o **Release|x64**, Compila soluzione.
La DLL finisce in `bin\x64\<Config>\` e viene copiata accanto a `USBCopyManager.exe`.
Se il toolset `v145` non risulta installato: Progetto Core > Proprietà > Generale > Platform Toolset.

## Dati dell'applicazione (`%LOCALAPPDATA%\USBCopyManager`)
- `config\<id>.profile` profili dispositivo
- `journal\<seriale>\<job-id>.journal` journal + checkpoint (offset e stato SHA-256)
- `logs\ucm.log`

## Scelte non specificate nel documento
- **Brutal**: copia tutti i work validi a ogni job, sovrascrive i file in destinazione, non aggiorna `LastCopiedWork`.
- `LastCopiedWork = 0` significa "nessuno copiato" (i work con numero > 0 sono tutti nuovi).
- Checkpoint ogni 64 MiB; buffer 4 MiB; il resume riparte solo se dimensione e data del sorgente coincidono col journal.
- Un job `Cancelled` non viene ripreso; `Running` (crash) e `Interrupted` (scollegamento) sì.
