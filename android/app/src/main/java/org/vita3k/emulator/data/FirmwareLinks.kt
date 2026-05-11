package org.vita3k.emulator.data

import androidx.annotation.StringRes
import org.vita3k.emulator.R

data class FirmwareLocale(
    @StringRes val nameResId: Int,
    val code: String
)

object FirmwareLinks {
    const val PREINSTALL_URL = "https://bit.ly/4hlePsX"
    const val FONT_PACKAGE_URL = "https://bit.ly/2P2rb0r"

    val locales = listOf(
        FirmwareLocale(R.string.settings_lang_japanese, "ja-jp"),
        FirmwareLocale(R.string.settings_lang_english_us, "en-us"),
        FirmwareLocale(R.string.settings_lang_french, "fr-fr"),
        FirmwareLocale(R.string.settings_lang_spanish, "es-es"),
        FirmwareLocale(R.string.settings_lang_german, "de-de"),
        FirmwareLocale(R.string.settings_lang_italian, "it-it"),
        FirmwareLocale(R.string.settings_lang_dutch, "nl-nl"),
        FirmwareLocale(R.string.settings_lang_portuguese_pt, "pt-pt"),
        FirmwareLocale(R.string.settings_lang_russian, "ru-ru"),
        FirmwareLocale(R.string.settings_lang_korean, "ko-kr"),
        FirmwareLocale(R.string.settings_lang_chinese_trad, "zh-hant-hk"),
        FirmwareLocale(R.string.settings_lang_chinese_simp, "zh-hans-cn"),
        FirmwareLocale(R.string.settings_lang_finnish, "fi-fi"),
        FirmwareLocale(R.string.settings_lang_swedish, "sv-se"),
        FirmwareLocale(R.string.settings_lang_danish, "da-dk"),
        FirmwareLocale(R.string.settings_lang_norwegian, "no-no"),
        FirmwareLocale(R.string.settings_lang_polish, "pl-pl"),
        FirmwareLocale(R.string.settings_lang_portuguese_br, "pt-br"),
        FirmwareLocale(R.string.settings_lang_english_gb, "en-gb"),
        FirmwareLocale(R.string.settings_lang_turkish, "tr-tr")
    )

    fun coerceLocaleIndex(index: Int): Int =
        index.coerceIn(0, locales.lastIndex)

    fun firmwareDownloadUrl(index: Int): String {
        val locale = locales[coerceLocaleIndex(index)]
        return "https://www.playstation.com/${locale.code}/support/hardware/psvita/system-software/"
    }
}
