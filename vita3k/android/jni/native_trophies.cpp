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

#include <io/state.h>
#include <np/trophy/collection.h>

#include <vector>

namespace {

void clear_pending_exception(JNIEnv *env) {
    if (env->ExceptionCheck())
        env->ExceptionClear();
}

np::trophy::CollectionSource make_collection_source(EmuEnvState &emuenv) {
    np::trophy::CollectionSource source;
    source.io = &emuenv.io;
    source.vita_fs_path = emuenv.vita_fs_path;
    source.user_id = emuenv.io.user_id;
    source.lang = static_cast<uint32_t>(emuenv.cfg.sys_lang);
    return source;
}

jclass find_trophy_collection_class(JNIEnv *env) {
    jclass cls = env->FindClass("org/vita3k/emulator/data/NativeTrophyCollection");
    if (!cls)
        clear_pending_exception(env);
    return cls;
}

jclass find_trophy_class(JNIEnv *env) {
    jclass cls = env->FindClass("org/vita3k/emulator/data/NativeTrophy");
    if (!cls)
        clear_pending_exception(env);
    return cls;
}

struct TrophyCollectionFields {
    jmethodID ctor;
    jfieldID np_com_id;
    jfieldID title;
    jfieldID icon_path;
    jfieldID total_count;
    jfieldID unlocked_count;
    jfieldID platinum_unlocked_count;
    jfieldID gold_unlocked_count;
    jfieldID silver_unlocked_count;
    jfieldID bronze_unlocked_count;
    jfieldID progress_percent;
    jfieldID trophies;
};

struct TrophyFields {
    jmethodID ctor;
    jfieldID np_com_id;
    jfieldID trophy_id;
    jfieldID name;
    jfieldID detail;
    jfieldID grade;
    jfieldID hidden;
    jfieldID earned;
    jfieldID timestamp_sec;
    jfieldID icon_path;
};

bool resolve_trophy_collection_fields(JNIEnv *env, jclass cls, TrophyCollectionFields &fields) {
    fields.ctor = env->GetMethodID(cls, "<init>", "()V");
    fields.np_com_id = env->GetFieldID(cls, "npComId", "Ljava/lang/String;");
    fields.title = env->GetFieldID(cls, "title", "Ljava/lang/String;");
    fields.icon_path = env->GetFieldID(cls, "iconPath", "Ljava/lang/String;");
    fields.total_count = env->GetFieldID(cls, "totalCount", "I");
    fields.unlocked_count = env->GetFieldID(cls, "unlockedCount", "I");
    fields.platinum_unlocked_count = env->GetFieldID(cls, "platinumUnlockedCount", "I");
    fields.gold_unlocked_count = env->GetFieldID(cls, "goldUnlockedCount", "I");
    fields.silver_unlocked_count = env->GetFieldID(cls, "silverUnlockedCount", "I");
    fields.bronze_unlocked_count = env->GetFieldID(cls, "bronzeUnlockedCount", "I");
    fields.progress_percent = env->GetFieldID(cls, "progressPercent", "I");
    fields.trophies = env->GetFieldID(cls, "trophies", "[Lorg/vita3k/emulator/data/NativeTrophy;");

    const bool resolved = fields.ctor && fields.np_com_id && fields.title && fields.icon_path && fields.total_count && fields.unlocked_count && fields.platinum_unlocked_count && fields.gold_unlocked_count && fields.silver_unlocked_count && fields.bronze_unlocked_count && fields.progress_percent && fields.trophies;
    if (!resolved)
        clear_pending_exception(env);
    return resolved;
}

bool resolve_trophy_fields(JNIEnv *env, jclass cls, TrophyFields &fields) {
    fields.ctor = env->GetMethodID(cls, "<init>", "()V");
    fields.np_com_id = env->GetFieldID(cls, "npComId", "Ljava/lang/String;");
    fields.trophy_id = env->GetFieldID(cls, "trophyId", "I");
    fields.name = env->GetFieldID(cls, "name", "Ljava/lang/String;");
    fields.detail = env->GetFieldID(cls, "detail", "Ljava/lang/String;");
    fields.grade = env->GetFieldID(cls, "grade", "I");
    fields.hidden = env->GetFieldID(cls, "hidden", "Z");
    fields.earned = env->GetFieldID(cls, "earned", "Z");
    fields.timestamp_sec = env->GetFieldID(cls, "timestampSec", "J");
    fields.icon_path = env->GetFieldID(cls, "iconPath", "Ljava/lang/String;");

    const bool resolved = fields.ctor && fields.np_com_id && fields.trophy_id && fields.name && fields.detail && fields.grade && fields.hidden && fields.earned && fields.timestamp_sec && fields.icon_path;
    if (!resolved)
        clear_pending_exception(env);
    return resolved;
}

jobjectArray new_empty_trophy_collection_array(JNIEnv *env) {
    jclass collection_class = find_trophy_collection_class(env);
    if (!collection_class)
        return nullptr;

    return env->NewObjectArray(0, collection_class, nullptr);
}

void populate_trophy_collection_fields(JNIEnv *env, jobject obj, const TrophyCollectionFields &fields,
    const np::trophy::CollectionSnapshot &snapshot, jobjectArray trophies = nullptr) {
    const int progress_percent = snapshot.total > 0 ? (snapshot.unlocked * 100 / snapshot.total) : 0;

    jstring np_com_id = env->NewStringUTF(snapshot.np_com_id.c_str());
    jstring title = env->NewStringUTF(snapshot.title.c_str());
    jstring icon_path = env->NewStringUTF(snapshot.icon_path.c_str());

    env->SetObjectField(obj, fields.np_com_id, np_com_id);
    env->SetObjectField(obj, fields.title, title);
    env->SetObjectField(obj, fields.icon_path, icon_path);
    env->SetIntField(obj, fields.total_count, static_cast<jint>(snapshot.total));
    env->SetIntField(obj, fields.unlocked_count, static_cast<jint>(snapshot.unlocked));
    env->SetIntField(obj, fields.platinum_unlocked_count,
        static_cast<jint>(snapshot.grade_unlocked[static_cast<int>(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_PLATINUM)]));
    env->SetIntField(obj, fields.gold_unlocked_count,
        static_cast<jint>(snapshot.grade_unlocked[static_cast<int>(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_GOLD)]));
    env->SetIntField(obj, fields.silver_unlocked_count,
        static_cast<jint>(snapshot.grade_unlocked[static_cast<int>(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_SILVER)]));
    env->SetIntField(obj, fields.bronze_unlocked_count,
        static_cast<jint>(snapshot.grade_unlocked[static_cast<int>(np::trophy::SceNpTrophyGrade::SCE_NP_TROPHY_GRADE_BRONZE)]));
    env->SetIntField(obj, fields.progress_percent, static_cast<jint>(progress_percent));
    if (trophies)
        env->SetObjectField(obj, fields.trophies, trophies);

    env->DeleteLocalRef(np_com_id);
    env->DeleteLocalRef(title);
    env->DeleteLocalRef(icon_path);
}

jobject new_trophy_object(JNIEnv *env, jclass cls, const TrophyFields &fields,
    const np::trophy::TrophyRecord &trophy, const std::string &np_com_id) {
    jstring j_np_com_id = env->NewStringUTF(np_com_id.c_str());
    jstring name = env->NewStringUTF(trophy.name.c_str());
    jstring detail = env->NewStringUTF(trophy.detail.c_str());
    jstring icon_path = env->NewStringUTF(trophy.icon_path.c_str());

    jobject obj = env->NewObject(cls, fields.ctor);
    env->SetObjectField(obj, fields.np_com_id, j_np_com_id);
    env->SetIntField(obj, fields.trophy_id, static_cast<jint>(trophy.id));
    env->SetObjectField(obj, fields.name, name);
    env->SetObjectField(obj, fields.detail, detail);
    env->SetIntField(obj, fields.grade, static_cast<jint>(trophy.grade));
    env->SetBooleanField(obj, fields.hidden, static_cast<jboolean>(trophy.hidden));
    env->SetBooleanField(obj, fields.earned, static_cast<jboolean>(trophy.earned));
    env->SetLongField(obj, fields.timestamp_sec, static_cast<jlong>(trophy.timestamp));
    env->SetObjectField(obj, fields.icon_path, icon_path);

    env->DeleteLocalRef(j_np_com_id);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(detail);
    env->DeleteLocalRef(icon_path);
    return obj;
}

jobjectArray new_trophy_array(JNIEnv *env, jclass trophy_class, const TrophyFields &fields,
    const np::trophy::CollectionSnapshot &snapshot) {
    jobjectArray result = env->NewObjectArray(static_cast<jsize>(snapshot.trophies.size()), trophy_class, nullptr);
    for (jsize i = 0; i < static_cast<jsize>(snapshot.trophies.size()); ++i) {
        jobject trophy_obj = new_trophy_object(env, trophy_class, fields, snapshot.trophies[static_cast<size_t>(i)], snapshot.np_com_id);
        env->SetObjectArrayElement(result, i, trophy_obj);
        env->DeleteLocalRef(trophy_obj);
    }
    return result;
}

jobject new_trophy_collection_object(JNIEnv *env, jclass cls, const TrophyCollectionFields &fields,
    const np::trophy::CollectionSnapshot &snapshot, jobjectArray trophies = nullptr) {
    jobject obj = env->NewObject(cls, fields.ctor);
    populate_trophy_collection_fields(env, obj, fields, snapshot, trophies);
    return obj;
}

} // namespace

