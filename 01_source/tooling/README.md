# Tooling metadata and dependency files

The firmware source uses English Doxygen comments; build and host scripts use their
native comment or help syntax. The JSON lock manifest is kept as strict JSON and is
documented here because JSON does not accept comments.

`../tooling.lock.json` records:

| Field | Meaning |
|---|---|
| `schema_version` | Version of the project's manifest structure. |
| `host` / `target` | Supported host platform and firmware target. |
| `esp_idf` | SDK repository, reference tag, required commit and local submodule path. |
| `blackmagic` | Debug backend repository, reference tag, required commit and local path. |
| `python` | Reference Python version, supported minor version, local runtime/venv paths and requirements file. |
| `tools` | SDK tool manifest, local installation directory and reference tool versions. |

Paths in the manifest are relative to `01_source`. `tools/setup.ps1` verifies the SDK
and backend commits before preparing the local environment. Tool downloads and
generated state remain under `.tooling`; they are not source files to annotate.

`python-requirements.lock.txt` pins packages installed into the local Python
environment. `espidf.constraints.v6.1.txt` supplies the SDK's compatibility bounds
and retains its upstream requirement explanations. A version pin and a compatible
version range serve different purposes; both files are used by the tooling workflow.
