#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
from typing import Union, Tuple
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend


def parse_log_header(data: bytes) -> Tuple[bool, int]:
    """
    Parse the header (5 bytes) of a single log entry

    Args:
        data: Byte data containing the header

    Returns:
        (valid flag, data length)
    """
    if len(data) < 5:
        return False, 0

    # Check magic number.
    if data[0] != 0xFF or data[1] != 0xFF:
        return False, 0

    # Extract version and parity bit.
    _ = data[2] & 0x3F  # First 6 bits (extracted for future use).
    parity_bit = (data[2] >> 7) & 0x01  # 8th bit.

    # Calculate actual parity.
    calculated_parity = 0
    for i in range(5):
        # Temporarily clear the 8th bit for calculation.
        byte_val = data[i]
        if i == 2:
            byte_val &= 0x7F
        calculated_parity ^= byte_val

    calculated_parity &= 0x01

    # Check parity bit.
    if parity_bit != calculated_parity:
        return False, 0

    # Get data length.
    data_len = (data[3] << 8) | data[4]

    return True, data_len


def create_decryptor(key: bytes, nonce: bytes):
    """
    Create a new AES-CTR decryptor.

    Args:
        key: AES key (16 bytes)
        nonce: Nonce for CTR mode (16 bytes)

    Returns:
        New decryptor object.
    """
    cipher = Cipher(
        algorithms.AES(key), modes.CTR(nonce), backend=default_backend()
    )
    return cipher.decryptor()


def decrypt_log_file(
    input_file: Union[str, Path],
    output_file: Union[str, Path],
    key: Union[str, bytes],
    nonce: Union[str, bytes],
) -> None:
    """
    Decrypt a file containing multiple encrypted log entries, each with its own
    5-byte header. Each log entry is independently encrypted, requiring a new
    decryptor for each entry.

    Args:
        input_file: Path to the source file to decrypt
        output_file: Path to the destination file after decryption
        key: AES key (16 bytes), either string or bytes format
        nonce: Nonce for CTR mode (16 bytes), either string or bytes format
    """
    # Ensure key and nonce are in bytes format.
    if isinstance(key, str):
        key = key.encode("utf-8")
    if isinstance(nonce, str):
        nonce = nonce.encode("utf-8")

    # Validate key length.
    if len(key) != 16:
        raise ValueError(
            f"Key must be 16 bytes (current length: {len(key)} bytes)"
        )

    # Validate nonce length.
    if len(nonce) != 16:
        raise ValueError(
            f"Nonce must be 16 bytes (current length: {len(nonce)} bytes)"
        )

    # Open input and output files.
    with open(input_file, "rb") as in_file, open(output_file, "wb") as out_file:
        log_count = 0
        total_bytes_decrypted = 0

        while True:
            # Read header (5 bytes).
            header = in_file.read(5)
            if not header or len(header) < 5:
                # End of file or incomplete data.
                break

            valid, data_len = parse_log_header(header)
            if not valid:
                print(
                    "Warning: Invalid log header found at offset "
                    f"{in_file.tell() - 5}, trying to continue..."
                )

                # Try to find the next possible log header. Go back 4 bytes,
                # keeping the last byte for the next check.
                in_file.seek(-4, os.SEEK_CUR)
                continue

            # Read the data portion of the single log entry.
            log_data = in_file.read(data_len)
            if len(log_data) < data_len:
                print(
                    f"Warning: Incomplete log data, expected {data_len} bytes, "
                    f"got {len(log_data)} bytes"
                )

            if log_data:
                # Create a new decryptor for each log entry.
                decryptor = create_decryptor(key, nonce)

                # Decrypt this log entry.
                decrypted_data = decryptor.update(log_data)

                # Ensure all data is processed.
                final_data = decryptor.finalize()

                if final_data:
                    decrypted_data += final_data

                out_file.write(decrypted_data)
                total_bytes_decrypted += len(decrypted_data)
                log_count += 1

    print(
        f"Decryption completed: Processed {log_count} log entries, "
        f"decrypted {total_bytes_decrypted} bytes of data."
    )


def main() -> None:
    """Parse command line arguments and execute decryption operation"""
    parser = argparse.ArgumentParser(description="Log File Decryption Tool")
    parser.add_argument(
        "--input",
        "-i",
        required=True,
        help="Path to the source file to decrypt",
    )
    parser.add_argument(
        "--key", "-k", required=True, help="AES key (16-byte string)"
    )
    parser.add_argument(
        "--nonce",
        "-n",
        required=True,
        help="Nonce for CTR mode (16 bytes)",
    )
    parser.add_argument(
        "--output",
        "-o",
        required=True,
        help="Path to the destination file after decryption",
    )

    args = parser.parse_args()

    # Check if source file exists.
    if not os.path.exists(args.input):
        print(f"Error: Source file '{args.input}' does not exist")
        return

    # Ensure destination file directory exists.
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)

    try:
        decrypt_log_file(args.input, args.output, args.key, args.nonce)
    except Exception as e:
        print(f"Error during decryption: {e}")


if __name__ == "__main__":
    main()
