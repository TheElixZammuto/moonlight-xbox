# Submodule patches

The published repository keeps the submodule gitlinks at their **upstream** commits.
The Windows 10 Mobile changes are stored as patches here and applied by
`apply-submodule-patches.ps1` / `.cmd`.

## Apply

```powershell
git submodule update --init --recursive
powershell -NoProfile -ExecutionPolicy Bypass -File .\apply-submodule-patches.ps1
```

`setup-dev.ps1` and `generate-thirdparty-projects-arm.bat` call this automatically.

The script is idempotent: it checks `git apply --reverse --check` and skips patches
that are already applied. Use `-Check` to verify without modifying anything.

## Patch bases

| Patch | Submodule | Upstream base | Patched tree |
|---|---|---|---|
| `moonlight-common-c.patch` | `third_party/moonlight-common-c` | `8d665ce9b040107d5ad0a8e2ed3e6e123d4bde20` | `f941c026303ddca515ac04be5ec899108789009d` |
| `vcpkg.patch` | `vcpkg` | `9e593bb18ea69cc5095e012465dcd675a822ed0d` | `1588af7d59632178d7f218334a8fe3e6f73d4c74` |
| `DirectXTK.patch` | `third_party/DirectXTK` | `a57b9b7625c5b0642f7439ea413fdf39b4a02272` | `449ecaf83a79001cb89f69e7d8c7a008fe91ec43` |
| `tracy-xbox.patch` | `third_party/tracy-xbox` | `f8fdd1e3a8aad23cefc87b61dd751910eef3abb5` | `e44f7d11818cdb52fd7f09e6704b1f46ee109877` |

`third_party/imgui` and `third_party/pyrowave` are unmodified and stay at their
upstream gitlinks.

## Why patches

Publishing only `moonlight-xbox` must not depend on local submodule commits that
exist only on this machine. The commit graph of the published repository therefore
points at upstream submodule commits; the build applies the port changes from these
patch files.
