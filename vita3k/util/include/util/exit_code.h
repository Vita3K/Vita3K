#pragma once

enum class ExitCode {
    Success = 0,
    QuitRequested,
    FileNotFound,
    InvalidApplicationPath,

    InitConfigFailed,
    SDLInitFailed,
    HostInitFailed,
    RendererInitFailed,
    ModuleLoadFailed,
    InitThreadFailed,
    RunThreadFailed
};
