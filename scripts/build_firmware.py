#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the application using the same source/include list as the Keil project."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / 'firmware/application/project/nfctag.uvprojx'

def run(argv):
    result = subprocess.run([str(x) for x in argv], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(result.stdout)
    return result.stdout

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('ARM_CC', 'arm-none-eabi-gcc'))
    parser.add_argument('--output', type=Path, default=ROOT / 'build/firmware')
    parser.add_argument('-j', type=int, default=min(os.cpu_count() or 1, 8))
    args = parser.parse_args()
    cc = Path(shutil.which(args.cc) or args.cc).resolve()
    if not cc.is_file():
        parser.error('Install gcc-arm-none-eabi, binutils-arm-none-eabi and newlib, or set ARM_CC.')
    output = args.output.resolve(); output.mkdir(parents=True, exist_ok=True)
    project = ET.parse(PROJECT)
    controls = project.find('.//Cads/VariousControls')
    includes = [(PROJECT.parent / p.replace('\\', '/')).resolve() for p in controls.findtext('IncludePath').split(';')]
    includes.append(ROOT / 'firmware/bootloader/sdk/components/toolchain/cmsis/include')
    includes.append(ROOT / 'firmware/application/sdk/modules/nrfx/mdk')
    defines = controls.findtext('Define').split() + ['__START=application_start', '__STARTUP_CLEAR_BSS']
    sources = [(PROJECT.parent / p.text.replace('\\', '/')).resolve() for p in project.findall('.//FilePath') if p.text.endswith('.c') and 'Syscalls_KEIL' not in p.text]
    sources += [ROOT / 'firmware/application/app/startup_gcc.c', ROOT / 'firmware/application/sdk/modules/nrfx/mdk/gcc_startup_nrf52840.S', ROOT / 'firmware/application/sdk/modules/nrfx/mdk/system_nrf52840.c']
    sources = [s.with_name('app_error_handler_gcc.c') if s.name == 'app_error_handler_keil.c' else s for s in sources]
    sources = list(dict.fromkeys(sources))
    common = ['-mcpu=cortex-m4', '-mthumb', '-mfloat-abi=hard', '-mfpu=fpv4-sp-d16', '-Os', '-g3', '-ffunction-sections', '-fdata-sections', '-fshort-enums', '-fno-strict-aliasing', '-fgnu89-inline']
    # Debian's relocated compiler does not discover newlib automatically.
    prefix = cc.parent.parent
    newlib = prefix / 'lib/arm-none-eabi/newlib/thumb/v7e-m+fp/hard'
    link_extra = []
    if newlib.is_dir():
        includes.append(prefix / 'include/newlib')
        link_extra += ['-L' + str(newlib), '-specs=' + str(newlib / 'nosys.specs')]
    else:
        link_extra += ['--specs=nosys.specs']
    flags = common + ['-include', str(ROOT / 'firmware/application/app/utils/compiler_compat.h'), '-std=gnu11', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-sign-compare'] + ['-I' + str(p) for p in includes] + ['-D' + d for d in defines]
    def compile_one(item):
        index, source = item
        obj = output / f'{index:03d}_{source.stem}.o'
        try:
            source_flags = flags if source.suffix == '.c' else common + ['-D' + d for d in defines]
            log = run([cc, *source_flags, '-c', source, '-o', obj])
            return obj, log
        except RuntimeError as error:
            return None, str(error)
    with ThreadPoolExecutor(max_workers=max(1, args.j)) as pool:
        results = list(pool.map(compile_one, enumerate(sources)))
    (output / 'warnings.log').write_text(''.join(log for _, log in results))
    failures = [log for obj, log in results if obj is None]
    if failures:
        raise RuntimeError('\n'.join(failures))
    elf = output / 'chameleon-learning.elf'
    mdk = ROOT / 'firmware/application/sdk/modules/nrfx/mdk'
    run([cc, *common, '-nostartfiles', *link_extra, '-L' + str(mdk), '-T' + str(ROOT / 'firmware/application/project/application.ld'), '-Wl,--gc-sections', '-Wl,-Map=' + str(output / 'chameleon-learning.map'), *[obj for obj, _ in results], '-o', elf])
    objcopy = cc.with_name('arm-none-eabi-objcopy')
    run([objcopy, '-O', 'ihex', elf, output / 'chameleon-learning.hex'])
    run([objcopy, '-O', 'binary', elf, output / 'chameleon-learning.bin'])
    print(run([cc.with_name('arm-none-eabi-size'), elf]), end='')
    files = ['chameleon-learning.elf', 'chameleon-learning.hex', 'chameleon-learning.bin', 'chameleon-learning.map']
    manifest = {'commit': run(['git', '-C', ROOT, 'rev-parse', 'HEAD']).strip(), 'dirty': bool(run(['git', '-C', ROOT, 'status', '--porcelain']).strip()), 'compiler': run([cc, '--version']).splitlines()[0], 'target': 'nRF52840 / S140 7.2.0 / application at 0x27000', 'signed': False, 'sha256': {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in files}}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Built {len(sources)} sources in {output}')

if __name__ == '__main__':
    main()
