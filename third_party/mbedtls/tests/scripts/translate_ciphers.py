#!/usr/bin/env python3

# translate_ciphers.py
#
# Copyright The Mbed TLS Contributors
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Translate ciphersuite names in Mbed TLS format to OpenSSL and GNUTLS
standards.

To test the translation functions run:
python3 -m unittest translate_cipher.py
"""

import re
import argparse
import unittest

class TestTranslateCiphers(unittest.TestCase):
    """
    Ensure translate_ciphers.py translates and formats ciphersuite names
    correctly
    """
    def test_translate_all_cipher_names(self):
        """
        Translate MbedTLS ciphersuite names to their OpenSSL and
        GnuTLS counterpart. Use only a small subset of ciphers
        that exercise each step of the translate functions
        """
        ciphers = [
            ("TLS-ECDHE-ECDSA-WITH-NULL-SHA",
             "+ECDHE-ECDSA:+NULL:+SHA1",
             "ECDHE-ECDSA-NULL-SHA"),
            ("TLS-ECDHE-ECDSA-WITH-AES-128-GCM-SHA256",
             "+ECDHE-ECDSA:+AES-128-GCM:+AEAD",
             "ECDHE-ECDSA-AES128-GCM-SHA256"),
            ("TLS-DHE-RSA-WITH-3DES-EDE-CBC-SHA",
             "+DHE-RSA:+3DES-CBC:+SHA1",
             "EDH-RSA-DES-CBC3-SHA"),
            ("TLS-RSA-WITH-AES-256-CBC-SHA",
             "+RSA:+AES-256-CBC:+SHA1",
             "AES256-SHA"),
            ("TLS-PSK-WITH-3DES-EDE-CBC-SHA",
             "+PSK:+3DES-CBC:+SHA1",
             "PSK-3DES-EDE-CBC-SHA"),
            ("TLS-ECDHE-ECDSA-WITH-CHACHA20-POLY1305-SHA256",
             None,
             "ECDHE-ECDSA-CHACHA20-POLY1305"),
            ("TLS-ECDHE-ECDSA-WITH-AES-128-CCM",
             "+ECDHE-ECDSA:+AES-128-CCM:+AEAD",
             None),
            ("TLS-ECDHE-RSA-WITH-ARIA-256-GCM-SHA384",
             None,
             "ECDHE-ARIA256-GCM-SHA384"),
        ]

        for m, g_exp, o_exp in ciphers:

            if g_exp is not None:
                g = translate_gnutls(m)
                self.assertEqual(g, g_exp)

            if o_exp is not None:
                o = translate_ossl(m)
                self.assertEqual(o, o_exp)

def translate_gnutls(m_cipher):
    """
    Translate m_cipher from Mbed TLS ciphersuite naming convention
    and return the GnuTLS naming convention
    """

    m_cipher = re.sub(r'\ATLS-', '+', m_cipher)
    m_cipher = m_cipher.replace("-WITH-", ":+")
    m_cipher = m_cipher.replace("-EDE", "")

    # SHA in Mbed TLS == SHA1 GnuTLS,
    # if the last 3 chars are SHA append 1
    if m_cipher[-3:] == "SHA":
        m_cipher = m_cipher+"1"

    # CCM or CCM-8 should be followed by ":+AEAD"
    # Replace "GCM:+SHAxyz" with "GCM:+AEAD"
    if "CCM" in m_cipher or "GCM" in m_cipher:
        m_cipher = re.sub(r"GCM-SHA\d\d\d", "GCM", m_cipher)
        m_cipher = m_cipher+":+AEAD"

    # Replace the last "-" with ":+"
    else:
        index = m_cipher.rindex("-")
        m_cipher = m_cipher[:index] + ":+" + m_cipher[index+1:]

    return m_cipher

def translate_ossl(m_cipher):
    """
    Translate m_cipher from Mbed TLS ciphersuite naming convention
    and return the OpenSSL naming convention
    """

    m_cipher = re.sub(r'^TLS-', '', m_cipher)
    m_cipher = m_cipher.replace("-WITH", "")

    # Remove the "-" from "ABC-xyz"
    m_cipher = m_cipher.replace("AES-", "AES")
    m_cipher = m_cipher.replace("CAMELLIA-", "CAMELLIA")
    m_cipher = m_cipher.replace("ARIA-", "ARIA")

    # Remove "RSA" if it is at the beginning
    m_cipher = re.sub(r'^RSA-', r'', m_cipher)

    # For all circumstances outside of PSK
    if "PSK" not in m_cipher:
        m_cipher = m_cipher.replace("-EDE", "")
        m_cipher = m_cipher.replace("3DES-CBC", "DES-CBC3")

        # Remove "CBC" if it is not prefixed by DES
        m_cipher = re.sub(r'(?<!DES-)CBC-', r'', m_cipher)

    # ECDHE-RSA-ARIA does not exist in OpenSSL
    m_cipher = m_cipher.replace("ECDHE-RSA-ARIA", "ECDHE-ARIA")

    # POLY1305 should not be followed by anything
    if "POLY1305" in m_cipher:
        index = m_cipher.rindex("POLY1305")
        m_cipher = m_cipher[:index+8]

    # If DES is being used, Replace DHE with EDH
    if "DES" in m_cipher and "DHE" in m_cipher and "ECDHE" not in m_cipher:
        m_cipher = m_cipher.replace("DHE", "EDH")

    return m_cipher

def format_ciphersuite_names(mode, names):
    t = {"g": translate_gnutls, "o": translate_ossl}[mode]
    return " ".join(t(c) for c in names)

def main(target, names):
    print(format_ciphersuite_names(target, names))

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser()
    PARSER.add_argument('target', metavar='TARGET', choices=['o', 'g'])
    PARSER.add_argument('names', metavar='NAMES', nargs='+')
    ARGS = PARSER.parse_args()
    main(ARGS.target, ARGS.names)
