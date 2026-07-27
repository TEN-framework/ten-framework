import shlex


def convert_heap_to_text_cmd(bin, heapFile, textFile):
    # run cmd: google-pprof --text bin heapFile > textFile
    return (
        f"google-pprof --text {shlex.quote(bin)} {shlex.quote(heapFile)}"
        f" > {shlex.quote(textFile)}"
    )


def convert_heap_to_raw_cmd(bin, heapFile, rawFile):
    # run cmd: google-pprof --raw bin heapFile > rawFile
    return (
        f"google-pprof --raw {shlex.quote(bin)} {shlex.quote(heapFile)}"
        f" > {shlex.quote(rawFile)}"
    )


def convert_raw_to_text_cmd(rawFile, textFile):
    # run cmd: google-pprof --text rawFile > textFile
    return (
        f"google-pprof --text {shlex.quote(rawFile)} > {shlex.quote(textFile)}"
    )


def compare_heaps_cmd(baseRawFile, rawFile, outputType, outputFile):
    # run cmd: google-pprof --base baseRawFile rawFile --<outputType> > outputFile
    return (
        f"google-pprof --base {shlex.quote(baseRawFile)}"
        f" {shlex.quote(rawFile)} --{shlex.quote(outputType)}"
        f" > {shlex.quote(outputFile)}"
    )
