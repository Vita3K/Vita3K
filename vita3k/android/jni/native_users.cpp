// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
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

#include "android_state.h"

#include <app/functions.h>
#include <config/functions.h>
#include <config/state.h>
#include <io/state.h>

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getUsers(JNIEnv *env, jclass) {
    jclass user_class = env->FindClass("org/vita3k/emulator/data/NativeUser");
    if (!user_class)
        return nullptr;

    jmethodID ctor = env->GetMethodID(user_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Z)V");
    if (!ctor)
        return env->NewObjectArray(0, user_class, nullptr);

    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewObjectArray(0, user_class, nullptr);

    const auto &users = emuenv->app.user_list.users;
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(users.size()), user_class, nullptr);

    jsize index = 0;
    for (const auto &[id, user] : users) {
        jstring user_id = env->NewStringUTF(id.c_str());
        jstring name = env->NewStringUTF(user.name.c_str());
        const jboolean is_active = (id == emuenv->io.user_id) ? JNI_TRUE : JNI_FALSE;

        jobject user_obj = env->NewObject(user_class, ctor, user_id, name, is_active);
        env->SetObjectArrayElement(result, index++, user_obj);

        env->DeleteLocalRef(user_obj);
        env->DeleteLocalRef(user_id);
        env->DeleteLocalRef(name);
    }

    return result;
}

JNIEXPORT jstring JNICALL
Java_org_vita3k_emulator_NativeLib_createUser(JNIEnv *env, jclass, jstring name_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewStringUTF("");

    const std::string name = jstring_to_string(env, name_str);
    if (name.empty())
        return env->NewStringUTF("");

    const std::string new_id = app::create_user(*emuenv, name);
    if (new_id.empty())
        return env->NewStringUTF("");

    if (emuenv->io.user_id.empty() && app::activate_user(*emuenv, new_id)) {
        emuenv->cfg.user_id = new_id;
        config::serialize_config(emuenv->cfg, emuenv->cfg.config_path);
    }

    return env->NewStringUTF(new_id.c_str());
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_activateUser(JNIEnv *env, jclass, jstring user_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string user_id = jstring_to_string(env, user_id_str);
    if (!app::activate_user(*emuenv, user_id))
        return JNI_FALSE;

    emuenv->cfg.user_id = user_id;
    config::serialize_config(emuenv->cfg, emuenv->cfg.config_path);
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_org_vita3k_emulator_NativeLib_deleteUser(JNIEnv *env, jclass, jstring user_id_str) {
    auto *emuenv = get_emuenv();
    if (!emuenv)
        return JNI_FALSE;

    const std::string user_id = jstring_to_string(env, user_id_str);
    if (emuenv->app.user_list.users.find(user_id) == emuenv->app.user_list.users.end())
        return JNI_FALSE;

    app::delete_user(*emuenv, user_id);

    if (emuenv->io.user_id.empty() && !emuenv->app.user_list.users.empty()) {
        const auto first = emuenv->app.user_list.users.begin();
        if (!app::activate_user(*emuenv, first->first))
            return JNI_FALSE;

        emuenv->cfg.user_id = first->first;
        config::serialize_config(emuenv->cfg, emuenv->cfg.config_path);
    }

    return JNI_TRUE;
}

} // extern "C"
