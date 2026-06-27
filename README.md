![image_alt](https://github.com/Yebot32/vitra-ios/blob/39159ade3f88ff7685493cdac771f724f91bb6a8/IMG_20260330_095953.png)

<h1 align="center"> Vitra-iOS -
 psvita emulator for ios </h1>

Vitra-iOS is a proof of concept experimental research project aimed at porting PlayStation Vita emulation to the iOS and iPadOS ecosystem. This repository serves as a technical proof of concept (PoC) to demonstrate the feasibility of Vita hardware abstraction on Apple Silicon.

🎯 Project Goals
The primary objective of Vitra-iOS is to bridge the gap between the Vita’s unique architecture and the modern iOS environment, specifically focusing on:
 * ARM64 Translation: Mapping the Vita’s Cortex-A9 instructions to native ARM64.
 * Metal Rendering: Translating GXM/PICA graphics calls into the Apple Metal API.
 * Low-Level Emulation: Researching HLE (High-Level Emulation) for Vita OS modules within the iOS sandbox.

🛠 Tech Stack
 * Core: C++
 * Frontend: Swift / SwiftUI
 * Graphics API: Metal / MoltenVK
 * Target: iOS 16.0+ / iPadOS 16.0+

⚖️ Legal Disclaimer
Vitra-iOS is a non-profit, open-source project created for educational purposes.
 * This project is not affiliated with, authorized, or endorsed by Sony Interactive Entertainment.
 * "PlayStation" and "PS Vita" are registered trademarks of Sony.
 * No proprietary Sony code, firmware, or BIOS files are hosted in this repository.
 * This project does not promote or facilitate piracy; users are expected to provide their own legally dumped software backups.
