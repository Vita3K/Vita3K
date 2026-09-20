// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#include <renderer/functions.h>

#include <gtest/gtest.h>

TEST(command_list, append_copies_ownership_through_declared_tail) {
    renderer::Command first{};
    renderer::Command last{};
    renderer::Command stale_tail{};
    first.opcode = renderer::CommandOpcode::SetContext;
    first.flags = renderer::Command::FLAG_NO_FREE;
    first.next = &last;
    renderer::RenderTarget *render_target = nullptr;
    auto *color_surface = new SceGxmColorSurface{};
    auto *depth_stencil_surface = new SceGxmDepthStencilSurface{};
    color_surface->width = 960;
    depth_stencil_surface->background_depth = 0.5f;
    renderer::CommandHelper source_helper(&first);
    source_helper.push(render_target);
    source_helper.push(color_surface);
    source_helper.push(depth_stencil_surface);
    last.opcode = renderer::CommandOpcode::Nop;
    last.flags = renderer::Command::FLAG_NO_FREE;
    last.next = &stale_tail;

    renderer::CommandList source{};
    source.first = &first;
    source.last = &last;

    renderer::Context destination;
    destination.alloc_func = []() {
        auto *command = new renderer::Command;
        command->flags = renderer::Command::FLAG_FROM_HOST;
        return command;
    };

    renderer::append_command_list(destination, source);

    ASSERT_NE(destination.command_list.first, nullptr);
    EXPECT_NE(destination.command_list.first, &first);
    EXPECT_EQ(destination.command_list.first->opcode, renderer::CommandOpcode::SetContext);
    EXPECT_EQ(destination.command_list.first->flags, renderer::Command::FLAG_FROM_HOST);
    renderer::CommandHelper copy_helper(destination.command_list.first);
    EXPECT_EQ(copy_helper.pop<renderer::RenderTarget *>(), render_target);
    const auto *color_surface_copy = copy_helper.pop<SceGxmColorSurface *>();
    const auto *depth_stencil_surface_copy = copy_helper.pop<SceGxmDepthStencilSurface *>();
    ASSERT_NE(color_surface_copy, nullptr);
    EXPECT_NE(color_surface_copy, color_surface);
    ASSERT_NE(depth_stencil_surface_copy, nullptr);
    EXPECT_NE(depth_stencil_surface_copy, depth_stencil_surface);
    renderer::destroy_command_payload(first);
    EXPECT_EQ(color_surface_copy->width, 960);
    EXPECT_EQ(depth_stencil_surface_copy->background_depth, 0.5f);
    ASSERT_NE(destination.command_list.last, nullptr);
    EXPECT_NE(destination.command_list.last, &last);
    EXPECT_EQ(destination.command_list.last->opcode, renderer::CommandOpcode::Nop);
    EXPECT_EQ(destination.command_list.last->next, nullptr);

    renderer::destroy_command_payload(*destination.command_list.first);
    delete destination.command_list.last;
    delete destination.command_list.first;
}
