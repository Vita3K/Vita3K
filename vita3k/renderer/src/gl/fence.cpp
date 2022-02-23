// Vita3K emulator project
// Copyright (C) 2022 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

// NOTE: I revised algorithm from RPCS3's GLHelpers.h. Please look at the class fence. Asked kd for advice.
// I (pent0) mostly rewritten to understand it, other contributors in future can see the rpcs3 code.

#include <renderer/gl/fence.h>
#include <util/log.h>

namespace renderer::gl {

Fence::Fence()
    : sync_(nullptr)
    , signaled_(false) {
}

Fence::~Fence() {
    if (sync_) {
        glDeleteSync(sync_);
    }
}

void Fence::insert() {
    if (sync_ && !signaled_) {
        if (!wait_for_signal()) {
            LOG_ERROR("Unable to wait for previous sync to be done!");
            return;
        }
    }

    sync_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    if (!sync_) {
        LOG_ERROR("Unable to create fence sync object!");
    }
}

bool Fence::wait_for_signal() {
    if (signaled_) {
        if (sync_) {
            glDeleteSync(sync_);
            sync_ = nullptr;
        }

        return true;
    }

    GLenum error = GL_WAIT_FAILED;
    bool is_done = false;
    bool check_status = false;

    while (!is_done) {
        if (!check_status) {
            error = glClientWaitSync(sync_, GL_SYNC_FLUSH_COMMANDS_BIT, 0);

            switch (error) {
            case GL_TIMEOUT_EXPIRED:
                check_status = true;
                continue;

            case GL_ALREADY_SIGNALED:
            case GL_CONDITION_SATISFIED:
                is_done = true;
                break;

            default:
                LOG_ERROR("Unknown error while retrieving wait sync result {}", error);
                is_done = true;
                break;
            }
        } else {
            // Get the status
            GLint length = 0;
            GLint status = GL_UNSIGNALED;

            glGetSynciv(sync_, GL_SYNC_STATUS, 1, &length, &status);

            if (status == GL_SIGNALED) {
                break;
            }
        }
    }

    signaled_ = ((error == GL_ALREADY_SIGNALED) || (error == GL_CONDITION_SATISFIED));

    glDeleteSync(sync_);
    sync_ = nullptr;

    return signaled_;
}

} // namespace renderer::gl
