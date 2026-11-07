# Skin profiles on another device

XylarJava reads a profile's skin from the skin backend when the Home screen opens,
including on a new installation with an empty cache. Downloading a skin does not
require the key that allows changing it. Microsoft and offline profiles use the
backend's offline UUID for the player name, with the original capitalization.

A profile already claimed on one computer needs its saved access key on another
computer before the second installation can change its skin or cape.

1. On the device where changing the skin works, open **Skins & Capes → Skin access → Export access to another device**.
2. Transfer the `.xylar-skin-access` file to the other computer, select the same player and backend, and use **Skin access → Import access from another device**.

The access file allows changing that profile. Keep it private. Imported keys are
stored using the destination installation's existing local credential storage.
Access for a different player or backend is rejected. These files are ignored by
Git. Exporting access leaves the original device authorized too.

You can also manage the profile through its linked Xylar website account. Link
codes expire after ten minutes; generate a new one from the launcher when needed.

## Development checks

`XylarSkinClient` tests cover downloading without a local key, waiting for the
initial claim before an automatic upload, sharing concurrent claims, preserving
backend error statuses, rejecting unauthorized uploads and transferring profile
access. Run them with:

```bash
cmake --build build --config Debug --target XylarSkinClient
ctest --test-dir build -C Debug -R '^XylarSkinClient$' --output-on-failure
```

The website's Worker service connection is configured in the adjacent `xylarinc`
project. A website service-transport 403 and a player ownership rejection are
separate errors. Fixing the website requires deploying its Worker configuration;
changing a launcher binary does not update the running website.
