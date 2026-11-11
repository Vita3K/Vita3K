# Firmware SELF to ELF

Build the optional `vita3k-self-elf` target and pass an encrypted kernel or user firmware SELF and a new output path:

```powershell
cmake --build build --config RelWithDebInfo --target vita3k-self-elf
& 'build\bin\RelWithDebInfo\vita3k-self-elf.exe' 'path\to\module.skprx' 'path\to\module.elf'
```

The tool uses Vita3K's existing firmware decryption and writes the unrelocated ELF without loading or executing the module. It refuses to overwrite an existing output. Encrypted game modules requiring a license key and already-decrypted SELF files are not supported.