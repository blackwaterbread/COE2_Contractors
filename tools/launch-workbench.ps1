<#
.SYNOPSIS
Starts Arma Reforger Workbench with COE2_Contractors, without the launcher.

.DESCRIPTION
Opens addons/COE2_Contractors with these addon folders registered through -addonsDir: this repo's addons,
the Marx addons (a sibling Marx checkout by default), the Workshop download folder (COE2, Kex Scenario Core,
ACE) and the game's addons folder. Workbench and the game are looked up in the Steam library folders listed
in Steam's libraryfolders.vdf, unless given explicitly.

.PARAMETER MarxDir
Marx repository root. Default: ..\Marx next to this repository.

.PARAMETER WorkshopAddonsDir
Folder with Workshop downloads. Default: Documents\My Games\ArmaReforger\addons.

.PARAMETER NoScriptAuthorizeAll
Do not pass -scriptAuthorizeAll (which suppresses the "Script Authorization Required" prompt).

.PARAMETER WorkbenchExe
Path to ArmaReforgerWorkbenchSteamDiag.exe. Default: found in the Steam libraries.

.PARAMETER GameDir
Arma Reforger install folder. Default: found in the Steam libraries.

.PARAMETER LogsDir
Folder for this session's logs. Default: a new logs_<timestamp> folder in the Workbench logs folder
(Documents\My Games\ArmaReforgerWorkbench\logs).

.PARAMETER DryRun
Print the command line without starting Workbench.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File tools/launch-workbench.ps1
#>
param(
	[string]$MarxDir,
	[string]$WorkshopAddonsDir,
	[switch]$NoScriptAuthorizeAll,
	[string]$WorkbenchExe,
	[string]$GameDir,
	[string]$LogsDir,
	[switch]$DryRun
)

$ErrorActionPreference = "Stop"

function Get-SteamLibraries
{
	$steamRoot = $null
	try
	{
		$steamRoot = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction Stop).SteamPath
	}
	catch
	{
		$steamRoot = "C:\Program Files (x86)\Steam"
	}

	$libraries = @($steamRoot)
	$vdf = Join-Path $steamRoot "steamapps\libraryfolders.vdf"
	if (Test-Path $vdf)
	{
		foreach ($match in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"'))
		{
			$libraries += $match.Groups[1].Value.Replace("\\", "\")
		}
	}

	return $libraries | Select-Object -Unique
}

function Find-InSteamLibraries([string]$relativePath)
{
	foreach ($library in Get-SteamLibraries)
	{
		$candidate = Join-Path $library $relativePath
		if (Test-Path $candidate)
		{
			return $candidate
		}
	}

	return $null
}

function Get-AddonFolders([string]$root)
{
	return @(Get-ChildItem -Path $root -Directory | Where-Object { Test-Path (Join-Path $_.FullName "addon.gproj") } | ForEach-Object { $_.FullName })
}

if (-not $WorkbenchExe)
{
	$WorkbenchExe = Find-InSteamLibraries "steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe"
}

if (-not $GameDir)
{
	$GameDir = Find-InSteamLibraries "steamapps\common\Arma Reforger"
}

if (-not $WorkbenchExe -or -not (Test-Path $WorkbenchExe))
{
	throw "Workbench not found. Pass -WorkbenchExe."
}

if (-not $GameDir -or -not (Test-Path (Join-Path $GameDir "addons")))
{
	throw "Arma Reforger not found. Pass -GameDir."
}

$repoRoot = Split-Path $PSScriptRoot -Parent
if (-not $MarxDir)
{
	$MarxDir = Join-Path (Split-Path $repoRoot -Parent) "Marx"
}

if (-not (Test-Path (Join-Path $MarxDir "addons")))
{
	throw "Marx not found at $MarxDir. Pass -MarxDir."
}

if (-not $WorkshopAddonsDir)
{
	$WorkshopAddonsDir = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\ArmaReforger\addons"
}

if (-not (Test-Path $WorkshopAddonsDir))
{
	throw "Workshop addons folder not found at $WorkshopAddonsDir. Pass -WorkshopAddonsDir."
}

$addonsRoot = Join-Path $repoRoot "addons"
$gproj = Join-Path $addonsRoot "COE2_Contractors\addon.gproj"
if (-not (Test-Path $gproj))
{
	throw "Project not found: $gproj"
}

# This repo's and Marx's addon folders, then the Workshop and game addon roots, as one comma-separated list.
$addonDirs = @(Get-AddonFolders $addonsRoot)
$addonDirs += @(Get-AddonFolders (Join-Path $MarxDir "addons"))
$addonDirs += $WorkshopAddonsDir
$addonDirs += (Join-Path $GameDir "addons")

if (-not $LogsDir)
{
	$logsRoot = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "My Games\ArmaReforgerWorkbench\logs"
	$LogsDir = Join-Path $logsRoot ("logs_" + (Get-Date -Format "yyyy-MM-dd_HH-mm-ss"))
}

$arguments = @("-gproj", "`"$gproj`"", "-addonsDir", "`"$($addonDirs -join ',')`"", "-logsDir", "`"$LogsDir`"")
if (-not $NoScriptAuthorizeAll)
{
	$arguments += "-scriptAuthorizeAll"
}

Write-Host "Workbench: $WorkbenchExe"
Write-Host "Working directory: $GameDir"
Write-Host "Logs: $LogsDir"
Write-Host "Arguments: $($arguments -join ' ')"
if ($DryRun)
{
	return
}

$running = Get-CimInstance Win32_Process -Filter "Name = 'ArmaReforgerWorkbenchSteamDiag.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.Contains($gproj) }
if ($running)
{
	throw "Workbench is already running with $gproj (process $($running.ProcessId -join ', ')). Close it first."
}

New-Item -ItemType Directory -Force -Path $LogsDir | Out-Null
$process = Start-Process -FilePath $WorkbenchExe -ArgumentList $arguments -WorkingDirectory $GameDir -PassThru
Write-Host "Started process $($process.Id)"
