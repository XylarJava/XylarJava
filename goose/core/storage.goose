// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Rust-to-Goose adaptation for Xylar Goose. Full notice: ../licenses/upstream-MIT.txt.
// Reference file/revision: ../upstream.lock.json (storage.rs).
import std;
namespace packed;

let MAGIC_LEN: i64 = 1 << 14;
struct Magic { multiplier: u32, filled: u32 }
fn make_magic() -> Magic[33] {
    var table: Magic[33] = [Magic { multiplier: 0, filled: 0 }; 33];
    for bits in 1..33 {
        let per: u32 = 64 / (bits as u32);
        table[bits] = Magic { multiplier: ((1 << 20) + per - 1) / per,
                             filled: per * (bits as u32) };
    }
    return table;
}
let MAGIC = make_magic();

enum StorageError {
    Ok,
    InvalidBits { bits: u8 },
    InvalidLength { expected: i64, actual: i64 },
    OutOfBounds { index: i64, len: i64 },
    ValueTooLarge { value: u32 },
}
struct PackedStorage {
    bits: u8 = 1,
    len: i64 = 0,
    per_word: i64 = 64,
    mask: u64 = 1,
    wide: bool = false,
    // The Rust source selects this optimization only on x86. Callers on other
    // architectures can select division; this does not change the word format.
    use_magic: bool = true,
    words: u64[>..] = [],
}
fn is_ok(error: StorageError) -> bool {
    match error { Ok => true, _ => false }
}
fn mask(bits: u8) -> u64 { ((1 as u64) << bits) - 1 }
fn is_empty(storage: PackedStorage&) -> bool { storage.len == 0 }
fn word_count(bits: u8, len: i64) -> i64, StorageError {
    assert(len >= 0);
    if bits < 1 || bits > 32 { return 0, StorageError.InvalidBits { bits: bits }; }
    let per = 64 / (bits as i64);
    return len / per + (if len % per != 0 { 1 } else { 0 }), StorageError.Ok;
}
fn new_storage(bits: u8, len: i64, use_magic: bool = true) -> PackedStorage, StorageError {
    let count, error = word_count(bits, len);
    if !is_ok(error) { return PackedStorage {}, error; }
    var storage = PackedStorage {
        bits: bits, len: len, per_word: 64 / (bits as i64),
        mask: mask(bits), wide: len > MAGIC_LEN, use_magic: use_magic,
    };
    for i in count { storage.words.push(0); }
    return storage, StorageError.Ok;
}
fn from_words(bits: u8, len: i64, words: u64[:], use_magic: bool = true) -> PackedStorage, StorageError {
    let expected, error = word_count(bits, len);
    if !is_ok(error) { return PackedStorage {}, error; }
    if words.len != expected {
        return PackedStorage {}, StorageError.InvalidLength { expected: expected, actual: words.len };
    }
    var storage = PackedStorage {
        bits: bits, len: len, per_word: 64 / (bits as i64),
        mask: mask(bits), wide: len > MAGIC_LEN, use_magic: use_magic,
    };
    // Preserve padding and unused tail bits; importing is not normalization.
    for word in words { storage.words.push(word); }
    return storage, StorageError.Ok;
}
fn locate(bits: u8, index: i64) -> i64, i64 {
    assert(bits >= 1 && bits <= 32 && index >= 0 && index < MAGIC_LEN);
    if (bits & (bits - 1)) == 0 {
        var log: i64 = 0;
        var n = bits;
        while n > 1 { n >>= 1; log += 1; }
        return index >> (6 - log), (index & ((64 >> log) - 1)) << log;
    }
    let magic = MAGIC[bits];
    // Keep the reference's u32 wrapping arithmetic, including at large indices.
    let i = index as u32;
    let word = (i * magic.multiplier) >> 20;
    return word, i * bits - word * magic.filled;
}
fn slot(storage: PackedStorage&, index: i64) -> i64, i64 {
    if storage.wide || !storage.use_magic {
        return index / storage.per_word, (index % storage.per_word) * storage.bits;
    }
    return locate(storage.bits, index);
}
fn location(storage: PackedStorage&, index: i64) -> i64, i64, StorageError {
    if index < 0 || index >= storage.len {
        return 0, 0, StorageError.OutOfBounds { index: index, len: storage.len };
    }
    let word, shift = slot(storage, index);
    return word, shift, StorageError.Ok;
}
fn get(storage: PackedStorage&, index: i64) -> u32, StorageError {
    let word, shift, error = location(storage, index);
    if !is_ok(error) { return 0, error; }
    return ((storage.words[word] >> shift) & storage.mask) as u32, StorageError.Ok;
}
fn read(storage: PackedStorage&, index: i64) -> u32 {
    assert(index >= 0 && index < storage.len);
    let word, shift = slot(storage, index);
    return ((storage.words[word] >> shift) & storage.mask) as u32;
}
fn write(storage: PackedStorage&, index: i64, value: u32) -> u32 {
    assert(index >= 0 && index < storage.len && value <= storage.mask);
    let word, shift = slot(storage, index);
    let old = ((storage.words[word] >> shift) & storage.mask) as u32;
    storage.words[word] = (storage.words[word] & ~(storage.mask << shift)) | ((value as u64) << shift);
    return old;
}
fn set(storage: PackedStorage&, index: i64, value: u32) -> u32, StorageError {
    let word, shift, error = location(storage, index);
    if !is_ok(error) { return 0, error; }
    if value > storage.mask || value > 2147483647 {
        return 0, StorageError.ValueTooLarge { value: value };
    }
    let old = ((storage.words[word] >> shift) & storage.mask) as u32;
    storage.words[word] = (storage.words[word] & ~(storage.mask << shift)) | ((value as u64) << shift);
    return old, StorageError.Ok;
}
fn fill_storage(storage: PackedStorage&, value: u32) -> StorageError {
    if value > storage.mask || value > 2147483647 {
        return StorageError.ValueTooLarge { value: value };
    }
    var word = value as u64;
    var shift: i64 = storage.bits;
    // As in the reference: doubling also fills padding and unused tail slots.
    while shift < 64 { word |= word << shift; shift *= 2; }
    for i in storage.words.len { storage.words[i] = word; }
    return StorageError.Ok;
}
fn unpack_into(storage: PackedStorage&, output: u32[:]) {
    assert(output.len >= storage.len);
    var i: i64 = 0;
    for word in storage.words {
        for entry in min(storage.per_word, storage.len - i) {
            output[i] = ((word >> (entry * storage.bits)) & storage.mask) as u32;
            i += 1;
        }
    }
}
fn unpack(storage: PackedStorage&) -> u32[>..] {
    var values: u32[>..] = [];
    for word in storage.words {
        for entry in min(storage.per_word, storage.len - values.len) {
            values.push(((word >> (entry * storage.bits)) & storage.mask) as u32);
        }
    }
    return values;
}
// Goose owns inline data stacks, not Vec heap capacity: this metric counts
// logical word bytes and is deliberately not named heap_bytes.
fn word_bytes(storage: PackedStorage&) -> i64 { storage.words.len * 8 }
