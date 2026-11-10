"""Real Linux process/socket tests. No third-party dependencies."""
import json
import os
import pathlib
import signal
import socket
import struct
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
os.chdir(ROOT)
EVIDENCE = ROOT / 'evidence'
EVIDENCE.mkdir(exist_ok=True)
results = []

def frame(text):
    data = text.encode()
    return struct.pack('!I', len(data)) + data

def exact(s, n):
    data = b''
    while len(data) < n:
        part = s.recv(n-len(data))
        if not part:
            raise EOFError('peer closed')
        data += part
    return data

def receive(s):
    n = struct.unpack('!I', exact(s, 4))[0]
    assert 0 < n <= 4096
    return exact(s, n).decode()

class Server:
    def __init__(self, baseline=False, debug=False, build=1):
        with socket.socket() as s:
            s.bind(('127.0.0.1', 0))
            self.port = s.getsockname()[1]
        name = f'{"baseline" if baseline else "robust"}-build{build}-debug{int(debug)}'
        self.path = EVIDENCE / (name + '.log')
        self.log = self.path.open('w')
        args = [f'build/{build}/server', '--port', str(self.port), '--timeout-ms', '800']
        if baseline:
            args += ['--baseline']
        if debug:
            args += ['--debug']
        self.p = subprocess.Popen(args, stdout=self.log, stderr=self.log)
        for _ in range(100):
            if 'listening' in self.path.read_text():
                break
            if self.p.poll() is not None:
                raise RuntimeError('server failed to start')
            time.sleep(.02)
        else:
            self.close()
            raise RuntimeError('startup timed out')

    def connect(self):
        return socket.create_connection(('127.0.0.1', self.port), timeout=3)

    def children(self):
        path = pathlib.Path(f'/proc/{self.p.pid}/task/{self.p.pid}/children')
        return path.read_text().split()

    def close(self):
        self.p.send_signal(signal.SIGTERM)
        try:
            self.p.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.p.kill()
            raise
        finally:
            self.log.close()
        assert self.p.returncode == 0

def record(name, detail):
    results.append({'test': name, 'result': 'PASS', 'detail': detail})
    print(f'PASS {name}: {detail}', flush=True)

subprocess.run(['make', 'DEBUG=0'], check=True)
for build, debug in [(1, True), (1, False), (0, True)]:
    srv = Server(debug=debug, build=build)
    try:
        for command in ['SYSINFO', 'MEMORY', 'DISK', 'PING', 'QUIT']:
            run = subprocess.run([f'build/{build}/client', '127.0.0.1', str(srv.port), command, '--debug'], capture_output=True, text=True, check=True)
            assert run.stdout.startswith('OK ')
            if command == 'SYSINFO':
                record(f'features build={build} debug={debug}', run.stdout.strip())
        time.sleep(.15)
    finally:
        srv.close()
    has_debug = '[DEBUG' in srv.path.read_text()
    assert has_debug == (build == 1 and debug)
record('compile/runtime debug', 'debug messages only when DEBUG=1 AND --debug')

srv = Server(debug=True)
try:
    clients = [srv.connect() for _ in range(12)]
    for s in clients:
        s.sendall(frame('HOLD 3'))
    time.sleep(.3)
    children = srv.children()
    assert len(children) == 12, children
    ps = subprocess.check_output(['ps', '-o', 'pid,ppid,stat,args', '-p', ','.join([str(srv.p.pid)] + children)], text=True)
    (EVIDENCE / 'concurrency-processes.txt').write_text(ps)
    begin = time.monotonic()
    replies = [receive(s) for s in clients]
    assert len({r.split('pid=')[1] for r in replies}) == 12
    for s in clients:
        s.close()
    record('12 concurrent clients', f'12 live child PIDs={children}; 12 HOLD responses received in {time.monotonic()-begin:.2f}s after snapshot')
    time.sleep(1)
    assert not srv.children(), srv.children()
    record('child reaping', 'no children remain after clients close')
    for payload in [struct.pack('!I', 5000), struct.pack('!I', 0), frame('PI\x00NG')]:
        with srv.connect() as s:
            s.sendall(payload)
            assert s.recv(1) == b''
    with srv.connect() as s:
        s.sendall(frame('PING'))
        assert receive(s).startswith('OK PONG')
    record('malformed frames', 'oversized, empty and NUL frames rejected; later client succeeds')
finally:
    srv.close()

for baseline in [False, True]:
    srv = Server(baseline=baseline)
    label = 'baseline' if baseline else 'robust'
    try:
        with srv.connect() as s:
            data = frame('SYSINFO')
            s.sendall(data[:5])
            time.sleep(.2)
            try:
                s.sendall(data[5:])
                reply = receive(s)
            except (OSError, EOFError):
                reply = 'connection closed: incomplete recv'
            assert reply.startswith('OK') != baseline
            record(f'fragmentation {label}', reply)
        with srv.connect() as s:
            s.sendall(b'\x00')
            time.sleep(1.1)
            live = len(srv.children())
            assert (live > 0) == baseline
            record(f'stalled sender {label}', f'child count after 1.1s={live}; timeout=800ms')
        time.sleep(.2)
        with srv.connect() as s:
            s.sendall(frame('INVALID'))
            try:
                reply = receive(s)
                assert reply.startswith('ERR')
                s.sendall(frame('PING'))
                reply += '; ' + receive(s)
            except (OSError, EOFError):
                reply = 'connection closed; cannot continue session'
            assert ('OK PONG' in reply) != baseline
            record(f'unknown command {label}', reply)
    finally:
        srv.close()

subprocess.run(['ldd', 'build/1/server'], check=True, stdout=(EVIDENCE / 'server-ldd.txt').open('w'))
subprocess.run(['ldd', 'build/1/client'], check=True, stdout=(EVIDENCE / 'client-ldd.txt').open('w'))
metadata = {'platform': subprocess.check_output(['uname', '-srmo'], text=True).strip(), 'results': results}
(EVIDENCE / 'results.json').write_text(json.dumps(metadata, ensure_ascii=False, indent=2)+'\n')
print(f'All {len(results)} checks passed. Evidence: {EVIDENCE}')
