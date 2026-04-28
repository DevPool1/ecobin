@echo off
:: ============================================
:: ECOBIN — Iniciar Auto Commit Watcher
:: ============================================
:: Duplo-clique para iniciar o watcher.
:: Faz commit + push a cada 60 segundos.
:: Para parar: fechar a janela ou Ctrl+C
:: ============================================

title ECOBIN Auto-Commit Watcher
cd /d "%~dp0.."
powershell -ExecutionPolicy Bypass -File "%~dp0auto_commit.ps1" -Interval 60
pause
