DUMP_FILE_NAME = "soniox_asr_in.pcm"
MODULE_NAME_ASR = "asr"

# Language code mapping from ISO codes to Soniox language codes
LANGUAGE_CODE_MAPPING = {
    "af": "af-ZA",  # Afrikaans
    "sq": "sq-AL",  # Albanian
    "ar": "ar-SA",  # Arabic
    "az": "az-AZ",  # Azerbaijani
    "eu": "eu-ES",  # Basque
    "be": "be-BY",  # Belarusian
    "bn": "bn-BD",  # Bengali
    "bs": "bs-BA",  # Bosnian
    "bg": "bg-BG",  # Bulgarian
    "ca": "ca-ES",  # Catalan
    "zh": "zh-CN",  # Chinese
    "hr": "hr-HR",  # Croatian
    "cs": "cs-CZ",  # Czech
    "da": "da-DK",  # Danish
    "nl": "nl-NL",  # Dutch
    "en": "en-US",  # English
    "et": "et-EE",  # Estonian
    "fi": "fi-FI",  # Finnish
    "fr": "fr-FR",  # French
    "gl": "gl-ES",  # Galician
    "de": "de-DE",  # German
    "el": "el-GR",  # Greek
    "gu": "gu-IN",  # Gujarati
    "he": "he-IL",  # Hebrew
    "hi": "hi-IN",  # Hindi
    "hu": "hu-HU",  # Hungarian
    "id": "id-ID",  # Indonesian
    "it": "it-IT",  # Italian
    "ja": "ja-JP",  # Japanese
    "kn": "kn-IN",  # Kannada
    "kk": "kk-KZ",  # Kazakh
    "ko": "ko-KR",  # Korean
    "lv": "lv-LV",  # Latvian
    "lt": "lt-LT",  # Lithuanian
    "mk": "mk-MK",  # Macedonian
    "ms": "ms-MY",  # Malay
    "ml": "ml-IN",  # Malayalam
    "mr": "mr-IN",  # Marathi
    "no": "no-NO",  # Norwegian
    "fa": "fa-IR",  # Persian
    "pl": "pl-PL",  # Polish
    "pt": "pt-PT",  # Portuguese
    "pa": "pa-IN",  # Punjabi
    "ro": "ro-RO",  # Romanian
    "ru": "ru-RU",  # Russian
    "sr": "sr-RS",  # Serbian
    "sk": "sk-SK",  # Slovak
    "sl": "sl-SI",  # Slovenian
    "es": "es-ES",  # Spanish
    "sw": "sw-KE",  # Swahili
    "sv": "sv-SE",  # Swedish
    "tl": "tl-PH",  # Tagalog
    "ta": "ta-IN",  # Tamil
    "te": "te-IN",  # Telugu
    "th": "th-TH",  # Thai
    "tr": "tr-TR",  # Turkish
    "uk": "uk-UA",  # Ukrainian
    "ur": "ur-PK",  # Urdu
    "vi": "vi-VN",  # Vietnamese
    "cy": "cy-GB",  # Welsh
}


def map_language_code(iso_code: str) -> str:
    """
    Map ISO language code to Soniox language code.

    Args:
        iso_code: ISO language code (e.g., 'en', 'fr', 'zh')

    Returns:
        Mapped Soniox language code (e.g., 'en-US', 'fr-FR', 'zh-CN')
        If the ISO code is not supported, returns the original code
    """
    return LANGUAGE_CODE_MAPPING.get(iso_code, iso_code)


def is_supported_language(iso_code: str) -> bool:
    """
    Check if a language code is supported.

    Args:
        iso_code: ISO language code to check

    Returns:
        True if the language is supported, False otherwise
    """
    return iso_code in LANGUAGE_CODE_MAPPING
