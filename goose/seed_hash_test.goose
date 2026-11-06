// Golden values independently calculated with Python hashlib.sha256.
import core.seed_hash;
import std;
fn main() {
    assert(biome_seed::hash(0) == 8794265229978523055);
    assert(biome_seed::hash(1) == -6467378160175308932);
    assert(biome_seed::hash(-1) == 6759447113877070610);
    assert(biome_seed::hash(42) == -4111196313959201555);
    assert(biome_seed::hash(123456789) == -5458710421630621949);
    assert(biome_seed::hash(-987654321) == 739882241652834894);
    assert(biome_seed::hash(9223372036854775807) == 7179146226492139882);
    assert(biome_seed::hash(-9223372036854775808) == 6374347445474471398);
    print("Biome seed SHA-256 transform: independent golden seeds passed.");
}