extern "C" {

JNIEXPORT jobjectArray JNICALL
Java_org_vita3k_emulator_NativeLib_getTrophyCollections(JNIEnv *env, jclass) {
    jclass collection_class = find_trophy_collection_class(env);
    if (!collection_class)
        return new_empty_trophy_collection_array(env);

    jclass trophy_class = find_trophy_class(env);
    if (!trophy_class)
        return env->NewObjectArray(0, collection_class, nullptr);

    TrophyCollectionFields fields{};
    if (!resolve_trophy_collection_fields(env, collection_class, fields))
        return env->NewObjectArray(0, collection_class, nullptr);

    TrophyFields trophy_fields{};
    if (!resolve_trophy_fields(env, trophy_class, trophy_fields))
        return env->NewObjectArray(0, collection_class, nullptr);

    auto *emuenv = get_emuenv();
    if (!emuenv)
        return env->NewObjectArray(0, collection_class, nullptr);

    const auto source = make_collection_source(*emuenv);
    const auto ids = np::trophy::list_collection_ids(source);
    std::vector<np::trophy::CollectionSnapshot> snapshots;
    snapshots.reserve(ids.size());

    for (const auto &np_com_id : ids) {
        np::trophy::CollectionSnapshot snapshot;
        if (np::trophy::load_collection(source, np_com_id, snapshot))
            snapshots.push_back(std::move(snapshot));
    }

    jobjectArray result = env->NewObjectArray(static_cast<jsize>(snapshots.size()), collection_class, nullptr);

    for (jsize index = 0; index < static_cast<jsize>(snapshots.size()); ++index) {
        const auto &snapshot = snapshots[static_cast<size_t>(index)];
        jobjectArray trophies = new_trophy_array(env, trophy_class, trophy_fields, snapshot);
        jobject collection_obj = new_trophy_collection_object(env, collection_class, fields, snapshot, trophies);
        env->SetObjectArrayElement(result, index, collection_obj);
        env->DeleteLocalRef(trophies);
        env->DeleteLocalRef(collection_obj);
    }

    return result;
}

} // extern "C"
