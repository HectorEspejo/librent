#!/usr/bin/env python3
#
# PROJECT:     librent NTVDM test harness
# LICENSE:     GPL-3.0-or-later (https://spdx.org/licenses/GPL-3.0-or-later)
# PURPOSE:     Prepare a test payload disk, boot LiberNT under QEMU/KVM, run
#              the NTVDM test corpus and report the results
# COPYRIGHT:   Copyright 2026 Hector Espejo

import argparse
import hashlib
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.request
from pathlib import Path

DEFAULT_MANIFEST = Path(__file__).resolve().parent / 'corpus' / 'manifest.json'
DEFAULT_GOLDEN = Path(__file__).resolve().parent / 'corpus' / 'golden'
DEFAULT_CACHE = Path.home() / '.cache' / 'librent' / 'corpus'

PAYLOAD_SECTORS = 65536
DONE_MARK = 'LIBRENT-VMTEST-DONE'


def run(cmd, **kwargs):
    print('+', ' '.join(str(c) for c in cmd), file=sys.stderr)
    return subprocess.run([str(c) for c in cmd], check=True, **kwargs)


def load_manifest(path):
    with open(path, 'r', encoding='utf-8') as handle:
        return json.load(handle)


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as handle:
        for block in iter(lambda: handle.read(1 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def fetch_corpus_item(item, cache_dir):
    cache_dir.mkdir(parents=True, exist_ok=True)
    cached = cache_dir / item['sha256']
    if cached.exists() and sha256_file(cached) == item['sha256']:
        return cached
    print('Downloading {}'.format(item['url']), file=sys.stderr)
    with tempfile.NamedTemporaryFile(dir=cache_dir, delete=False) as temp:
        temp_path = Path(temp.name)
    try:
        urllib.request.urlretrieve(item['url'], temp_path)
    except Exception:
        temp_path.unlink(missing_ok=True)
        raise
    digest = sha256_file(temp_path)
    if digest != item['sha256']:
        temp_path.unlink(missing_ok=True)
        raise SystemExit('ERROR: SHA-256 mismatch for {}: {} (expected {})'.format(
            item['id'], digest, item['sha256']))
    cached.unlink(missing_ok=True)
    temp_path.rename(cached)
    return cached


def resolve_item_files(item, build_dir, cache_dir):
    files = []
    if item['source'] == 'repo':
        source_dirs = [
            Path(build_dir) / 'subsystems' / 'mvdm' / 'ntvdm16',
            Path(build_dir) / 'modules' / 'rostests' / 'tests' / 'ntvdm16',
        ]
        for name in item['files']:
            path = next((source / name for source in source_dirs if (source / name).exists()), None)
            if path is None:
                raise SystemExit('ERROR: missing build output {} for {}'.format(name, item['id']))
            files.append((name.upper(), path))
    elif item['source'] == 'url':
        if len(item['files']) != 1:
            raise SystemExit('ERROR: url corpus items with several files are not supported yet')
        files.append((item['files'][0].upper(), fetch_corpus_item(item, cache_dir)))
    else:
        raise SystemExit('ERROR: unknown source {} for {}'.format(item['source'], item['id']))
    return files


def generate_runtests(items):
    lines = ['@echo off', 'set TESTDRIVE=%~d0', 'set RESULTS=%TESTDRIVE%\\RESULTS',
             'if not exist "%RESULTS%" mkdir "%RESULTS%"']
    drive = False
    for item in items:
        if item.get('keys'):
            lines.append('%SystemRoot%\\system32\\vmtestmark.exe keys {}'.format(item['id']))
        lines.append('rem {}'.format(item['id']))
        if not drive:
            lines.append('%TESTDRIVE%')
            lines.append('cd \\TESTS')
            drive = True
        lines.append('set NTVDM_STATS=%RESULTS%\\{}.stats'.format(item['id']))
        lines.append('set NTVDM_SCREEN_DUMP=%RESULTS%\\{}.screen'.format(item['id']))
        lines.append('set NTVDM_NO_DIALOGS=1')
        lines.append(item['command'])
        lines.append('echo %ERRORLEVEL% > "%RESULTS%\\{}.exit"'.format(item['id']))
    lines.append('%SystemRoot%\\system32\\vmtestmark.exe done')
    lines.append('%SystemRoot%\\system32\\shutdown.exe /s /t 0')
    return '\r\n'.join(lines) + '\r\n'


def cmd_payload(args):
    manifest = load_manifest(args.manifest)
    items = manifest['items']
    payload = Path(args.out)
    payload.parent.mkdir(parents=True, exist_ok=True)
    payload.unlink(missing_ok=True)

    run(['mformat', '-C', '-F', '-v', 'LIBRENT', '-i', payload, '-T', str(PAYLOAD_SECTORS), '::'])
    run(['mmd', '-i', payload, '::LIBRENT', '::TESTS', '::RESULTS'])

    copied = set()
    for item in items:
        for name, path in resolve_item_files(item, args.build_dir, Path(args.cache)):
            if name in copied:
                continue
            copied.add(name)
            run(['mcopy', '-i', payload, path, '::TESTS/{}'.format(name)])

    runtests = Path(args.out).parent / 'RUNTESTS.CMD'
    runtests.write_text(generate_runtests(items), encoding='ascii', newline='')
    run(['mcopy', '-i', payload, runtests, '::LIBRENT/RUNTESTS.CMD'])

    print('Payload disk {} ready with {} item(s)'.format(payload, len(items)), file=sys.stderr)
    return 0


class Qmp:
    def __init__(self, path, timeout):
        deadline = time.monotonic() + timeout
        self.sock = None
        while time.monotonic() < deadline:
            try:
                self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                self.sock.connect(str(path))
                break
            except OSError:
                self.sock = None
                time.sleep(0.2)
        if self.sock is None:
            raise SystemExit('ERROR: cannot connect to the QMP socket {}'.format(path))
        self.sock.settimeout(5.0)
        self.buf = b''
        self.readline()
        self.execute('qmp_capabilities')

    def readline(self):
        while b'\n' not in self.buf:
            block = self.sock.recv(4096)
            if not block:
                raise SystemExit('ERROR: QMP connection closed')
            self.buf += block
        line, self.buf = self.buf.split(b'\n', 1)
        return line

    def execute(self, command, arguments=None):
        message = {'execute': command}
        if arguments:
            message['arguments'] = arguments
        self.sock.sendall(json.dumps(message).encode() + b'\n')
        while True:
            reply = json.loads(self.readline())
            if 'return' in reply or 'error' in reply:
                return reply


def cmd_run(args):
    payload = Path(args.payload)
    results = Path(args.results)
    results.mkdir(parents=True, exist_ok=True)
    serial = Path(args.serial_log)
    serial.parent.mkdir(parents=True, exist_ok=True)
    serial.unlink(missing_ok=True)
    qmp_path = serial.parent / 'qmp.sock'
    qmp_path.unlink(missing_ok=True)

    cmd = [
        args.qemu,
        '-machine', 'pc,accel=kvm:tcg',
        '-m', '2048',
        '-display', 'none',
        '-no-reboot',
        '-drive', 'file={},format=raw,snapshot=on,if=ide,index=0'.format(args.image),
        '-drive', 'file={},format=raw,if=ide,index=1'.format(payload),
        '-serial', 'file:{}'.format(serial),
        '-qmp', 'unix:{},server,nowait'.format(qmp_path),
    ]

    print('+', ' '.join(cmd), file=sys.stderr)
    qemu = subprocess.Popen(cmd)
    deadline = time.monotonic() + args.timeout
    done = False
    try:
        while time.monotonic() < deadline:
            if serial.exists():
                text = serial.read_text(errors='replace')
                if DONE_MARK in text:
                    done = True
                    break
            if qemu.poll() is not None:
                break
            time.sleep(0.5)
    finally:
        if done:
            stop = time.monotonic() + 30
            while qemu.poll() is None and time.monotonic() < stop:
                time.sleep(0.5)
        if qemu.poll() is None:
            try:
                Qmp(qmp_path, 10).execute('quit')
            except SystemExit:
                qemu.kill()
            qemu.wait()

    if not done:
        print('ERROR: the test run did not finish within {} s'.format(args.timeout), file=sys.stderr)
        return 1

    print('Test run finished; serial log in {}'.format(serial), file=sys.stderr)
    return 0


def extract_results(payload, results):
    if results.exists():
        shutil.rmtree(results)
    results.mkdir(parents=True)
    out = subprocess.run(
        ['mcopy', '-s', '-i', str(payload), '::RESULTS/*', str(results) + '/'],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if out.returncode != 0:
        print('ERROR: cannot extract RESULTS from {}'.format(payload), file=sys.stderr)
        return False
    return True


def cmd_report(args):
    manifest = load_manifest(args.manifest)
    results = Path(args.results)
    golden_dir = Path(args.golden)

    if args.payload and not extract_results(Path(args.payload), results):
        return 1

    failures = 0
    rows = []
    for item in manifest['items']:
        item_id = item['id']
        expect = item.get('expect', {})
        problems = []

        exit_file = results / '{}.exit'.format(item_id)
        if expect.get('exit_code') is not None:
            if not exit_file.exists():
                problems.append('missing {}'.format(exit_file.name))
            else:
                got = int(exit_file.read_text().strip() or '0')
                if got != expect['exit_code']:
                    problems.append('exit {} != {}'.format(got, expect['exit_code']))

        if expect.get('stdout_golden'):
            out_file = results / '{}.out'.format(item_id)
            expected_file = golden_dir / expect['stdout_golden']
            if not out_file.exists():
                problems.append('missing {}'.format(out_file.name))
            elif not expected_file.exists():
                problems.append('missing golden {}'.format(expected_file))
            elif out_file.read_bytes() != expected_file.read_bytes():
                problems.append('stdout differs from {}'.format(expected_file.name))

        if problems:
            failures += 1
            rows.append((item_id, 'FALLO: ' + '; '.join(problems)))
        else:
            rows.append((item_id, 'ok'))

    for item_id, status in rows:
        print('{:<20} {}'.format(item_id, status))
    print('{}/{} items ok'.format(len(rows) - failures, len(rows)))
    return 1 if failures else 0


def add_common_arguments(parser, need_run):
    parser.add_argument('--manifest', type=Path, default=DEFAULT_MANIFEST)
    if need_run:
        parser.add_argument('--image', type=Path, required=True)
        parser.add_argument('--qemu', default='qemu-system-x86_64')


def main():
    parser = argparse.ArgumentParser(description='librent NTVDM test harness')
    sub = parser.add_subparsers(dest='command', required=True)

    payload = sub.add_parser('payload', help='build the test payload disk')
    add_common_arguments(payload, False)
    payload.add_argument('--build-dir', type=Path, required=True)
    payload.add_argument('--cache', type=Path, default=DEFAULT_CACHE)
    payload.add_argument('--out', type=Path, required=True)
    payload.set_defaults(func=cmd_payload)

    run_parser = sub.add_parser('run', help='boot LiberNT and run the tests')
    add_common_arguments(run_parser, True)
    run_parser.add_argument('--payload', type=Path, required=True)
    run_parser.add_argument('--timeout', type=int, default=180)
    run_parser.add_argument('--results', type=Path, required=True)
    run_parser.add_argument('--serial-log', type=Path, required=True)
    run_parser.set_defaults(func=cmd_run)

    report = sub.add_parser('report', help='compare the results with the golden files')
    add_common_arguments(report, False)
    report.add_argument('--results', type=Path, required=True)
    report.add_argument('--golden', type=Path, default=DEFAULT_GOLDEN)
    report.add_argument('--payload', type=Path)
    report.set_defaults(func=cmd_report)

    everything = sub.add_parser('all', help='payload, run and report')
    add_common_arguments(everything, True)
    everything.add_argument('--build-dir', type=Path, required=True)
    everything.add_argument('--cache', type=Path, default=DEFAULT_CACHE)
    everything.add_argument('--timeout', type=int, default=180)

    args = parser.parse_args()

    if args.command == 'all':
        work = args.image.parent / 'vmtest'
        args.out = work / 'payload.img'
        args.results = work / 'results'
        args.serial_log = work / 'serial.log'
        status = cmd_payload(args)
        if status:
            return status
        args.payload = args.out
        status = cmd_run(args)
        if status:
            return status
        args.golden = DEFAULT_GOLDEN
        return cmd_report(args)

    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
