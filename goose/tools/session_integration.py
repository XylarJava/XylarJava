#!/usr/bin/env python3
"""Native stream integration, with an explicit pipe backend and optional TCP.

No account, Java process, game jar, network service or player data is used.
The peer independently encodes protocol-777 frames; OpenSSL CLI supplies the
peer's RSA decryption and CFB8 stream. This is integration, not a Rust oracle.
"""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import random
import select
import socket
import struct
import subprocess
import tempfile
import threading
import uuid
import zlib

OPENSSL = 'openssl'


def varint(n):
    n &= 0xffffffff
    out = bytearray()
    while n >= 128:
        out.append((n & 127) | 128)
        n >>= 7
    return bytes(out + bytes([n]))


def string(s):
    data = s.encode() if isinstance(s, str) else s
    return varint(len(data)) + data


def number(data, at=0):
    value = 0
    for i in range(5):
        if at + i >= len(data):
            return None
        b = data[at + i]
        value |= (b & 127) << (7 * i)
        if b < 128:
            return value, at + i + 1
    raise AssertionError('peer received an oversized VarInt')


def framed(body, threshold=-1):
    content = body
    if threshold >= 0:
        content = (varint(len(body)) + zlib.compress(body)) if len(body) >= threshold else b'\0' + body
    return varint(len(content)) + content


def unframe(content, threshold):
    if threshold < 0:
        return content
    size, at = number(content)
    data = content[at:]
    if size:
        data = zlib.decompress(data)
        assert len(data) == size
    return data


def cipher(data, secret, decrypt=False):
    cmd = [OPENSSL, 'enc', '-aes-128-cfb8', '-K', secret.hex(), '-iv', secret.hex(), '-nopad']
    if decrypt:
        cmd.append('-d')
    return subprocess.run(cmd, input=data, check=True, capture_output=True, timeout=10).stdout


