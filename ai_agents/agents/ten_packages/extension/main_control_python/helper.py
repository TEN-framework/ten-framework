#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

def is_punctuation(char):
    if char in [",", "，", ".", "。", "?", "？", "!", "！"]:
        return True
    return False


def parse_sentences(sentence_fragment, content):
    sentences = []
    current_sentence = sentence_fragment
    for char in content:
        current_sentence += char
        if is_punctuation(char):
            # Check if the current sentence contains non-punctuation characters
            stripped_sentence = current_sentence
            if any(c.isalnum() for c in stripped_sentence):
                sentences.append(stripped_sentence)
            current_sentence = ""  # Reset for the next sentence

    remain = current_sentence  # Any remaining characters form the incomplete sentence
    return sentences, remain