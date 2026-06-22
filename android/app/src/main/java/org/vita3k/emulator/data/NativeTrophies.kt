package org.vita3k.emulator.data

import androidx.annotation.Keep

@Keep
class NativeTrophyCollection {
    @Keep
    var npComId: String = ""

    @Keep
    var title: String = ""

    @Keep
    var iconPath: String = ""

    @Keep
    var totalCount: Int = 0

    @Keep
    var unlockedCount: Int = 0

    @Keep
    var platinumUnlockedCount: Int = 0

    @Keep
    var goldUnlockedCount: Int = 0

    @Keep
    var silverUnlockedCount: Int = 0

    @Keep
    var bronzeUnlockedCount: Int = 0

    @Keep
    var progressPercent: Int = 0

    @Keep
    var trophies: Array<NativeTrophy> = emptyArray()
}

@Keep
class NativeTrophy {
    @Keep
    var npComId: String = ""

    @Keep
    var trophyId: Int = 0

    @Keep
    var name: String = ""

    @Keep
    var detail: String = ""

    @Keep
    var grade: Int = 0

    @Keep
    var hidden: Boolean = false

    @Keep
    var earned: Boolean = false

    @Keep
    var timestampSec: Long = 0L

    @Keep
    var iconPath: String = ""
}
