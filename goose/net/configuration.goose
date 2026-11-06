// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ludovic Marie and the Horizon contributors
// Protocol 777 configuration.rs port. Full notice: ../licenses/upstream-MIT.txt.
// NBT values are validated canonical network bytes, rather than Rust Tag trees.
import net.buf;
namespace configuration263;
struct OptionalUuid { present: bool = false, value: wire::Uuid = wire::Uuid {} }
struct OptionalData { present: bool = false, data: u8[] = [] }
struct Text { value: u8[] = [] }
struct LinkKind { known: bool = true, id: i32 = 0, data: u8[] = [] }
fn read_optional_uuid(r: wire::Reader&) -> OptionalUuid, wire::Error {
    let present, e = wire::read_bool(r); if !wire::is_ok(e) { return OptionalUuid {}, e; }
    if !present { return OptionalUuid {}, wire::Error.Ok; }
    let value, e2 = wire::read_uuid(r); if !wire::is_ok(e2) { return OptionalUuid {}, e2; }
    return OptionalUuid { present: true, value: value }, wire::Error.Ok;
}
fn write_optional_uuid(v: OptionalUuid&, out: u8[>..]&) { wire::write_bool(out, v.present); if v.present { wire::write_uuid(out, v.value); } }
fn read_optional_data(r: wire::Reader&, nbt_value: bool) -> OptionalData, wire::Error {
    let present, e = wire::read_bool(r); if !wire::is_ok(e) { return OptionalData {}, e; }
    if !present { return OptionalData {}, wire::Error.Ok; }
    if nbt_value {
        let data, error = wire::read_nbt(r); if !wire::is_ok(error) { return OptionalData {}, error; }
        return OptionalData { present: true, data: data[..] }, wire::Error.Ok;
    }
    let data, error = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(error) { return OptionalData {}, error; }
    return OptionalData { present: true, data: data }, wire::Error.Ok;
}
fn read_optional_nbt(r: wire::Reader&) -> OptionalData, wire::Error { return read_optional_data(r, true); }
fn read_optional_bytes(r: wire::Reader&) -> OptionalData, wire::Error { return read_optional_data(r, false); }
fn write_optional_nbt(v: OptionalData&, out: u8[>..]&) { wire::write_bool(out, v.present); if v.present { wire::write_nbt(out, v.data); } }
fn write_optional_bytes(v: OptionalData&, out: u8[>..]&) { wire::write_bool(out, v.present); if v.present { wire::write_byte_array(out, v.data); } }
fn read_link_kind(r: wire::Reader&) -> LinkKind, wire::Error {
    let known, e = wire::read_bool(r); if !wire::is_ok(e) { return LinkKind {}, e; }
    if known {
        let id, error = wire::read_varint(r); if !wire::is_ok(error) { return LinkKind {}, error; }
        return LinkKind { known: true, id: id }, wire::Error.Ok;
    }
    let data, error = wire::read_nbt(r); if !wire::is_ok(error) { return LinkKind {}, error; }
    return LinkKind { known: false, data: data[..] }, wire::Error.Ok;
}
fn write_link_kind(v: LinkKind&, out: u8[>..]&) { wire::write_bool(out, v.known); if v.known { wire::write_varint(out, v.id); } else { wire::write_nbt(out, v.data); } }
fn read_click(r: wire::Reader&) -> OptionalData, wire::Error {
    let size, e = wire::read_count(r, 65536); if !wire::is_ok(e) { return OptionalData {}, e; }
    let bytes, e2 = wire::read_bytes(r, size); if !wire::is_ok(e2) { return OptionalData {}, e2; }
    var inner = wire::reader(bytes);
    let data, error = wire::read_nbt(inner); if !wire::is_ok(error) { return OptionalData {}, error; }
    return OptionalData { present: data[0] != 0, data: data[..] }, wire::Error.Ok;
}
fn write_click(v: OptionalData&, out: u8[>..]&) {
    wire::write_varint(out, if v.present { v.data.len as! i32 } else { 1 });
    if v.present { wire::write_nbt(out, v.data); } else { wire::write_u8(out, 0); }
}
struct CookieRequest { key: u8[] = [] }
fn read_CookieRequest(r: wire::Reader&) -> CookieRequest, wire::Error {
    let key, e_key = wire::read_string(r, 32767); if !wire::is_ok(e_key) { return CookieRequest {}, e_key; }
    return CookieRequest { key: key }, wire::Error.Ok;
}
fn write_CookieRequest(v: CookieRequest&, out: u8[>..]&) {
    wire::write_string(out, v.key);
}
struct CustomPayload { channel: u8[] = [], data: u8[] = [] }
fn read_CustomPayload(r: wire::Reader&) -> CustomPayload, wire::Error {
    let channel, e_channel = wire::read_string(r, 32767); if !wire::is_ok(e_channel) { return CustomPayload {}, e_channel; }
    let data = wire::rest(r);
    return CustomPayload { channel: channel, data: data }, wire::Error.Ok;
}
fn write_CustomPayload(v: CustomPayload&, out: u8[>..]&) {
    wire::write_string(out, v.channel);
    out.append(v.data);
}
struct Disconnect { reason: u8[] = [] }
fn read_Disconnect(r: wire::Reader&) -> Disconnect, wire::Error {
    let reason, e_reason = wire::read_nbt(r); if !wire::is_ok(e_reason) { return Disconnect {}, e_reason; }
    return Disconnect { reason: reason[..] }, wire::Error.Ok;
}
fn write_Disconnect(v: Disconnect&, out: u8[>..]&) {
    wire::write_nbt(out, v.reason);
}
struct KeepAlive { id: i64 = 0 }
fn read_KeepAlive(r: wire::Reader&) -> KeepAlive, wire::Error {
    let id, e_id = wire::read_i64(r); if !wire::is_ok(e_id) { return KeepAlive {}, e_id; }
    return KeepAlive { id: id }, wire::Error.Ok;
}
fn write_KeepAlive(v: KeepAlive&, out: u8[>..]&) {
    wire::write_i64(out, v.id);
}
struct Ping { id: i32 = 0 }
fn read_Ping(r: wire::Reader&) -> Ping, wire::Error {
    let id, e_id = wire::read_i32(r); if !wire::is_ok(e_id) { return Ping {}, e_id; }
    return Ping { id: id }, wire::Error.Ok;
}
fn write_Ping(v: Ping&, out: u8[>..]&) {
    wire::write_i32(out, v.id);
}
struct RegistryEntry { id: u8[] = [], data: OptionalData = OptionalData {} }
fn read_RegistryEntry(r: wire::Reader&) -> RegistryEntry, wire::Error {
    let id, e_id = wire::read_string(r, 32767); if !wire::is_ok(e_id) { return RegistryEntry {}, e_id; }
    let data, e_data = read_optional_nbt(r); if !wire::is_ok(e_data) { return RegistryEntry {}, e_data; }
    return RegistryEntry { id: id, data: data }, wire::Error.Ok;
}
fn write_RegistryEntry(v: RegistryEntry&, out: u8[>..]&) {
    wire::write_string(out, v.id);
    write_optional_nbt(v.data, out);
}
struct RegistryData { registry: u8[] = [], entries: RegistryEntry[] = [] }
fn read_RegistryData(r: wire::Reader&) -> RegistryData, wire::Error {
    let registry, e_registry = wire::read_string(r, 32767); if !wire::is_ok(e_registry) { return RegistryData {}, e_registry; }
    let entries, e_entries = read_list_RegistryEntry(r); if !wire::is_ok(e_entries) { return RegistryData {}, e_entries; }
    return RegistryData { registry: registry, entries: entries[..] }, wire::Error.Ok;
}
fn write_RegistryData(v: RegistryData&, out: u8[>..]&) {
    wire::write_string(out, v.registry);
    write_list_RegistryEntry(v.entries, out);
}
struct ResourcePackPop { id: OptionalUuid = OptionalUuid {} }
fn read_ResourcePackPop(r: wire::Reader&) -> ResourcePackPop, wire::Error {
    let id, e_id = read_optional_uuid(r); if !wire::is_ok(e_id) { return ResourcePackPop {}, e_id; }
    return ResourcePackPop { id: id }, wire::Error.Ok;
}
fn write_ResourcePackPop(v: ResourcePackPop&, out: u8[>..]&) {
    write_optional_uuid(v.id, out);
}
struct ResourcePackPush { id: wire::Uuid = wire::Uuid {}, url: u8[] = [], hash: u8[] = [], required: bool = false, prompt: OptionalData = OptionalData {} }
fn read_ResourcePackPush(r: wire::Reader&) -> ResourcePackPush, wire::Error {
    let id, e_id = wire::read_uuid(r); if !wire::is_ok(e_id) { return ResourcePackPush {}, e_id; }
    let url, e_url = wire::read_string(r, 32767); if !wire::is_ok(e_url) { return ResourcePackPush {}, e_url; }
    let hash, e_hash = wire::read_string(r, 40); if !wire::is_ok(e_hash) { return ResourcePackPush {}, e_hash; }
    let required, e_required = wire::read_bool(r); if !wire::is_ok(e_required) { return ResourcePackPush {}, e_required; }
    let prompt, e_prompt = read_optional_nbt(r); if !wire::is_ok(e_prompt) { return ResourcePackPush {}, e_prompt; }
    return ResourcePackPush { id: id, url: url, hash: hash, required: required, prompt: prompt }, wire::Error.Ok;
}
fn write_ResourcePackPush(v: ResourcePackPush&, out: u8[>..]&) {
    wire::write_uuid(out, v.id);
    wire::write_string(out, v.url);
    wire::write_string(out, v.hash);
    wire::write_bool(out, v.required);
    write_optional_nbt(v.prompt, out);
}
struct StoreCookie { key: u8[] = [], payload: u8[] = [] }
fn read_StoreCookie(r: wire::Reader&) -> StoreCookie, wire::Error {
    let key, e_key = wire::read_string(r, 32767); if !wire::is_ok(e_key) { return StoreCookie {}, e_key; }
    let payload, e_payload = wire::read_byte_array(r, wire::remaining(r)); if !wire::is_ok(e_payload) { return StoreCookie {}, e_payload; }
    return StoreCookie { key: key, payload: payload }, wire::Error.Ok;
}
fn write_StoreCookie(v: StoreCookie&, out: u8[>..]&) {
    wire::write_string(out, v.key);
    wire::write_byte_array(out, v.payload);
}
struct Transfer { host: u8[] = [], port: i32 = 0 }
fn read_Transfer(r: wire::Reader&) -> Transfer, wire::Error {
    let host, e_host = wire::read_string(r, 32767); if !wire::is_ok(e_host) { return Transfer {}, e_host; }
    let port, e_port = wire::read_varint(r); if !wire::is_ok(e_port) { return Transfer {}, e_port; }
    return Transfer { host: host, port: port }, wire::Error.Ok;
}
fn write_Transfer(v: Transfer&, out: u8[>..]&) {
    wire::write_string(out, v.host);
    wire::write_varint(out, v.port);
}
fn read_Text(r: wire::Reader&) -> Text, wire::Error {
    let value, e_value = wire::read_string(r, 32767); if !wire::is_ok(e_value) { return Text {}, e_value; }
    return Text { value: value }, wire::Error.Ok;
}
fn write_Text(v: Text&, out: u8[>..]&) {
    wire::write_string(out, v.value);
}
struct TagEntry { name: u8[] = [], ids: i32[] = [] }
fn read_TagEntry(r: wire::Reader&) -> TagEntry, wire::Error {
    let name, e_name = wire::read_string(r, 32767); if !wire::is_ok(e_name) { return TagEntry {}, e_name; }
    let ids, e_ids = read_list_varint(r); if !wire::is_ok(e_ids) { return TagEntry {}, e_ids; }
    return TagEntry { name: name, ids: ids[..] }, wire::Error.Ok;
}
fn write_TagEntry(v: TagEntry&, out: u8[>..]&) {
    wire::write_string(out, v.name);
    write_list_varint(v.ids, out);
}
struct RegistryTags { registry: u8[] = [], tags: TagEntry[] = [] }
fn read_RegistryTags(r: wire::Reader&) -> RegistryTags, wire::Error {
    let registry, e_registry = wire::read_string(r, 32767); if !wire::is_ok(e_registry) { return RegistryTags {}, e_registry; }
    let tags, e_tags = read_list_TagEntry(r); if !wire::is_ok(e_tags) { return RegistryTags {}, e_tags; }
    return RegistryTags { registry: registry, tags: tags[..] }, wire::Error.Ok;
}
fn write_RegistryTags(v: RegistryTags&, out: u8[>..]&) {
    wire::write_string(out, v.registry);
    write_list_TagEntry(v.tags, out);
}
struct UpdateTags { registries: RegistryTags[] = [] }
fn read_UpdateTags(r: wire::Reader&) -> UpdateTags, wire::Error {
    let registries, e_registries = read_list_RegistryTags(r); if !wire::is_ok(e_registries) { return UpdateTags {}, e_registries; }
    return UpdateTags { registries: registries[..] }, wire::Error.Ok;
}
fn write_UpdateTags(v: UpdateTags&, out: u8[>..]&) {
    write_list_RegistryTags(v.registries, out);
}
struct UpdateEnabledFeatures { features: Text[] = [] }
fn read_UpdateEnabledFeatures(r: wire::Reader&) -> UpdateEnabledFeatures, wire::Error {
    let features, e_features = read_list_Text(r); if !wire::is_ok(e_features) { return UpdateEnabledFeatures {}, e_features; }
    return UpdateEnabledFeatures { features: features[..] }, wire::Error.Ok;
}
fn write_UpdateEnabledFeatures(v: UpdateEnabledFeatures&, out: u8[>..]&) {
    write_list_Text(v.features, out);
}
struct KnownPack { pack_namespace: u8[] = [], id: u8[] = [], version: u8[] = [] }
fn read_KnownPack(r: wire::Reader&) -> KnownPack, wire::Error {
    let pack_namespace, e_namespace = wire::read_string(r, 32767); if !wire::is_ok(e_namespace) { return KnownPack {}, e_namespace; }
    let id, e_id = wire::read_string(r, 32767); if !wire::is_ok(e_id) { return KnownPack {}, e_id; }
    let version, e_version = wire::read_string(r, 32767); if !wire::is_ok(e_version) { return KnownPack {}, e_version; }
    return KnownPack { pack_namespace: pack_namespace, id: id, version: version }, wire::Error.Ok;
}
fn write_KnownPack(v: KnownPack&, out: u8[>..]&) {
    wire::write_string(out, v.pack_namespace);
    wire::write_string(out, v.id);
    wire::write_string(out, v.version);
}
struct SelectKnownPacks { packs: KnownPack[] = [] }
fn read_SelectKnownPacks(r: wire::Reader&) -> SelectKnownPacks, wire::Error {
    let packs, e_packs = read_list_KnownPack(r); if !wire::is_ok(e_packs) { return SelectKnownPacks {}, e_packs; }
    return SelectKnownPacks { packs: packs[..] }, wire::Error.Ok;
}
fn write_SelectKnownPacks(v: SelectKnownPacks&, out: u8[>..]&) {
    write_list_KnownPack(v.packs, out);
}
struct CodeOfConduct { text: u8[] = [] }
fn read_CodeOfConduct(r: wire::Reader&) -> CodeOfConduct, wire::Error {
    let text, e_text = wire::read_string(r, 32767); if !wire::is_ok(e_text) { return CodeOfConduct {}, e_text; }
    return CodeOfConduct { text: text }, wire::Error.Ok;
}
fn write_CodeOfConduct(v: CodeOfConduct&, out: u8[>..]&) {
    wire::write_string(out, v.text);
}
struct ServerLink { kind: LinkKind = LinkKind {}, link: u8[] = [] }
fn read_ServerLink(r: wire::Reader&) -> ServerLink, wire::Error {
    let kind, e_kind = read_link_kind(r); if !wire::is_ok(e_kind) { return ServerLink {}, e_kind; }
    let link, e_link = wire::read_string(r, 32767); if !wire::is_ok(e_link) { return ServerLink {}, e_link; }
    return ServerLink { kind: kind, link: link }, wire::Error.Ok;
}
fn write_ServerLink(v: ServerLink&, out: u8[>..]&) {
    write_link_kind(v.kind, out);
    wire::write_string(out, v.link);
}
struct ServerLinks { links: ServerLink[] = [] }
fn read_ServerLinks(r: wire::Reader&) -> ServerLinks, wire::Error {
    let links, e_links = read_list_ServerLink(r); if !wire::is_ok(e_links) { return ServerLinks {}, e_links; }
    return ServerLinks { links: links[..] }, wire::Error.Ok;
}
fn write_ServerLinks(v: ServerLinks&, out: u8[>..]&) {
    write_list_ServerLink(v.links, out);
}
struct ShowDialog { dialog: u8[] = [] }
fn read_ShowDialog(r: wire::Reader&) -> ShowDialog, wire::Error {
    let dialog, e_dialog = wire::read_nbt(r); if !wire::is_ok(e_dialog) { return ShowDialog {}, e_dialog; }
    return ShowDialog { dialog: dialog[..] }, wire::Error.Ok;
}
fn write_ShowDialog(v: ShowDialog&, out: u8[>..]&) {
    wire::write_nbt(out, v.dialog);
}
struct CustomClickAction { id: u8[] = [], payload: OptionalData = OptionalData {} }
fn read_CustomClickAction(r: wire::Reader&) -> CustomClickAction, wire::Error {
    let id, e_id = wire::read_string(r, 32767); if !wire::is_ok(e_id) { return CustomClickAction {}, e_id; }
    let payload, e_payload = read_click(r); if !wire::is_ok(e_payload) { return CustomClickAction {}, e_payload; }
    return CustomClickAction { id: id, payload: payload }, wire::Error.Ok;
}
fn write_CustomClickAction(v: CustomClickAction&, out: u8[>..]&) {
    wire::write_string(out, v.id);
    write_click(v.payload, out);
}
struct ClientInformation { language: u8[] = [], view_distance: i8 = 0, chat_visibility: i32 = 0, chat_colors: bool = false, model_customisation: u8 = 0, main_hand: i32 = 0, text_filtering: bool = false, allows_listing: bool = false, particle_status: i32 = 0 }
fn read_ClientInformation(r: wire::Reader&) -> ClientInformation, wire::Error {
    let language, e_language = wire::read_string(r, 16); if !wire::is_ok(e_language) { return ClientInformation {}, e_language; }
    let view_distance, e_view_distance = wire::read_i8(r); if !wire::is_ok(e_view_distance) { return ClientInformation {}, e_view_distance; }
    let chat_visibility, e_chat_visibility = wire::read_varint(r); if !wire::is_ok(e_chat_visibility) { return ClientInformation {}, e_chat_visibility; }
    let chat_colors, e_chat_colors = wire::read_bool(r); if !wire::is_ok(e_chat_colors) { return ClientInformation {}, e_chat_colors; }
    let model_customisation, e_model_customisation = wire::read_u8(r); if !wire::is_ok(e_model_customisation) { return ClientInformation {}, e_model_customisation; }
    let main_hand, e_main_hand = wire::read_varint(r); if !wire::is_ok(e_main_hand) { return ClientInformation {}, e_main_hand; }
    let text_filtering, e_text_filtering = wire::read_bool(r); if !wire::is_ok(e_text_filtering) { return ClientInformation {}, e_text_filtering; }
    let allows_listing, e_allows_listing = wire::read_bool(r); if !wire::is_ok(e_allows_listing) { return ClientInformation {}, e_allows_listing; }
    let particle_status, e_particle_status = wire::read_varint(r); if !wire::is_ok(e_particle_status) { return ClientInformation {}, e_particle_status; }
    return ClientInformation { language: language, view_distance: view_distance, chat_visibility: chat_visibility, chat_colors: chat_colors, model_customisation: model_customisation, main_hand: main_hand, text_filtering: text_filtering, allows_listing: allows_listing, particle_status: particle_status }, wire::Error.Ok;
}
fn write_ClientInformation(v: ClientInformation&, out: u8[>..]&) {
    wire::write_string(out, v.language);
    wire::write_i8(out, v.view_distance);
    wire::write_varint(out, v.chat_visibility);
    wire::write_bool(out, v.chat_colors);
    wire::write_u8(out, v.model_customisation);
    wire::write_varint(out, v.main_hand);
    wire::write_bool(out, v.text_filtering);
    wire::write_bool(out, v.allows_listing);
    wire::write_varint(out, v.particle_status);
}
struct CookieResponse { key: u8[] = [], payload: OptionalData = OptionalData {} }
fn read_CookieResponse(r: wire::Reader&) -> CookieResponse, wire::Error {
    let key, e_key = wire::read_string(r, 32767); if !wire::is_ok(e_key) { return CookieResponse {}, e_key; }
    let payload, e_payload = read_optional_bytes(r); if !wire::is_ok(e_payload) { return CookieResponse {}, e_payload; }
    return CookieResponse { key: key, payload: payload }, wire::Error.Ok;
}
fn write_CookieResponse(v: CookieResponse&, out: u8[>..]&) {
    wire::write_string(out, v.key);
    write_optional_bytes(v.payload, out);
}
struct ResourcePackResponse { id: wire::Uuid = wire::Uuid {}, action: i32 = 0 }
fn read_ResourcePackResponse(r: wire::Reader&) -> ResourcePackResponse, wire::Error {
    let id, e_id = wire::read_uuid(r); if !wire::is_ok(e_id) { return ResourcePackResponse {}, e_id; }
    let action, e_action = wire::read_varint(r); if !wire::is_ok(e_action) { return ResourcePackResponse {}, e_action; }
    return ResourcePackResponse { id: id, action: action }, wire::Error.Ok;
}
fn write_ResourcePackResponse(v: ResourcePackResponse&, out: u8[>..]&) {
    wire::write_uuid(out, v.id);
    wire::write_varint(out, v.action);
}
fn read_list_KnownPack(r: wire::Reader&) -> KnownPack[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: KnownPack[>..] = [];
    for i in n { let v, error = read_KnownPack(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_KnownPack(values: KnownPack[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { write_KnownPack(value, out); }
}
fn read_list_RegistryEntry(r: wire::Reader&) -> RegistryEntry[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: RegistryEntry[>..] = [];
    for i in n { let v, error = read_RegistryEntry(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_RegistryEntry(values: RegistryEntry[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { write_RegistryEntry(value, out); }
}
fn read_list_RegistryTags(r: wire::Reader&) -> RegistryTags[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: RegistryTags[>..] = [];
    for i in n { let v, error = read_RegistryTags(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_RegistryTags(values: RegistryTags[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { write_RegistryTags(value, out); }
}
fn read_list_ServerLink(r: wire::Reader&) -> ServerLink[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: ServerLink[>..] = [];
    for i in n { let v, error = read_ServerLink(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_ServerLink(values: ServerLink[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { write_ServerLink(value, out); }
}
fn read_list_TagEntry(r: wire::Reader&) -> TagEntry[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: TagEntry[>..] = [];
    for i in n { let v, error = read_TagEntry(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_TagEntry(values: TagEntry[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { write_TagEntry(value, out); }
}
fn read_list_Text(r: wire::Reader&) -> Text[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: Text[>..] = [];
    for i in n { let v, error = read_Text(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_Text(values: Text[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { write_Text(value, out); }
}
fn read_list_varint(r: wire::Reader&) -> i32[>..], wire::Error {
    let n, e = wire::read_count(r, wire::remaining(r)); if !wire::is_ok(e) { return [], e; }
    var out: i32[>..] = [];
    for i in n { let v, error = wire::read_varint(r); if !wire::is_ok(error) { return [], error; } out.push(copy(v)); }
    return out, wire::Error.Ok;
}
fn write_list_varint(values: i32[:], out: u8[>..]&) {
    wire::write_varint(out, values.len as! i32);
    for value in values { wire::write_varint(out, value); }
}
enum Clientbound {
    CookieRequest { value: CookieRequest },
    CustomPayload { value: CustomPayload },
    Disconnect { value: Disconnect },
    FinishConfiguration,
    KeepAlive { value: KeepAlive },
    Ping { value: Ping },
    ResetChat,
    RegistryData { value: RegistryData },
    ResourcePackPop { value: ResourcePackPop },
    ResourcePackPush { value: ResourcePackPush },
    StoreCookie { value: StoreCookie },
    Transfer { value: Transfer },
    UpdateEnabledFeatures { value: UpdateEnabledFeatures },
    UpdateTags { value: UpdateTags },
    SelectKnownPacks { value: SelectKnownPacks },
    ServerLinks { value: ServerLinks },
    ClearDialog,
    ShowDialog { value: ShowDialog },
    CodeOfConduct { value: CodeOfConduct },
    Unknown { id: i32, payload: u8[] },
}
fn read_clientbound(r: wire::Reader&, id: i32) -> Clientbound.., wire::Error {
    if id == 0 { let value, e = read_CookieRequest(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.CookieRequest { value: copy(value) }, wire::Error.Ok; }
    if id == 1 { let value, e = read_CustomPayload(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.CustomPayload { value: copy(value) }, wire::Error.Ok; }
    if id == 2 { let value, e = read_Disconnect(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.Disconnect { value: copy(value) }, wire::Error.Ok; }
    if id == 3 { return Clientbound.FinishConfiguration, wire::Error.Ok; }
    if id == 4 { let value, e = read_KeepAlive(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.KeepAlive { value: copy(value) }, wire::Error.Ok; }
    if id == 5 { let value, e = read_Ping(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.Ping { value: copy(value) }, wire::Error.Ok; }
    if id == 6 { return Clientbound.ResetChat, wire::Error.Ok; }
    if id == 7 { let value, e = read_RegistryData(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.RegistryData { value: copy(value) }, wire::Error.Ok; }
    if id == 8 { let value, e = read_ResourcePackPop(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.ResourcePackPop { value: copy(value) }, wire::Error.Ok; }
    if id == 9 { let value, e = read_ResourcePackPush(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.ResourcePackPush { value: copy(value) }, wire::Error.Ok; }
    if id == 11 { let value, e = read_StoreCookie(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.StoreCookie { value: copy(value) }, wire::Error.Ok; }
    if id == 12 { let value, e = read_Transfer(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.Transfer { value: copy(value) }, wire::Error.Ok; }
    if id == 13 { let value, e = read_UpdateEnabledFeatures(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.UpdateEnabledFeatures { value: copy(value) }, wire::Error.Ok; }
    if id == 14 { let value, e = read_UpdateTags(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.UpdateTags { value: copy(value) }, wire::Error.Ok; }
    if id == 15 { let value, e = read_SelectKnownPacks(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.SelectKnownPacks { value: copy(value) }, wire::Error.Ok; }
    if id == 17 { let value, e = read_ServerLinks(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.ServerLinks { value: copy(value) }, wire::Error.Ok; }
    if id == 18 { return Clientbound.ClearDialog, wire::Error.Ok; }
    if id == 19 { let value, e = read_ShowDialog(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.ShowDialog { value: copy(value) }, wire::Error.Ok; }
    if id == 20 { let value, e = read_CodeOfConduct(r); if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; } return Clientbound.CodeOfConduct { value: copy(value) }, wire::Error.Ok; }
    return Clientbound.Unknown { id: id, payload: wire::rest(r) }, wire::Error.Ok;
}
fn decode_clientbound(id: i32, bytes: const u8[:]) -> Clientbound.., wire::Error {
    var r = wire::reader(bytes); let p, e = read_clientbound(r, id);
    if !wire::is_ok(e) { return Clientbound.Unknown { id: id, payload: [] }, e; }
    if wire::remaining(r) != 0 { return Clientbound.Unknown { id: id, payload: [] }, wire::Error.PacketTooLarge { id: id, serverbound: false, extra: wire::remaining(r) }; }
    return copy(p), wire::Error.Ok;
}
fn encode_clientbound(p: Clientbound..&) -> u8[>..] {
    var out: u8[>..] = [];
    match p {
        CookieRequest v => { wire::write_varint(out, 0); write_CookieRequest(v.value, out); },
        CustomPayload v => { wire::write_varint(out, 1); write_CustomPayload(v.value, out); },
        Disconnect v => { wire::write_varint(out, 2); write_Disconnect(v.value, out); },
        FinishConfiguration => { wire::write_varint(out, 3); },
        KeepAlive v => { wire::write_varint(out, 4); write_KeepAlive(v.value, out); },
        Ping v => { wire::write_varint(out, 5); write_Ping(v.value, out); },
        ResetChat => { wire::write_varint(out, 6); },
        RegistryData v => { wire::write_varint(out, 7); write_RegistryData(v.value, out); },
        ResourcePackPop v => { wire::write_varint(out, 8); write_ResourcePackPop(v.value, out); },
        ResourcePackPush v => { wire::write_varint(out, 9); write_ResourcePackPush(v.value, out); },
        StoreCookie v => { wire::write_varint(out, 11); write_StoreCookie(v.value, out); },
        Transfer v => { wire::write_varint(out, 12); write_Transfer(v.value, out); },
        UpdateEnabledFeatures v => { wire::write_varint(out, 13); write_UpdateEnabledFeatures(v.value, out); },
        UpdateTags v => { wire::write_varint(out, 14); write_UpdateTags(v.value, out); },
        SelectKnownPacks v => { wire::write_varint(out, 15); write_SelectKnownPacks(v.value, out); },
        ServerLinks v => { wire::write_varint(out, 17); write_ServerLinks(v.value, out); },
        ClearDialog => { wire::write_varint(out, 18); },
        ShowDialog v => { wire::write_varint(out, 19); write_ShowDialog(v.value, out); },
        CodeOfConduct v => { wire::write_varint(out, 20); write_CodeOfConduct(v.value, out); },
        Unknown v => { wire::write_varint(out, v.id); out.append(v.payload); },
    }
    return out;
}
enum Serverbound {
    ClientInformation { value: ClientInformation },
    CookieResponse { value: CookieResponse },
    CustomPayload { value: CustomPayload },
    FinishConfiguration,
    KeepAlive { value: KeepAlive },
    Pong { value: Ping },
    ResourcePack { value: ResourcePackResponse },
    SelectKnownPacks { value: SelectKnownPacks },
    CustomClickAction { value: CustomClickAction },
    AcceptCodeOfConduct,
    Unknown { id: i32, payload: u8[] },
}
fn read_serverbound(r: wire::Reader&, id: i32) -> Serverbound.., wire::Error {
    if id == 0 { let value, e = read_ClientInformation(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.ClientInformation { value: copy(value) }, wire::Error.Ok; }
    if id == 1 { let value, e = read_CookieResponse(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.CookieResponse { value: copy(value) }, wire::Error.Ok; }
    if id == 2 { let value, e = read_CustomPayload(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.CustomPayload { value: copy(value) }, wire::Error.Ok; }
    if id == 3 { return Serverbound.FinishConfiguration, wire::Error.Ok; }
    if id == 4 { let value, e = read_KeepAlive(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.KeepAlive { value: copy(value) }, wire::Error.Ok; }
    if id == 5 { let value, e = read_Ping(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.Pong { value: copy(value) }, wire::Error.Ok; }
    if id == 6 { let value, e = read_ResourcePackResponse(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.ResourcePack { value: copy(value) }, wire::Error.Ok; }
    if id == 7 { let value, e = read_SelectKnownPacks(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.SelectKnownPacks { value: copy(value) }, wire::Error.Ok; }
    if id == 8 { let value, e = read_CustomClickAction(r); if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; } return Serverbound.CustomClickAction { value: copy(value) }, wire::Error.Ok; }
    if id == 9 { return Serverbound.AcceptCodeOfConduct, wire::Error.Ok; }
    return Serverbound.Unknown { id: id, payload: wire::rest(r) }, wire::Error.Ok;
}
fn decode_serverbound(id: i32, bytes: const u8[:]) -> Serverbound.., wire::Error {
    var r = wire::reader(bytes); let p, e = read_serverbound(r, id);
    if !wire::is_ok(e) { return Serverbound.Unknown { id: id, payload: [] }, e; }
    if wire::remaining(r) != 0 { return Serverbound.Unknown { id: id, payload: [] }, wire::Error.PacketTooLarge { id: id, serverbound: true, extra: wire::remaining(r) }; }
    return copy(p), wire::Error.Ok;
}
fn encode_serverbound(p: Serverbound..&) -> u8[>..] {
    var out: u8[>..] = [];
    match p {
        ClientInformation v => { wire::write_varint(out, 0); write_ClientInformation(v.value, out); },
        CookieResponse v => { wire::write_varint(out, 1); write_CookieResponse(v.value, out); },
        CustomPayload v => { wire::write_varint(out, 2); write_CustomPayload(v.value, out); },
        FinishConfiguration => { wire::write_varint(out, 3); },
        KeepAlive v => { wire::write_varint(out, 4); write_KeepAlive(v.value, out); },
        Pong v => { wire::write_varint(out, 5); write_Ping(v.value, out); },
        ResourcePack v => { wire::write_varint(out, 6); write_ResourcePackResponse(v.value, out); },
        SelectKnownPacks v => { wire::write_varint(out, 7); write_SelectKnownPacks(v.value, out); },
        CustomClickAction v => { wire::write_varint(out, 8); write_CustomClickAction(v.value, out); },
        AcceptCodeOfConduct => { wire::write_varint(out, 9); },
        Unknown v => { wire::write_varint(out, v.id); out.append(v.payload); },
    }
    return out;
}
fn default_information(view_distance: i8) -> ClientInformation {
    return ClientInformation { language: "en_us", view_distance: view_distance, chat_visibility: 0, chat_colors: true,
        model_customisation: 127, main_hand: 1, text_filtering: false, allows_listing: true, particle_status: 0 };
}
