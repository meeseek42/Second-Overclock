# License scope and third-party notices

## This repository's original work

Unless a file or section states otherwise, the original source code and authored data in this repository are licensed under the Mozilla Public License, version 2.0 (MPL-2.0). The complete license text is in [`LICENSE`](LICENSE).

This notice does not relicense or grant rights to Deep Rock Galactic, Unreal Engine, Epic Games, any game SDK/API, or other third-party material. Those materials remain subject to their own licenses and terms and are not included or redistributed in this publication payload. The build scripts expect those tools and inputs to be supplied separately. Existing source notices and legacy byte-pinned files are unchanged; no blanket source headers are added here.

## Build-time NuGet dependencies

The .NET inspection tools under `Analysis/tools` restore these dependencies from NuGet at build time. Their package contents are not vendored in this repository.

| Package and version/range | License | Upstream license and source |
| --- | --- | --- |
| [UAssetAPI 1.1.0](https://www.nuget.org/packages/UAssetAPI/1.1.0) | MIT | [source and license](https://github.com/atenfyr/UAssetAPI) · [LICENSE](https://github.com/atenfyr/UAssetAPI/blob/master/LICENSE) |
| [Newtonsoft.Json >= 13.0.3](https://www.nuget.org/packages/Newtonsoft.Json/) (transitive dependency of UAssetAPI) | MIT | [source and license](https://github.com/JamesNK/Newtonsoft.Json/blob/master/LICENSE.md) |
| [ZstdSharp.Port >= 0.8.1](https://www.nuget.org/packages/ZstdSharp.Port/0.8.1) (transitive dependency of UAssetAPI) | MIT | [source and license](https://github.com/oleg-st/ZstdSharp/blob/master/LICENSE) |

The upstream license and notice files linked above control the corresponding packages. This file is a locator and scope statement, not a replacement for those notices.
