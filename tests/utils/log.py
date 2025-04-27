#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import logging


class ColorCodes:
    grey = "\x1b[38;21m"
    green = "\x1b[1;32m"
    yellow = "\x1b[33;21m"
    red = "\x1b[31;21m"
    bold_red = "\x1b[31;1m"
    blue = "\x1b[1;34m"
    light_blue = "\x1b[1;36m"
    purple = "\x1b[1;35m"
    pink = "\x1b[35;1m"
    normal = "\x1b[0m"


def setup_logger(logger: logging.Logger):
    logger.setLevel(logging.DEBUG)

    sh = logging.StreamHandler()
    formatter = logging.Formatter(fmt="%(asctime)s [%(levelname)s] %(message)s")
    sh.setFormatter(formatter)

    def decorate_emit(fn):
        # Add methods we need to the class
        def new(*args):
            levelno = args[0].levelno
            if levelno >= logging.CRITICAL:
                color = ColorCodes.red
            elif levelno >= logging.ERROR:
                color = ColorCodes.red
            elif levelno >= logging.WARNING:
                color = ColorCodes.yellow
            elif levelno >= logging.INFO:
                color = ColorCodes.green
            elif levelno >= logging.DEBUG:
                color = ColorCodes.pink
            else:
                color = ColorCodes.normal

            # Add colored *** in the beginning of the message
            args[0].msg = "{0}***{1} {2}".format(
                color, ColorCodes.normal, args[0].msg
            )

            # Bolder each args of message
            args[0].args = tuple(
                "\x1b[1m" + arg + "\x1b[0m" for arg in args[0].args
            )

            return fn(*args)

        return new

    sh.emit = decorate_emit(sh.emit)  # type: ignore
    logger.addHandler(sh)

    def decorate_loglevel():
        logging.addLevelName(
            logging.CRITICAL,
            "{0}{1}{2}".format(
                ColorCodes.red,
                logging.getLevelName(logging.CRITICAL),
                ColorCodes.normal,
            ),
        )

        logging.addLevelName(
            logging.ERROR,
            "{0}{1}{2}".format(
                ColorCodes.red,
                logging.getLevelName(logging.ERROR),
                ColorCodes.normal,
            ),
        )

        logging.addLevelName(
            logging.WARNING,
            "{0}{1}{2}".format(
                ColorCodes.yellow,
                logging.getLevelName(logging.WARNING),
                ColorCodes.normal,
            ),
        )

        logging.addLevelName(
            logging.INFO,
            "{0}{1}{2}".format(
                ColorCodes.green,
                logging.getLevelName(logging.INFO),
                ColorCodes.normal,
            ),
        )

        logging.addLevelName(
            logging.DEBUG,
            "{0}{1}{2}".format(
                ColorCodes.pink,
                logging.getLevelName(logging.DEBUG),
                ColorCodes.normal,
            ),
        )

    decorate_loglevel()


logger = logging.getLogger("ten_gn")
setup_logger(logger)


def info(msg, *args, **kwargs):
    logger.info(msg, *args, **kwargs)


def warn(msg, *args, **kwargs):
    logger.warning(msg, *args, **kwargs)


def error(msg, *args, **kwargs):
    logger.error(msg, *args, **kwargs)