class Peer:
    def __init__(self, probe, mode, limit=65536):
        env = dict(os.environ, XYLAR_TEST_READ_LIMIT=str(limit))
        self.p = subprocess.Popen([str(probe), mode], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        self.threshold = -1
        self.secret = None
        self.incoming = b''
        self.in_at = 0
        self.outgoing = b''

    def send(self, body, fragment=False):
        self.raw(framed(body, self.threshold), fragment)

    def raw(self, data, fragment=False):
        if self.secret:
            before = len(self.outgoing)
            self.outgoing += data
            data = cipher(self.outgoing, self.secret)[before:]
        if fragment:
            for b in data:
                self.p.stdin.write(bytes([b]))
                self.p.stdin.flush()
        else:
            self.p.stdin.write(data)
            self.p.stdin.flush()

    def read(self):
        while True:
            plain = cipher(self.incoming, self.secret, True) if self.secret else self.incoming
            prefix = number(plain, self.in_at)
            if prefix:
                size, at = prefix
                if len(plain) >= at + size:
                    self.in_at = at + size
                    return unframe(plain[at:at + size], self.threshold)
            ready, _, _ = select.select([self.p.stdout], [], [], 5)
            if not ready:
                raise AssertionError('native session did not send its expected reply')
            data = os.read(self.p.stdout.fileno(), 65536)
            if not data:
                raise AssertionError('native session ended before its expected reply')
            self.incoming += data

    def finish(self, ok=True, error=None, wait=False):
        if wait:
            self.p.wait(timeout=5)
        self.p.stdin.close()
        self.p.stdin = None
        remaining, diagnostic = self.p.communicate(timeout=5)
        assert not remaining, f'unexpected extra output: {remaining[:50]!r}'
        result = json.loads(diagnostic)
        assert result['ok'] == ok, result
        assert self.p.returncode == (0 if ok else 1), result
        if error:
            assert error in result['error'], result
        return result

    def stop(self):
        if self.p.poll() is None:
            self.p.kill()
        self.p.communicate(timeout=5)
        for f in (self.p.stdin, self.p.stdout, self.p.stderr):
            if f:
                f.close()


@contextlib.contextmanager
def peer(probe, mode='login', limit=65536):
    p = Peer(probe, mode, limit)
    try:
        if mode in ('login', 'configure', 'terrain'):
            expected = varint(0) + varint(777) + string('localhost') + struct.pack('>H', 25565) + varint(2)
            assert p.read() == expected
            offline = uuid.UUID(bytes=hashlib.md5(b'OfflinePlayer:Xylar').digest(), version=3)
            assert p.read() == b'\0' + string('Xylar') + offline.bytes
        yield p
    finally:
        p.stop()


def finished(properties=b'\0'):
    return b'\x02' + bytes(range(16)) + string('Xylar') + properties + b'\xff' * 16


def login_checks(probe, work):
    count = 0
    for limit in (1, 7, 65536):
        with peer(probe, limit=limit) as p:
            # Unknown packets, a custom query and cookie response, coalesced
            # with compression negotiation and the first compressed frame.
            query = b'\x04' + varint(17) + string('test:query') + b'opaque'
            cookie = b'\x05' + string('test:cookie')
            p.raw(framed(b'\x63opaque') + framed(query) + framed(cookie) + framed(b'\x03' + varint(64)))
            # The responses to earlier packets still use the old framing.
            assert p.read() == b'\x02' + varint(17) + b'\0'
            assert p.read() == b'\x04' + string('test:cookie') + b'\0'
            p.threshold = 64
            props = varint(1) + string('textures') + string('x' * 10000) + b'\0'
            # Empty compressed frame is skipped; following packet is accepted.
            p.raw(framed(b'', 64) + framed(finished(props), 64))
            assert p.read() == b'\x03'
            result = p.finish()
            assert result['detail'] == '00010203-0405-0607-0809-0a0b0c0d0e0f'
            assert result['sent'] == 5 and result['received'] == 5, result
        count += 1
    with peer(probe) as p:
        p.send(finished(), fragment=True)
        assert p.read() == b'\x03'
        p.finish()
    count += 1
    failures = [
        (b'\x00' + string('{"text":"closed for maintenance"}'), 'closed for maintenance'),
        (b'\x01' + string('') + string(b'bad') + string(b'abcd') + b'\x01', 'Microsoft account'),
        (b'\x01' + string('') + string(b'bad') + string(b'abcd') + b'\0', 'Malformed RSA'),
        (b'\x02\0', 'needed'),
        (b'\x03' + varint(64) + b'\0', 'extra bytes'),
    ]
    for body, error in failures:
        with peer(probe) as p:
            p.send(body)
            p.finish(False, error)
        count += 1
    with peer(probe) as p:
        p.finish(False, 'Connection closed')
    count += 1
    with peer(probe) as p:
        p.finish(False, 'Timed out', wait=True)
    count += 1
    with peer(probe) as p:
        p.raw(b'\x80\x80\x80')
        p.finish(False, 'length wider than 21 bits')
    count += 1
    # Independent RSA private operation checks both encrypted secret and token.
    for bits in (1024, 2048):
        private = work / f'private-{bits}.pem'
        subprocess.run([OPENSSL, 'genpkey', '-algorithm', 'RSA', '-pkeyopt', f'rsa_keygen_bits:{bits}',
                        '-out', str(private)], check=True, capture_output=True, timeout=15)
        public = subprocess.run([OPENSSL, 'pkey', '-in', str(private), '-pubout', '-outform', 'DER'],
                                check=True, capture_output=True, timeout=5).stdout
        mode = 'configure' if bits == 2048 else 'login'
        with peer(probe, mode, limit=7) as p:
            token = b'\x00\xff\x10\x80'
            p.send(b'\x01' + string('') + string(public) + string(token) + b'\0')
            response = p.read()
            assert response[0] == 1
            keylen, at = number(response, 1)
            encrypted_key = response[at:at + keylen]
            challenge_len, start = number(response, at + keylen)
            encrypted_challenge = response[start:]
            assert len(encrypted_challenge) == challenge_len == keylen == bits // 8
            def decrypt(data):
                return subprocess.run([OPENSSL, 'pkeyutl', '-decrypt', '-inkey', str(private),
                                       '-pkeyopt', 'rsa_padding_mode:pkcs1'], input=data,
                                      check=True, capture_output=True, timeout=5).stdout
            secret = decrypt(encrypted_key)
            assert len(secret) == 16 and decrypt(encrypted_challenge) == token
            # The next bytes are all AES/CFB8, including frame length prefixes.
            p.secret = secret
            p.incoming = b''
            p.in_at = 0
            p.send(b'\x03' + varint(64))
            p.threshold = 64
            p.send(finished())
            assert p.read() == b'\x03'
            if mode == 'configure':
                p.read(); p.read()  # brand and client information, encrypted
                p.raw(framed(b'\x03', 64) + framed(b'\x2aplay', 64))
                assert p.read() == b'\x03'
            result = p.finish()
            assert result['sent'] == (7 if mode == 'configure' else 4), result
            assert result['received'] == (5 if mode == 'configure' else 3), result
        count += 1
        with peer(probe) as p:
            p.send(b'\x01' + string('') + string(public) + string(b'x' * (bits // 8)) + b'\0')
            p.finish(False, 'RSA encryption failed')
        count += 1
    return count


def status_checks(probe):
    count = 0
    for status, error in [('{"version":{"name":"26.3","protocol":777},"description":"ciao 🌍"}', None),
                          ('not json', 'Invalid status JSON')]:
        with peer(probe, 'status', 1) as p:
            assert p.read() == b'\0' + varint(777) + string('localhost') + struct.pack('>H', 25565) + b'\x01'
            assert p.read() == b'\0'
            p.send(b'\0' + string(status))
            if error:
                p.finish(False, error)
            else:
                assert p.read() == b'\x01' + struct.pack('>q', 1)
                p.send(b'\x01' + struct.pack('>q', 1))
                result = p.finish()
                assert result['detail'] == status and result['sent'] == 3 and result['received'] == 2
        count += 1
    return count


def configuration_checks(probe):
    count = 0
    for limit in (1, 65536):
        with peer(probe, 'configure', limit) as p:
            p.send(finished())
            assert p.read() == b'\x03'
            assert p.read() == b'\x02' + string('minecraft:brand') + string('xylar-goose')
            info = b'\x00' + string('en_us') + b'\x08\x00\x01\x7f\x01\x00\x01\x00'
            assert p.read() == info
            keepalive = struct.pack('>q', -9223372036854775808)
            ping = struct.pack('>i', -3)
            p.send(b'\x04' + keepalive)
            assert p.read() == b'\x04' + keepalive
            p.send(b'\x05' + ping)
            assert p.read() == b'\x05' + ping
            p.send(b'\x0f' + varint(1) + string('minecraft') + string('core') + string('26.3'))
            assert p.read() == b'\x07\x00'
            p.send(b'\x0b' + string('test:cookie') + string(b'old'))
            p.send(b'\x0b' + string('test:cookie') + string(b'new'))
            p.send(b'\x00' + string('test:cookie'))
            assert p.read() == b'\x01' + string('test:cookie') + b'\x01' + string(b'new')
            # A binary compound registry value, tags and enabled features.
            data = bytes.fromhex('0a030001780000000700')
            p.send(b'\x07' + string('minecraft:test') + varint(1) + string('test:entry') + b'\x01' + data)
            p.send(b'\x0e' + varint(1) + string('minecraft:block') + varint(1) + string('minecraft:logs') + varint(2) + varint(1) + varint(300))
            p.send(b'\x0d' + varint(1) + string('minecraft:vanilla'))
            # Coalesced phase transition: no decoder may consume the play packet
            # as configuration. The driver reports its untouched packet id.
            p.raw(framed(b'\x03') + framed(b'\x2a\x01\x02'))
            assert p.read() == b'\x03'
            result = p.finish()
            assert result['detail'] == 'registries:1 tags:1 features:1 cookies:2 next:42', result
        count += 1
    for packet, error in [(b'\x02\x08\x00\x03bye', 'Server disconnected'),
                          (b'\x14' + string('Please accept'), 'requires user acceptance'),
                          (b'\x07' + string('minecraft:test') + b'\x01' + string('test:x') + b'\x01\x0a', 'NBT:')]:
        with peer(probe, 'configure') as p:
            p.send(finished())
            assert p.read() == b'\x03'
            p.read(); p.read()
            p.send(packet)
            p.finish(False, error)
        count += 1
    return count


def terrain_checks(probe):
    count = 0
    section = bytes.fromhex('0100000000010002')
    chunk = b'\0' + string(section) + b'\x01' + bytes.fromhex('3fffc4020a030001780000000700')
    light = string(b'\x08') + b'\0' + b'\0' + string(b'\x02') + b'\x01' + string(bytes([0xf0])*2048) + b'\0'
    body = b'\x2e' + struct.pack('>ii', -123, 456) + chunk + light
    cases = [(body, None), (body[:-1], 'needed'),
             (body+b'\0', 'larger than expected'),
             (b'\x20\x08\0\3bye', 'Server disconnected'),
             (b'\x78', 'reconfiguration')]
    for limit in [1, 65536]:
        for packet, error in cases:
            with peer(probe, 'terrain', limit) as p:
                p.send(b'\x03'+varint(64)); p.threshold = 64
                p.send(finished()); assert p.read() == b'\x03'
                p.read(); p.read()
                p.raw(framed(b'\x03',64)+framed(b'\0',64))
                assert p.read() == b'\x03'
                p.send(b'\x2d'+struct.pack('>q',-9223372036854775808))
                assert p.read() == b'\x1c'+struct.pack('>q',-9223372036854775808)
                p.send(b'\x3e'+struct.pack('>i',-3))
                assert p.read() == b'\x2d'+struct.pack('>i',-3)
                p.send(b'\x0c');p.send(b'\x0b'+varint(0))
                assert p.read() == b'\x0b'+struct.pack('>f',64.0)
                p.send(packet)
                result = p.finish(error is None, error)
                if error is None:
                    assert result['detail'] == 'registries:0 tags:0 features:0 cookies:0 chunk:-123,456 bytes:8 entities:1', result
            count += 1
    return count


def player_checks(probe):
    def f32(x):return struct.unpack('>f',struct.pack('>f',x))[0]
    spawn=varint(0)+string('minecraft:overworld')+struct.pack('>q',123)+varint(1)+varint(0)+bytes(3)+varint(0)+varint(63)
    login=b'\x32'+struct.pack('>i',42)+b'\0'+varint(1)+string('minecraft:overworld')+varint(20)+varint(8)+varint(8)+bytes([0,1,0])+spawn+bytes(2)
    chunk=b'\x2e'+struct.pack('>ii',0,0)+b'\0'+string(bytes.fromhex('0000000000000000')*24)+b'\0'+bytes(6)
    count=0
    for limit in [1,65536]:
        with peer(probe,'terrain',limit) as p:
            p.send(b'\x03'+varint(64));p.threshold=64
            p.send(finished());assert p.read()==b'\x03';p.read();p.read()
            if limit==65536:
                def dimension(height):
                    tag=b'\x0a'+b'\x03\0\5min_y'+struct.pack('>i',-32)+b'\x03\0\6height'+struct.pack('>i',height)+b'\0'
                    return b'\x07'+string('minecraft:dimension_type')+b'\x01'+string('pack:custom')+b'\x01'+tag
                p.send(dimension(16));p.send(dimension(32))
            p.raw(framed(b'\x03',64)+framed(login,64));assert p.read()==b'\x03'
            position=[0.,0.,0.];yaw=0.;pitch=0.
            for flags in range(512):
                coords=[1.25,-2.5,3.75];turn=f32(12.5);tilt=f32(95. if flags%2==0 else -185.)
                packet=b'\x49'+varint(flags)+struct.pack('>6d2fi',*coords,4.,5.,6.,turn,tilt,flags)
                p.send(packet)
                position=[position[i]+v if flags&(1<<i) else v for i,v in enumerate(coords)]
                yaw=f32(yaw+turn) if flags&8 else turn
                pitch=max(-90.,min(90.,f32((pitch if flags&16 else 0.)+tilt)))
                expected=b'\0'+varint(flags)+struct.pack('>3d2f',*position,yaw,pitch)
                assert p.read()==expected,flags
            if limit==65536:
                custom=b'\x2e'+struct.pack('>ii',0,0)+b'\0'+string(bytes.fromhex('0001000000040007')*2)+b'\0'+bytes(6)
                p.send(custom);result=p.finish()
                assert 'chunk:0,0 bytes:16 entities:0 player:42 teleports:512 min_y:-32 sections:2 first:4' in result['detail'],result
            else:
                p.send(chunk);result=p.finish()
                assert 'chunk:0,0 bytes:192 entities:0 player:42 teleports:512 min_y:-64 sections:24 first:0' in result['detail'],result
        count+=1
    for packet,error in [(login[:-1],'needed'),(b'\x49'+bytes(10),'needed'),(b'\x54'+spawn+b'\0','respawn')]:
        with peer(probe,'terrain') as p:
            p.send(finished());assert p.read()==b'\x03';p.read();p.read();p.send(b'\x03');assert p.read()==b'\x03'
            p.send(packet);p.finish(False,error)
        count+=1
    return count


def echo_checks(probe):
    rng = random.Random(0x53545245414d)
    bodies = []
    for i in range(90):
        length = (1, 2, 63, 64, 65, 300, 4000, 70000, 200000)[i % 9]
        bodies.append(bytes([i & 127]) + rng.randbytes(length - 1))
    stream = b''.join(framed(body, 64) for body in bodies)
    secret = bytes([7]) * 16
    encrypted = cipher(stream, secret)
    for limit in (31, 65536):
        env = dict(os.environ, XYLAR_TEST_READ_LIMIT=str(limit))
        p = subprocess.run([str(probe), 'echo', str(len(bodies))], input=encrypted,
                           capture_output=True, timeout=30, env=env)
        assert p.returncode == 0, p.stderr.decode(errors='replace')
        output = cipher(p.stdout, secret, True)
        at = 0
        for body in bodies:
            size, start = number(output, at)
            assert unframe(output[start:start + size], 64) == body
            at = start + size
        assert at == len(output)
        result = json.loads(p.stderr)
        assert result['sent'] == result['received'] == len(bodies)
        assert result['bytes_in'] == len(encrypted) and result['bytes_out'] == len(p.stdout)
    return 2, len(bodies), len(stream)


def tcp_check(network):
    try:
        listener = socket.socket()
    except PermissionError as e:
        return {'status': 'unavailable', 'reason': str(e)}
    with listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(1)
        listener.settimeout(5)
        errors = []
        def serve():
            try:
                conn, _ = listener.accept()
                with conn:
                    conn.settimeout(5)
                    def packet():
                        prefix = bytearray()
                        while not prefix or prefix[-1] & 128:
                            piece = conn.recv(1)
                            assert piece, 'TCP peer closed before frame prefix'
                            prefix.extend(piece)
                        size, _ = number(prefix)
                        data = bytearray()
                        while len(data) < size:
                            piece = conn.recv(size - len(data))
                            if not piece:
                                raise AssertionError('TCP peer closed before packet')
                            data.extend(piece)
                        return data
                    packet()
                    assert packet() == b'\0'
                    conn.sendall(framed(b'\0' + string('{"version":{"protocol":777}}')))
                    assert packet() == b'\x01' + struct.pack('>q', 1)
                    conn.sendall(framed(b'\x01' + struct.pack('>q', 1)))
            except BaseException as e:
                errors.append(repr(e))
        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        try:
            p = subprocess.run([str(network), 'status', '127.0.0.1', str(listener.getsockname()[1]), '1000'],
                               capture_output=True, timeout=10)
            assert p.returncode == 0, p.stdout.decode(errors='replace') + p.stderr.decode(errors='replace')
            assert json.loads(p.stdout)['ok']
        finally:
            thread.join(timeout=6)
        assert not thread.is_alive() and not errors, errors
    return {'status': 'passed'}


def check_session(probe, network, report):
    root = Path(__file__).resolve().parents[2]
    lock = json.loads((root / 'goose/upstream.lock.json').read_text())
    sources = {name: lock['sources'][name] for name in (
        'crates/horizon-protocol/src/connection.rs', 'crates/horizon-protocol/src/crypto.rs',
        'crates/horizon-protocol/src/rsa.rs', 'crates/horizon-client/src/net.rs')}
    # Integration can run without the optional local Rust checkout. The
    # comparison suite separately verifies the pinned source material.
    with tempfile.TemporaryDirectory(prefix='xylar-session-') as tmp:
        checks = login_checks(probe, Path(tmp)) + status_checks(probe) + configuration_checks(probe) + terrain_checks(probe) + player_checks(probe)
        echoes, packets, size = echo_checks(probe)
        checks += echoes
        tcp = tcp_check(network) if network else {'status': 'not-requested'}
    result = {'module': 'net/session-integration', 'minecraft': '26.3',
              'checks_passed': checks, 'reference_revision': lock['revision'],
              'source_provenance': sources,
              'echo_packets_per_read_limit': packets,
              'echo_plain_framed_bytes_per_read_limit': size,
              'stream_backend': 'subprocess pipes; production TCP adapter compiled separately',
              'peer_oracle': 'Python protocol encodings, zlib, hashlib and OpenSSL CLI RSA/CFB8',
              'scope': 'Status, offline login, configuration acknowledgement, player login, all 512 teleport flags/acknowledgements, first column binding with default/custom/replaced dimension layouts and play control replies; optional RSA/AES and compression',
              'tcp': tcp,
              'pending': ['Microsoft session joining', 'velocity/physics and world binding', 'game resources and rendering']}
    report.write_text(json.dumps(result, indent=2) + '\n')
    print(f'PASS: {checks} stream integration checks; {packets} encrypted/compressed echo packets per read limit.')
    print('TCP:', tcp)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--network', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--openssl', default='openssl')
    args = parser.parse_args()
    OPENSSL = args.openssl
    check_session(args.probe.resolve(), args.network.resolve() if args.network else None, args.report.resolve())
