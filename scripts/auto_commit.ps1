# ============================================
# ECOBIN — Auto Commit & Push
# ============================================
# Vigia a pasta do projeto e faz commit + push
# automaticamente quando deteta alterações.
#
# Uso:
#   .\scripts\auto_commit.ps1              (intervalo padrão: 60s)
#   .\scripts\auto_commit.ps1 -Interval 30 (a cada 30 segundos)
#   .\scripts\auto_commit.ps1 -NoPush      (sem push, só commit)
#
# Para parar: Ctrl+C
# ============================================

param(
    [int]$Interval = 60,       # Intervalo em segundos entre verificações
    [switch]$NoPush            # Se ativado, não faz push automático
)

$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)

Write-Host ""
Write-Host "  ♻️  ECOBIN — Auto Commit Watcher" -ForegroundColor Green
Write-Host "  =================================" -ForegroundColor Green
Write-Host "  📂 Pasta:     $projectRoot" -ForegroundColor Cyan
Write-Host "  ⏱️  Intervalo: ${Interval}s" -ForegroundColor Cyan
Write-Host "  🚀 Push:      $(if ($NoPush) { 'Desativado' } else { 'Ativado' })" -ForegroundColor Cyan
Write-Host "  ⛔ Parar:     Ctrl+C" -ForegroundColor Yellow
Write-Host ""

function Get-CommitMessage {
    # Gera uma mensagem de commit inteligente baseada nos ficheiros alterados
    $status = git -C $projectRoot status --porcelain 2>$null
    if (-not $status) { return $null }

    $lines = $status -split "`n" | Where-Object { $_.Trim() -ne "" }
    $count = $lines.Count

    # Detetar tipos de alteração
    $areas = @()
    foreach ($line in $lines) {
        $file = ($line.Substring(3)).Trim()
        if ($file -match "^firmware/")       { $areas += "firmware" }
        elseif ($file -match "^gateway/")    { $areas += "gateway" }
        elseif ($file -match "^docs/")       { $areas += "docs" }
        elseif ($file -match "^3d_models/")  { $areas += "3d" }
        elseif ($file -match "^tests/")      { $areas += "tests" }
        elseif ($file -match "^\.github/")   { $areas += "ci" }
        elseif ($file -match "\.(md|txt)$")  { $areas += "docs" }
        else                                 { $areas += "chore" }
    }

    $areas = $areas | Select-Object -Unique
    $scope = ($areas -join ",")
    $timestamp = Get-Date -Format "HH:mm"

    if ($areas.Count -eq 1) {
        $prefix = switch ($areas[0]) {
            "firmware" { "feat(firmware)" }
            "gateway"  { "feat(gateway)" }
            "docs"     { "docs" }
            "3d"       { "feat(3d)" }
            "tests"    { "test" }
            "ci"       { "ci" }
            default    { "chore" }
        }
    } else {
        $prefix = "chore($scope)"
    }

    return "$prefix`: auto-save $count file(s) at $timestamp"
}

# Loop principal
try {
    while ($true) {
        # Verificar se há alterações
        $changes = git -C $projectRoot status --porcelain 2>$null

        if ($changes) {
            $msg = Get-CommitMessage

            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] 🔍 Alterações detetadas!" -ForegroundColor Yellow

            # Stage todas as alterações
            git -C $projectRoot add -A 2>$null

            # Commit
            $commitResult = git -C $projectRoot commit -m $msg 2>&1
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ✅ Commit: $msg" -ForegroundColor Green

            # Push (se ativado)
            if (-not $NoPush) {
                $pushResult = git -C $projectRoot push 2>&1
                if ($LASTEXITCODE -eq 0) {
                    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] 🚀 Push feito com sucesso!" -ForegroundColor Cyan
                } else {
                    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ⚠️  Push falhou (offline?). Tentará novamente." -ForegroundColor Red
                }
            }

            Write-Host ""
        } else {
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] 💤 Sem alterações..." -ForegroundColor DarkGray
        }

        Start-Sleep -Seconds $Interval
    }
} finally {
    Write-Host ""
    Write-Host "  👋 Auto-commit parado." -ForegroundColor Yellow
}
