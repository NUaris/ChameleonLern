#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the pinned current official application with the ChameleonLern extension."""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UPSTREAM="5c99d4a39b424cc67ae82bbcfc8ba5ec8f69bf9c"
def output(command):
    return subprocess.check_output([str(x) for x in command],text=True).strip()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cc',default=os.environ.get('ARM_CC','arm-none-eabi-gcc'))
    p.add_argument('--output',type=Path,default=ROOT/'build/firmware')
    p.add_argument('-j',type=int,default=min(os.cpu_count() or 1,8))
    a=p.parse_args();cc=Path(shutil.which(a.cc) or a.cc).resolve()
    if not cc.is_file():p.error('Install ARM GCC, binutils and newlib, or set ARM_CC')
    dest=a.output.resolve();dest.mkdir(parents=True,exist_ok=True);build=dest/'official'
    command=['make','--no-print-directory','-C',ROOT/'firmware/application',f'-j{max(1,a.j)}',f'GNU_INSTALL_ROOT={cc.parent}/',f'GNU_VERSION={output([cc,"-dumpfullversion"])}',f'OUTPUT_DIRECTORY={build}','EXTRA_CFLAGS=-Wno-error']
    newlib=cc.parent.parent/'lib/arm-none-eabi/newlib/thumb/v7e-m+fp/hard'
    if newlib.is_dir():
        command[-1]=f'EXTRA_CFLAGS=-Wno-error -I{cc.parent.parent / "include/newlib"}'
        command += [f'EXTRA_LDFLAGS=-L{newlib}',f'NANO_SPECS=--specs={newlib / "nano.specs"}']
    # New GCC releases warn in the unchanged Nordic SDK. Keep those diagnostics visible.
    result=subprocess.run([str(x) for x in command],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    (dest/'warnings.log').write_text(result.stdout)
    if result.returncode:raise RuntimeError(result.stdout)
    for ext,name in [('out','elf'),('hex','hex'),('bin','bin'),('map','map')]:shutil.copyfile(build/f'application.{ext}',dest/f'chameleon-learning.{name}')
    names=['chameleon-learning.'+ext for ext in ('elf','hex','bin','map')]
    manifest={'commit':output(['git','-C',ROOT,'rev-parse','HEAD']),'dirty':bool(output(['git','-C',ROOT,'status','--porcelain'])),'upstream_commit':UPSTREAM,'version':'ChameleonLern-v0.2.0-alpha.1','compiler':output([cc,'--version']).splitlines()[0],'target':'Chameleon Ultra HW v1 / nRF52840 / S140 7.2.0 / application at 0x27000','signed':False,'sha256':{n:hashlib.sha256((dest/n).read_bytes()).hexdigest() for n in names}}
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(output([cc.with_name('arm-none-eabi-size'),dest/'chameleon-learning.elf']))
    print('Built official firmware plus learning extension at',dest)
if __name__=='__main__':main()
