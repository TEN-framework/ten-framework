import logging
import logging.handlers


FORMAT = (
    "%(asctime)15s %(name)s-%(levelname)s  %(funcName)s:%(lineno)s %(message)s"
)
logging.basicConfig(level=logging.DEBUG, format=FORMAT)
logger = logging.getLogger("tencent_speech.log")

logger.setLevel("INFO")
