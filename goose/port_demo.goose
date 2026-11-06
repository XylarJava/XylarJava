// Manual exerciser for the translated modules, separate from the voxel sandbox.
import std;
import os;
import core.storage;
import core.text;
import core.seed_hash;

fn usage() {
    print("Xylar Goose — prova dei moduli tradotti");
    print("Questo programma prova il port delle librerie; non avvia un mondo di gioco.");
    print("Uso:");
    print("  xylar-goose-port integer TESTO     converte un intero Java, anche Unicode");
    print("  xylar-goose-port seed NUMERO       calcola il seed offuscato dei biomi");
    print("  xylar-goose-port storage BIT VALORI...  impacchetta una palette e la rilegge");
    print("Esempi: integer ١٢٣ | seed 42 | storage 5 1 31 7");
}
fn main() {
    let arguments = args();
    if arguments.len == 1 || (arguments.len == 2 && arguments[1] == "--help") { usage(); return; }
    let command = arguments[1];
    if command == "integer" && arguments.len == 3 {
        let value, ok = java_text::parse_java_integer(arguments[2]);
        if !ok { print("Rifiutato: il testo non e' un intero Java a 32 bit valido."); exit(2); }
        print("Valore: ", value);
    } else if command == "seed" && arguments.len == 3 {
        let value, ok = parse_int(arguments[2]);
        if !ok { print("Seed non valido: serve un intero con segno a 64 bit."); exit(2); }
        print("Seed dei biomi: ", biome_seed::hash(value));
    } else if command == "storage" && arguments.len >= 4 {
        let width, valid_width = parse_int(arguments[2]);
        if !valid_width || width < 1 || width > 32 { print("BIT deve essere tra 1 e 32."); exit(2); }
        var storage, error = packed::new_storage(width as u8, arguments.len - 3);
        assert(packed::is_ok(error));
        for i in 3..arguments.len {
            let value, valid_value = parse_int(arguments[i]);
            if !valid_value || value < 0 || value > 2147483647 {
                print("Valore non valido alla posizione ", i - 3, ": serve un intero tra 0 e 2147483647."); exit(2);
            }
            let previous, result = packed::set(storage, i - 3, value as u32);
            if !packed::is_ok(result) {
                print("Il valore ", value, " non entra in ", width, " bit."); exit(2);
            }
        }
        print("Elementi: ", storage.len, "; parole da 64 bit: ", storage.words.len);
        var words: u8[>..] = [];
        format(words, "Parole impacchettate:");
        for word in storage.words { format(words, " ", word); }
        print(words);
        var decoded: u8[>..] = [];
        format(decoded, "Valori riletti:");
        for value in packed::unpack(storage) { format(decoded, " ", value); }
        print(decoded);
    } else { usage(); exit(2); }
}
