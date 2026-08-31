@echo off
> "%~dp0env-%1.txt" (
  echo [whoami] & whoami
  echo [cd] & cd
  echo [set] & set
)
