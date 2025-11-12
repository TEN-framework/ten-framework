#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from http.client import RemoteDisconnected
import json
import ssl
import socket
import time
from urllib import request, error


ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE


def send_request_with_body(uri: str, body, method: str):
    body = json.dumps(body).encode("utf8")
    header = {"Content-Type": "application/json"}

    req = request.Request(url=uri, data=body, headers=header, method=method)
    try:
        # Ignore the proxy settings in the environment.
        proxy_handler = request.ProxyHandler({})
        https_handler = request.HTTPSHandler(context=ctx)
        opener = request.build_opener(proxy_handler, https_handler)

        with opener.open(req) as stream:
            res = stream.read().decode("utf8")
            return res
    except error.HTTPError as e:
        return e.code
    except Exception as e:
        return str(e)


def post(uri: str, body):
    return send_request_with_body(uri, body, "POST")


def put(uri: str, body):
    return send_request_with_body(uri, body, "PUT")


def patch(uri: str, body):
    return send_request_with_body(uri, body, "PATCH")


def delete(uri: str):
    req = request.Request(url=uri, method="DELETE")
    try:
        # Ignore the proxy settings in the environment.
        proxy_handler = request.ProxyHandler({})
        https_handler = request.HTTPSHandler(context=ctx)
        opener = request.build_opener(proxy_handler, https_handler)

        with opener.open(req) as stream:
            res = stream.read().decode("utf8")
            return res
    except error.HTTPError as e:
        return e.code
    except Exception as e:
        return str(e)


def get(uri: str):
    try:
        # Ignore the proxy settings in the environment.
        proxy_handler = request.ProxyHandler({})
        https_handler = request.HTTPSHandler(context=ctx)
        opener = request.build_opener(proxy_handler, https_handler)

        with opener.open(uri) as stream:
            data = stream.read()
            return data.decode("utf8")
    except error.HTTPError as e:
        return e.code
    except Exception as e:
        return str(e)


def detect_port(ip: str, port: int) -> bool:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((ip, int(port)))
        s.shutdown(2)
        return True
    except Exception:
        return False


def is_app_started(ip: str, port: int, timeout=10) -> bool:
    duration = 0
    is_started = False

    while duration < timeout:
        is_started = detect_port(ip, port)
        if is_started:
            break

        duration += 1
        time.sleep(1)

    return is_started


def is_app_stopped(ip: str, port: int, timeout=10) -> bool:
    duration = 0
    is_running = True

    while duration < timeout:
        is_running = detect_port(ip, port)
        if not is_running:
            break

        duration += 1
        time.sleep(1)

    if not is_running:
        print("The TEN app stops in %d seconds" % (duration))

    return not is_running


def stop_app(ip: str, port: int, timeout=10, is_https=False) -> bool:
    schema = "http"
    if is_https:
        schema = "https"

    uri = "%s://%s:%d/" % (schema, ip, port)
    body = json.dumps({"ten": {"type": "close_app"}}).encode("utf8")
    header = {"Content-Type": "application/json"}

    req = request.Request(url=uri, data=body, headers=header, method="POST")
    try:
        # Ignore the proxy settings in the environment.
        proxy_handler = request.ProxyHandler({})
        https_handler = request.HTTPSHandler(context=ctx)
        opener = request.build_opener(proxy_handler, https_handler)

        handler = opener.open(req)
        handler.close()
    except RemoteDisconnected:
        pass
    except error.HTTPError:
        pass

    return is_app_stopped(ip, port, timeout)
