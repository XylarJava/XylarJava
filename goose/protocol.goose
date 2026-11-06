// Minecraft Java wire primitives. Independently implemented; no game assets.
// Keep these independent of packet IDs, which differ between game versions.

fn write_varint(output: u8[>..]&, value: i32) {
    var bits = value as! u32;
    loop {
        let byte = (bits & 127) as u8;
        bits >>= 7;
        output.push(if bits == 0 { byte } else { byte | 128 });
        if bits == 0 { return; }
    }
}

// Returns value, bytes consumed, success. On failure consume nothing: callers
// can retain an incomplete TCP frame. Signed VarInts are NOT ZigZag encoded.
fn read_varint(input: u8[:]) -> i32, i64, bool {
    var bits: u32 = 0;
    for group in 5 {
        if group >= input.len { return 0, 0, false; }
        let byte = input[group];
        bits |= ((byte & 127) as u32) << (7 * group);
        if (byte & 128) == 0 {
            return bits as! i32, group + 1, true;
        }
    }
    return 0, 0, false;
}
