import logging
import logging.handlers
from pathlib import Path


class LoggerManager:
    """Logger manager singleton"""

    _instance = None
    _logger = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def _default_logger(
        self,
        name: str = "openai_asr",
        level: str = "INFO",
        log_path: str | None = None,
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

    def get_logger(
        self,
        name: str = "openai_asr",
        level: str = "INFO",
        log_path: str | None = None,
    ):
        if self._logger is None:
            self._logger = self._default_logger(name, level, log_path)
        return self._logger

    def set_logger(self, logger: logging.Logger):
        self._logger = logger


# create singleton instance
_logger_manager = LoggerManager()


def get_logger(
    name: str = "tencent_asr", level: str = "INFO", log_path: str | None = None
):
    """Get logger instance"""
    return _logger_manager.get_logger(name, level, log_path)


def set_logger(logger: logging.Logger):
    """Set logger instance"""
    _logger_manager.set_logger(logger)
