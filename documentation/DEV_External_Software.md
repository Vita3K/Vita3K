# External Software (Developers edition)

Vita3K uses multiple pieces of external software. The main five are Dynarmic, Unicorn, SDL, OpenGL and Vulkan.

# Dynarmic

Notice that Dynarmic is Dyn**arm**ic and not Dyn**am**ic. This is because Dynarmic is an ARM interpreter. All it does is converts low-end ARM instructions to high-end, executable instructions for the CPU. It keeps the emulator fast without having to emulate every little bit of the CPU, which is a major upside to Vita3K.

Before we continue, you can look at the Dynarmic GitHub repo by clicking [here](https://github.com/merryhime/dynarmic).
