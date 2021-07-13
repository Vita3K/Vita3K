#pragma once

#include <imgui.h>

#include <cstdint>

struct SDL_Window;
struct SDL_Cursor;

namespace renderer {
struct State;
}

struct ImGui_State {
    SDL_Window *window{};
    renderer::State *renderer{};

    uint64_t time = 0;
    bool mouse_pressed[3] = { false, false, false };
    SDL_Cursor *mouse_cursors[ImGuiMouseCursor_COUNT] = { nullptr };

    bool init = false;
    bool do_clear_screen = true;

    virtual ~ImGui_State() = default;
};

class ImGui_Texture {
    ImGui_State *state = nullptr;
    ImTextureID texture_id = nullptr;

public:
    ImGui_Texture() = default;
    ImGui_Texture(ImGui_State *new_state, void *data, int width, int height);
    ImGui_Texture(ImGui_Texture &&texture) noexcept;

    void init(ImGui_State *new_state, ImTextureID texture);
    void init(ImGui_State *new_state, void *data, int width, int height);

    operator bool() const;
    operator ImTextureID() const;
    bool operator==(const ImGui_Texture &texture);

    ImGui_Texture &operator=(ImGui_Texture &&texture) noexcept;
    ImGui_Texture &operator=(const ImGui_Texture &texture) = delete;

    ~ImGui_Texture();
};
