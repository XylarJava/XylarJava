// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Binary NBT port of horizon-world/nbt.rs; see ../upstream.lock.json.
// Full notice: ../licenses/upstream-MIT.txt. SNBT and streaming visitors pending.
// Trees use an owning arena and child indices instead of recursive Rust enums.
// Strings/compound keys retain exact UTF-16 units, including lone surrogates.
import std;
namespace nbt;
struct Limits { max_bytes: i64 = 67108864, max_nodes: i64 = 1000000, max_depth: i64 = 512 }
struct Error { offset: i64 = 0, reason: const u8[:] = "" }
fn is_ok(e: Error&) -> bool { e.reason.len == 0 }
fn error_message(e: Error&) -> u8[>..] {
    var out: u8[>..] = []; format(out, "NBT at byte ", e.offset, ": ", e.reason); return out;
}
// Each node occupies eight words, followed by its variable data in the same
// owner. Links/offsets stay valid when the backing buffer grows. Zero is null.
struct Node { kind: u8 = 0, bits: u64 = 0, name: i64 = 0, name_len: i64 = 0,
    data: i64 = 0, len: i64 = 0, first: i64 = 0, next: i64 = 0 }
struct Document { root: i64 = 0, name: i64 = 0, name_len: i64 = 0, words: u64[>..] = [0] }
fn node(d: Document&, at: i64) -> Node {
    return Node { kind: d.words[at] as u8, bits: d.words[at + 1], name: d.words[at + 2] as i64,
        name_len: d.words[at + 3] as i64, data: d.words[at + 4] as i64, len: d.words[at + 5] as i64,
        first: d.words[at + 6] as i64, next: d.words[at + 7] as i64 };
}
struct Reader { bytes: const u8[:], pos: i64 = 0, limits: Limits = Limits {}, nodes: i64 = 0 }
fn failure(r: Reader&, reason: const u8[:]) -> Error { Error { offset: r.pos, reason: reason } }
fn take(r: Reader&, n: i64) -> const u8[:], Error {
    if n > r.limits.max_bytes - r.pos { return "", failure(r, "byte limit exceeded"); }
    if n > r.bytes.len - r.pos { return "", failure(r, "truncated input"); }
    let start = r.pos; r.pos += n; return r.bytes[start..r.pos], Error {};
}
fn number(r: Reader&, n: i64) -> u64, Error {
    let bytes, e = take(r, n); if !is_ok(e) { return 0, copy(e); }
    var value: u64 = 0; for b in bytes { value = (value << 8) | b; } return value, Error {};
}
fn count(r: Reader&, width: i64) -> i64, Error {
    let raw, e = number(r, 4); if !is_ok(e) { return 0, copy(e); }
    let n = raw as! i32;
    if n < 0 { return 0, failure(r, "negative collection length"); }
    let size = (n as i64) * width;
    if size > r.bytes.len - r.pos || size > r.limits.max_bytes - r.pos {
        return 0, failure(r, "collection exceeds available bytes");
    }
    return n, Error {};
}
fn string(r: Reader&, d: Document&, root_name: bool = false) -> i64, i64, Error {
    let size, e = number(r, 2); if !is_ok(e) { return 0, 0, copy(e); }
    let bytes, e2 = take(r, size as i64); if !is_ok(e2) { return 0, 0, copy(e2); }
    let at = d.words.len; var i: i64 = 0;
    while i < bytes.len {
        let b = bytes[i];
        if b < 128 { d.words.push(b); i += 1; }
        else if b >= 192 && b <= 223 && i + 1 < bytes.len && (bytes[i + 1] & 192) == 128 {
            d.words.push((((b as u64) & 31) << 6) | (bytes[i + 1] & 63)); i += 2;
        } else if b >= 224 && b <= 239 && i + 2 < bytes.len && (bytes[i + 1] & 192) == 128 && (bytes[i + 2] & 192) == 128 {
            d.words.push((((b as u64) & 15) << 12) | (((bytes[i + 1] as u64) & 63) << 6) | (bytes[i + 2] & 63)); i += 3;
        } else {
            if root_name { return 0, 0, Error {}; }
            return 0, 0, failure(r, "invalid Java modified UTF-8");
        }
    }
    return at, d.words.len - at, Error {};
}
fn compare(d: Document&, a: i64, an: i64, b: i64, bn: i64) -> i64 {
    for i in min(an, bn) { if d.words[a + i] < d.words[b + i] { return -1; } if d.words[a + i] > d.words[b + i] { return 1; } }
    return if an < bn { -1 } else if an > bn { 1 } else { 0 };
}
recursive fn payload(r: Reader&, d: Document&, id: u8, depth: i64) -> i64, Error {
    if r.nodes >= r.limits.max_nodes { return 0, failure(r, "node limit exceeded"); }
    r.nodes += 1;
    if (id == 9 || id == 10) && depth >= r.limits.max_depth { return 0, failure(r, "depth limit exceeded"); }
    let at = d.words.len; for i in 8 { d.words.push(0); } d.words[at] = id;
    if id == 0 { return at, Error {}; }
    if id >= 1 && id <= 6 {
        let width = if id == 1 { 1 } else if id == 2 { 2 } else if id == 4 || id == 6 { 8 } else { 4 };
        let bits, e = number(r, width); if !is_ok(e) { return 0, copy(e); }
        d.words[at + 1] = if (id == 5 && (bits & 0x7fffffff) == 0) || (id == 6 && (bits & 0x7fffffffffffffff) == 0) { 0 } else { bits };
    } else if id == 7 {
        let n, e = count(r, 1); if !is_ok(e) { return 0, copy(e); }
        let bytes, e2 = take(r, n); if !is_ok(e2) { return 0, copy(e2); }
        d.words[at + 4] = d.words.len as u64; d.words[at + 5] = n as u64;
        for b in bytes { d.words.push(b); }
    } else if id == 8 {
        let text, len, e = string(r, d); if !is_ok(e) { return 0, copy(e); }
        d.words[at + 4] = text as u64; d.words[at + 5] = len as u64;
    } else if id == 9 {
        let raw, e = number(r, 1); if !is_ok(e) { return 0, copy(e); }
        let element_id = raw as u8;
        let n, e2 = count(r, 0); if !is_ok(e2) { return 0, copy(e2); }
        if n > 0 && (element_id == 0 || element_id > 12) { return 0, failure(r, "invalid list element type"); }
        if n > r.limits.max_nodes - r.nodes { return 0, failure(r, "node limit exceeded"); }
        if n > r.bytes.len - r.pos { return 0, failure(r, "truncated list"); }
        var last: i64 = 0;
        for i in n {
            let child, error = payload(r, d, element_id, depth + 1); if !is_ok(error) { return 0, copy(error); }
            var value = child; let t = node(d, value);
            if t.kind == 10 && t.len == 1 { let inner = node(d, t.first); if inner.name_len == 0 { value = t.first; } }
            if last == 0 { d.words[at + 6] = value as u64; } else { d.words[last + 7] = value as u64; }
            last = value;
        }
        d.words[at + 5] = n as u64;
    } else if id == 10 {
        loop {
            let raw, e = number(r, 1); if !is_ok(e) { return 0, copy(e); } if raw == 0 { break; }
            let name, len, e2 = string(r, d); if !is_ok(e2) { return 0, copy(e2); }
            let child, e3 = payload(r, d, raw as u8, depth + 1); if !is_ok(e3) { return 0, copy(e3); }
            d.words[child + 2] = name as u64; d.words[child + 3] = len as u64;
            // Sorted UTF-16 map; insertion preserves the last duplicate value.
            var prev: i64 = 0; var current = d.words[at + 6] as i64; var duplicate = false;
            while current != 0 {
                let key = node(d, current); let order = compare(d, key.name, key.name_len, name, len);
                if order >= 0 { duplicate = order == 0; break; }
                prev = current; current = key.next;
            }
            d.words[child + 7] = if duplicate { d.words[current + 7] } else { current as u64 };
            if prev == 0 { d.words[at + 6] = child as u64; } else { d.words[prev + 7] = child as u64; }
            if !duplicate { d.words[at + 5] += 1; }
        }
    } else if id == 11 || id == 12 {
        let width = if id == 11 { 4 } else { 8 };
        let n, e = count(r, width); if !is_ok(e) { return 0, copy(e); }
        d.words[at + 4] = d.words.len as u64; d.words[at + 5] = n as u64;
        for i in n { let v, error = number(r, width); if !is_ok(error) { return 0, copy(error); } d.words.push(v); }
    } else { return 0, failure(r, "unknown tag type"); }
    return at, Error {};
}
fn read(bytes: const u8[:], limits: Limits = Limits {}, named: bool = false) -> Document, i64, Error {
    assert(limits.max_bytes >= 0 && limits.max_nodes >= 0 && limits.max_depth >= 0);
    var r = Reader { bytes: bytes, limits: limits }; var d = Document {};
    let id, e = number(r, 1); if !is_ok(e) { return d, r.pos, copy(e); }
    if named && id != 0 { let name, len, error = string(r, d, true); if !is_ok(error) { return d, r.pos, copy(error); } d.name = name; d.name_len = len; }
    let root, error = payload(r, d, id as u8, 0); d.root = root;
    return d, r.pos, copy(error);
}
struct Writer { limits: Limits = Limits {}, nodes: i64 = 0, named: bool = false, output: u8[>..] = [] }
fn failure(w: Writer&, reason: const u8[:]) -> Error { Error { offset: w.output.len, reason: reason } }
fn put(w: Writer&, bytes: const u8[:]) -> Error {
    if bytes.len > w.limits.max_bytes - w.output.len { return failure(w, "byte limit exceeded"); }
    w.output.append(bytes); return Error {};
}
fn put_num(w: Writer&, value: u64, width: i64) -> Error {
    if width > w.limits.max_bytes - w.output.len { return failure(w, "byte limit exceeded"); }
    for i in width { w.output.push((value >> (8 * (width - i - 1))) as! u8); } return Error {};
}
fn put_count(w: Writer&, n: i64) -> Error {
    if n > 2147483647 { return failure(w, "collection too long"); }
    return put_num(w, n as u64, 4);
}
fn put_string(w: Writer&, d: Document&, at: i64, len: i64) -> Error {
    var size: i64 = 0;
    for i in len { let c = d.words[at + i]; size += if c >= 1 && c <= 127 { 1 } else if c <= 2047 { 2 } else { 3 }; }
    if size > 65535 {
        if w.named { return put_num(w, 0, 2); }
        return failure(w, "modified UTF-8 exceeds 65535 bytes");
    }
    let e = put_num(w, size as u64, 2); if !is_ok(e) { return copy(e); }
    for i in len {
        let c = d.words[at + i];
        var bytes: u8[3] = [0; 3]; var n: i64 = 0;
        if c >= 1 && c <= 127 { bytes[0] = c as! u8; n = 1; }
        else if c <= 2047 { bytes[0] = (192 | (c >> 6)) as! u8; bytes[1] = (128 | (c & 63)) as! u8; n = 2; }
        else { bytes[0] = (224 | (c >> 12)) as! u8; bytes[1] = (128 | ((c >> 6) & 63)) as! u8; bytes[2] = (128 | (c & 63)) as! u8; n = 3; }
        let error = put(w, bytes[..n]); if !is_ok(error) { return copy(error); }
    }
    return Error {};
}
recursive fn write_payload(w: Writer&, d: Document&, index: i64, depth: i64) -> Error {
    if w.nodes >= w.limits.max_nodes { return failure(w, "node limit exceeded"); }
    w.nodes += 1;
    let t = node(d, index); let id = t.kind;
    if (id == 9 || id == 10) && depth >= w.limits.max_depth { return failure(w, "depth limit exceeded"); }
    if id == 0 { return Error {}; }
    if id >= 1 && id <= 6 {
        let width = if id == 1 { 1 } else if id == 2 { 2 } else if id == 4 || id == 6 { 8 } else { 4 };
        var bits = t.bits;
        if w.named && id == 5 && (bits & 0x7fffffff) > 0x7f800000 { bits = 0x7fc00000; }
        if w.named && id == 6 && (bits & 0x7fffffffffffffff) > 0x7ff0000000000000 { bits = 0x7ff8000000000000; }
        return put_num(w, bits, width);
    }
    if id == 7 || id == 11 || id == 12 {
        let e = put_count(w, t.len); if !is_ok(e) { return copy(e); }
        let width = if id == 7 { 1 } else if id == 11 { 4 } else { 8 };
        if id == 7 && t.len > w.limits.max_bytes - w.output.len { return failure(w, "byte limit exceeded"); }
        for i in t.len { let error = put_num(w, d.words[t.data + i], width); if !is_ok(error) { return copy(error); } }
        return Error {};
    }
    if id == 8 { return put_string(w, d, t.data, t.len); }
    if id == 9 {
        let first_id: u8 = if t.first == 0 { 0 } else { d.words[t.first] as u8 };
        var element_id = first_id; var child = t.first;
        while child != 0 {
            let value = node(d, child);
            if value.kind == 0 { return failure(w, "End cannot be a list element"); }
            if value.kind != first_id { element_id = 10; }
            child = value.next;
        }
        let e = put_num(w, element_id, 1); if !is_ok(e) { return copy(e); }
        let e2 = put_count(w, t.len); if !is_ok(e2) { return copy(e2); }
        child = t.first;
        while child != 0 {
            let value = node(d, child);
            let wrapper = value.kind == 10 && value.len == 1 && d.words[value.first + 3] == 0;
            if element_id == 10 && (value.kind != 10 || wrapper) {
                if depth + 1 >= w.limits.max_depth { return failure(w, "depth limit exceeded"); }
                if w.nodes >= w.limits.max_nodes { return failure(w, "node limit exceeded"); }
                w.nodes += 1;
                let a = put_num(w, value.kind, 1); if !is_ok(a) { return copy(a); }
                let b = put_num(w, 0, 2); if !is_ok(b) { return copy(b); }
                let c = write_payload(w, d, child, depth + 2); if !is_ok(c) { return copy(c); }
                let e3 = put_num(w, 0, 1); if !is_ok(e3) { return copy(e3); }
            } else { let e3 = write_payload(w, d, child, depth + 1); if !is_ok(e3) { return copy(e3); } }
            child = value.next;
        }
    } else if id == 10 {
        var child = t.first;
        while child != 0 {
            let value = node(d, child);
            if value.kind == 0 { return failure(w, "End cannot be a named value"); }
            let a = put_num(w, value.kind, 1); if !is_ok(a) { return copy(a); }
            let b = put_string(w, d, value.name, value.name_len); if !is_ok(b) { return copy(b); }
            let c = write_payload(w, d, child, depth + 1); if !is_ok(c) { return copy(c); }
            child = value.next;
        }
        return put_num(w, 0, 1);
    }
    return Error {};
}
fn write(d: Document&, limits: Limits = Limits {}, named: bool = false) -> u8[>..], Error {
    var w = Writer { limits: limits, named: named };
    let e = put_num(w, d.words[d.root] as u8, 1); if !is_ok(e) { return [], copy(e); }
    if named && d.words[d.root] as u8 != 0 { let e2 = put_string(w, d, d.name, d.name_len); if !is_ok(e2) { return [], copy(e2); } }
    let error = write_payload(w, d, d.root, 0); if !is_ok(error) { return [], copy(error); }
    return copy(w.output), Error {};
}
