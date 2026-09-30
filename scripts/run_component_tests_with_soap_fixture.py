# SPDX-License-Identifier: MIT
# Copyright (c) Robert Vokac and contributors
"""Run all component tests using an isolated original Yacht SOAP fixture.

Usage: python3 scripts/run_component_tests_with_soap_fixture.py <original-bin-dir> [build]
No sample source or shared configuration is changed; the disposable fixture uses a private port.
"""
from pathlib import Path
import os, socket, subprocess, shutil, time, sys
root=Path(__file__).resolve().parents[1]
if len(sys.argv) not in (2,3):raise SystemExit(__doc__)
source=Path(sys.argv[1]).resolve()
build=Path(sys.argv[2]) if len(sys.argv)==3 else Path('build')
if not build.is_absolute():build=root/build
if not (build/'SharpRuntimeTests_ServiceModel').is_file():raise SystemExit('Configure/build the existing runtime build first')
probe=root/'build-probe';probe.mkdir(exist_ok=True)
exe=probe/'gs-007b-soap.exe';config=probe/'gs-007b-soap.exe.config'
assert not exe.exists() and not config.exists(),'do not overwrite another probe'
with socket.socket() as choose:
 choose.bind(('127.0.0.1',0));port=choose.getsockname()[1]
shutil.copyfile(source/'Server.exe',exe)
config.write_text((source/'Server.exe.config').read_text(encoding='utf-8-sig').replace('http://localhost:8888/GameServer/',f'http://localhost:{port}/GameServer/'))
env=os.environ.copy();env['MONO_PATH']=str(source);env.pop('DISPLAY',None);env['WAYLAND_DISPLAY']=''
process=None
try:
 with (build/'gs-007b-soap.log').open('w') as log:
  process=subprocess.Popen(['mono',str(exe)],cwd=probe,stdin=subprocess.PIPE,stdout=log,stderr=log,env=env,text=True)
  for attempt in range(100):
   if process.poll() is not None:raise RuntimeError('private SOAP fixture failed')
   try:
    with socket.create_connection(('127.0.0.1',port),timeout=.2):break
   except OSError:time.sleep(.1)
  else:raise RuntimeError('private SOAP fixture timed out')
  env['SHARP_RUNTIME_SOAP_ENDPOINT']=f'http://localhost:{port}/GameServer/'
  with (build/'gs-007b-components-live.log').open('w') as result:
   completed=subprocess.run(['scripts/run_component_tests.sh',str(build)],cwd=root,env=env,stdout=result,stderr=subprocess.STDOUT)
   if completed.returncode:raise RuntimeError('component gate failed; inspect components-live.log')
finally:
 if process and process.poll() is None:
  process.stdin.write('\n');process.stdin.flush()
  try:process.wait(timeout=10)
  except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10)
 exe.unlink(missing_ok=True);config.unlink(missing_ok=True)
print('Full runtime gate used unchanged original Yacht SOAP fixture on private port; fixture stopped/removed')
