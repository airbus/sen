@echo off
rem Windows launcher for the Sen MCP gateway, mirroring the POSIX `sen-mcp-gateway` script:
rem resolves the JS bundle alongside this file and runs node against it. MCP hosts on Windows
rem cannot spawn the shell script, so the registration command names this one.
setlocal

set "BUNDLE=%~dp0sen-mcp-gateway.cjs"

if not exist "%BUNDLE%" (
  echo sen-mcp-gateway: bundle not found at %BUNDLE% 1>&2
  exit /b 1
)

where node >nul 2>&1
if errorlevel 1 (
  echo sen-mcp-gateway: 'node' not found on PATH. Install Node 22.20.0+ or activate the Conan env. 1>&2
  exit /b 1
)

rem Floor only, as in the POSIX script: the bundle is built --target=node22, so an older runtime
rem trips over syntax it cannot parse with nothing pointing at the version. Newer is fine.
rem No `>` in the expression: it would be read as a redirection when cmd re-parses this line.
set "NODE_VERSION=unknown"
set "NODE_NUMBER=0"
for /f "usebackq tokens=1,2" %%a in (`node -p "var v = process.versions.node.split('.').map(Number); process.versions.node + ' ' + (v[0] * 1000000 + v[1] * 1000 + v[2])"`) do (
  set "NODE_VERSION=%%a"
  set "NODE_NUMBER=%%b"
)
if %NODE_NUMBER% LSS 22020000 (
  echo sen-mcp-gateway: node %NODE_VERSION% is too old, this build needs 22.20.0 or newer. 1>&2
  echo sen-mcp-gateway: install a newer Node or activate the Conan env. 1>&2
  exit /b 1
)

node --enable-source-maps "%BUNDLE%" %*
exit /b %ERRORLEVEL%
