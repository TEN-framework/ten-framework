import logging
import logging.handlers


FORMAT = "%(asctime)15s %(name)s-%(levelname)s  %(funcName)s:%(lineno)s %(message)s"
logging.basicConfig(level=logging.DEBUG, format=FORMAT)
logger = logging.getLogger("tencent_speech.log")

handler = logging.handlers.RotatingFileHandler(
    "tencent_speech.log", maxBytes=1024 * 1024, backupCount=5, encoding="utf-8"
)
handler.setLevel(logging.DEBUG)
handler.setFormatter(logging.Formatter(FORMAT))
logger.addHandler(handler)
logger.setLevel("INFO")
