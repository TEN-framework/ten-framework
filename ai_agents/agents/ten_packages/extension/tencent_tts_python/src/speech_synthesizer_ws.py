# -*- coding: utf-8 -*-
import sys
import hmac
import hashlib
import base64
import time
import json
import threading
from websocket import ABNF, WebSocketApp
import uuid
import urllib
from .common.log import logger


_PROTOCOL = "wss://"
_HOST = "tts.cloud.tencent.com"
_PATH = "/stream_ws"
_ACTION = "TextToStreamAudioWS"


class SpeechSynthesisListener(object):
    """ """

    def on_synthesis_start(self, session_id):
        logger.info("on_synthesis_start: session_id={}".format(session_id))

    def on_synthesis_end(self):
        logger.info("on_synthesis_end: -")

    def on_audio_result(self, audio_bytes):
        logger.info(
            "on_audio_result: recv audio bytes, len={}".format(len(audio_bytes))
        )

    def on_text_result(self, response):
        session_id = response["session_id"]
        request_id = response["request_id"]
        message_id = response["message_id"]
        result = response["result"]
        subtitles = []
        if "subtitles" in result and len(result["subtitles"]) > 0:
            subtitles = result["subtitles"]
        logger.info(
            "on_text_result: session_id={} request_id={} message_id={}\nsubtitles={}".format(
                session_id, request_id, message_id, subtitles
            )
        )

    def on_synthesis_fail(self, response):
        logger.error(
            "on_synthesis_fail: code={} msg={}".format(
                response["code"], response["message"]
            )
        )


NOTOPEN = 0
STARTED = 1
OPENED = 2
FINAL = 3
ERROR = 4
CLOSED = 5


class SpeechSynthesizer:

    def __init__(self, appid, credential, listener):
        self.appid = appid
        self.credential = credential
        self.status = NOTOPEN
        self.ws = None
        self.wst = None
        self.listener = listener

        self.text = "欢迎使用腾讯云实时语音合成"
        self.voice_type = 0
        self.codec = "pcm"
        self.sample_rate = 16000
        self.volume = 0
        self.speed = 0
        self.session_id = ""
        self.enable_subtitle = True
        self.fast_voice_type = ""

    def set_voice_type(self, voice_type):
        self.voice_type = voice_type

    def set_codec(self, codec):
        self.codec = codec

    def set_sample_rate(self, sample_rate):
        self.sample_rate = sample_rate

    def set_speed(self, speed):
        self.speed = speed

    def set_volume(self, volume):
        self.volume = volume

    def set_text(self, text):
        self.text = text

    def set_enable_subtitle(self, enable_subtitle):
        self.enable_subtitle = enable_subtitle

    def set_fast_voice_type(self, fast_voice_type):
        self.fast_voice_type = fast_voice_type

    def __gen_signature(self, params):
        sort_dict = sorted(params.keys())
        sign_str = "GET" + _HOST + _PATH + "?"
        for key in sort_dict:
            sign_str = sign_str + key + "=" + str(params[key]) + "&"
        sign_str = sign_str[:-1]
        secret_key = self.credential.secret_key.encode("utf-8")
        sign_str = sign_str.encode("utf-8")
        hmacstr = hmac.new(secret_key, sign_str, hashlib.sha1).digest()
        s = base64.b64encode(hmacstr)
        s = s.decode("utf-8")
        return s

    def __gen_params(self, session_id):
        self.session_id = session_id

        params = dict()
        params["Action"] = _ACTION
        params["AppId"] = int(self.appid)
        params["SecretId"] = self.credential.secret_id
        params["ModelType"] = 1
        params["VoiceType"] = self.voice_type
        params["Codec"] = self.codec
        params["SampleRate"] = self.sample_rate
        params["Speed"] = self.speed
        params["Volume"] = self.volume
        params["SessionId"] = self.session_id
        params["Text"] = self.text
        params["EnableSubtitle"] = self.enable_subtitle
        if len(self.fast_voice_type) > 0:
            params["FastVoiceType"] = self.fast_voice_type

        timestamp = int(time.time())
        params["Timestamp"] = timestamp
        params["Expired"] = timestamp + 24 * 60 * 60
        return params

    def __create_query_string(self, param):
        param["Text"] = urllib.parse.quote(param["Text"])

        param = sorted(param.items(), key=lambda d: d[0])

        url = _PROTOCOL + _HOST + _PATH

        signstr = url + "?"
        for x in param:
            tmp = x
            for t in tmp:
                signstr += str(t)
                signstr += "="
            signstr = signstr[:-1]
            signstr += "&"
        signstr = signstr[:-1]
        return signstr

    def start(self):
        logger.info("synthesizer start: begin")

        def _close_conn(reason):
            ta = time.time()
            self.ws.close()
            tb = time.time()
            logger.info(
                "client has closed connection ({}), cost {} ms".format(
                    reason, int((tb - ta) * 1000)
                )
            )

        def _on_data(ws, data, opcode, flag):
            # NOTE print all message that client received
            # logger.info("data={} opcode={} flag={}".format(data, opcode, flag))
            if opcode == ABNF.OPCODE_BINARY:
                self.listener.on_audio_result(data)  # <class 'bytes'>
                pass
            elif opcode == ABNF.OPCODE_TEXT:
                resp = json.loads(data)  # WSResponseMessage
                if resp["code"] != 0:
                    logger.error(
                        "server synthesis fail request_id={} code={} msg={}".format(
                            resp["request_id"], resp["code"], resp["message"]
                        )
                    )
                    self.listener.on_synthesis_fail(resp)
                    return
                if "final" in resp and resp["final"] == 1:
                    logger.info("recv FINAL frame")
                    self.status = FINAL
                    _close_conn("after recv final")
                    self.listener.on_synthesis_end()
                    return
                if "result" in resp:
                    if (
                        "subtitles" in resp["result"]
                        and resp["result"]["subtitles"] is not None
                    ):
                        self.listener.on_text_result(resp)
                    return
            else:
                logger.error("invalid on_data code, opcode=".format(opcode))

        def _on_error(ws, error):
            if self.status == FINAL or self.status == CLOSED:
                return
            self.status = ERROR
            logger.error("error={}, session_id={}".format(error, self.session_id))
            _close_conn("after recv error")

        def _on_close(ws, close_status_code, close_msg):
            logger.info(
                "conn closed, close_status_code={} close_msg={}".format(
                    close_status_code, close_msg
                )
            )
            self.status = CLOSED

        def _on_open(ws):
            logger.info("conn opened")
            self.status = OPENED

        session_id = str(uuid.uuid1())
        params = self.__gen_params(session_id)
        signature = self.__gen_signature(params)
        requrl = self.__create_query_string(params)

        autho = urllib.parse.quote(signature)
        requrl += "&Signature=%s" % autho

        self.ws = WebSocketApp(
            requrl, None, on_error=_on_error, on_close=_on_close, on_data=_on_data
        )
        self.ws.on_open = _on_open

        self.wst = threading.Thread(target=self.ws.run_forever)
        self.wst.daemon = True
        self.wst.start()
        self.status = STARTED
        self.listener.on_synthesis_start(session_id)

        logger.info("synthesizer start: end")

    def wait(self):
        logger.info("synthesizer wait: begin")
        if self.ws:
            if self.wst and self.wst.is_alive():
                self.wst.join()
        logger.info("synthesizer wait: end")
