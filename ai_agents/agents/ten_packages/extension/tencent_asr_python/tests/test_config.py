from ..config import TencentASRConfig


def test_config():
    property_json = {
        "params": {
            "secretid": "fake_secretid",
            "engine_model_type": "16k_en",
            "voice_format": 1,
            "word_info": 2,
            "appid": "fake_app_id",
            "secretkey": "fake_secret_key",
            "finalize_mode": "mute_pkg",
            "vad_silence_time": 900,
            "log_level": "DEBUG",
            "max_speak_time": 80000,
        },
    }
    config = TencentASRConfig.model_validate(property_json)
    assert config.params.secretid == "fake_secretid"
    assert config.params.engine_model_type == "16k_en"
    assert config.params.voice_format == 1
    assert config.params.word_info == 2
    assert config.params.appid == "fake_app_id"
    assert config.params.secretkey == "fake_secret_key"


def test_compatible_config():
    property_json = {
        "language": "en-US",
        "params": {
            "secret_id": "fake_secretid",
            "voice_format": 1,
            "word_info": 2,
            "app_id": "fake_app_id",
            "secret_key": "fake_secret_key",
            "finalize_mode": "mute_pkg",
            "vad_silence_time": 900,
            "log_level": "DEBUG",
            "max_speak_time": 80000,
        },
    }
    config = TencentASRConfig.model_validate(property_json)
    assert config.params.engine_model_type == "16k_en"
    assert config.params.secretid == "fake_secretid"
    assert config.params.secretkey == "fake_secret_key"
    assert config.params.appid == "fake_app_id"


def test_compatible_config2():
    property_json = {
        "params": {
            "language": "en-US",
            "secret_id": "fake_secretid",
            "voice_format": 1,
            "word_info": 2,
            "app_id": "fake_app_id",
            "secret_key": "fake_secret_key",
            "finalize_mode": "mute_pkg",
            "vad_silence_time": 900,
            "log_level": "DEBUG",
            "max_speak_time": 80000,
        },
    }
    config = TencentASRConfig.model_validate(property_json)
    assert config.params.engine_model_type == "16k_en"
    assert config.params.secretid == "fake_secretid"
    assert config.params.secretkey == "fake_secret_key"
    assert config.params.appid == "fake_app_id"


def test_compatible_config3():
    property_json = {
        "params": {
            "language": "en-US",
            "secret": "fake_secret_key",
            "voice_format": 1,
            "word_info": 2,
            "app_id": "fake_app_id",
            "key": "fake_secretid",
            "finalize_mode": "mute_pkg",
            "vad_silence_time": 900,
            "log_level": "DEBUG",
            "max_speak_time": 80000,
            "hotword_list": ["aaa|5", "bbb|5"],
        },
        "vad_silence_time": 0,  # ignore
        "max_speak_time": 0,  # ignore
    }
    config = TencentASRConfig.model_validate(property_json)
    assert config.params.engine_model_type == "16k_en"
    assert config.params.secretid == "fake_secretid"
    assert config.params.secretkey == "fake_secret_key"
    assert config.params.appid == "fake_app_id"
    assert config.params.vad_silence_time == 900
    assert config.params.max_speak_time == 80000
    assert config.params.hotword_list == "aaa|5,bbb|5"


def test_compatible_config4():
    property_json = {
        "params": {
            "language": "en-US",
            "secret": "fake_secret_key",
            "voice_format": 1,
            "word_info": 2,
            "app_id": "fake_app_id",
            "key": "fake_secretid",
            "finalize_mode": "mute_pkg",
            "vad_silence_time": 900,
            "log_level": "DEBUG",
            "max_speak_time": 80000,
            "hotword_list": "aaa|5,bbb|5",
        },
        "vad_silence_time": 0,  # ignore
        "max_speak_time": 0,  # ignore
    }
    config = TencentASRConfig.model_validate(property_json)
    assert config.params.engine_model_type == "16k_en"
    assert config.params.secretid == "fake_secretid"
    assert config.params.secretkey == "fake_secret_key"
    assert config.params.appid == "fake_app_id"
    assert config.params.vad_silence_time == 900
    assert config.params.max_speak_time == 80000
    assert config.params.hotword_list == "aaa|5,bbb|5"
