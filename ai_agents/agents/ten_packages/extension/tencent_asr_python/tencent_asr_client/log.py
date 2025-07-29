import logging
import logging.handlers
from pathlib import Path

module_logger = None


def _default_logger(
    name: str = "tencent_asr", level: str = "INFO", log_path: str | None = None
):
    FORMAT = "%(asctime)15s %(name)s-%(levelname)s  %(funcName)s:%(lineno)s %(message)s"
    logging.basicConfig(level=logging.DEBUG, format=FORMAT)
    logger = logging.getLogger(name)

    if log_path is not None:
        _path = Path(log_path)
        _path.mkdir(parents=True, exist_ok=True)
        log_file = _path / f"{name}.log"
    else:
        log_file = f"{name}.log"

    handler = logging.handlers.RotatingFileHandler(
        str(log_file), maxBytes=1024 * 1024, backupCount=5, encoding="utf-8"
    )
    handler.setLevel(logging.DEBUG)
    handler.setFormatter(logging.Formatter(FORMAT))
    logger.addHandler(handler)
    logger.setLevel(level)
    return logger


def get_logger(name: str = "tencent_asr", level: str = "INFO", log_path: str | None = None):
    global module_logger
    if module_logger is None:
        module_logger = _default_logger(name, level, log_path)
    return module_logger


def set_logger(logger: logging.Logger):
    global module_logger
    module_logger = logger
