def convert_heap_to_text_cmd(bin, heapFile, textFile):
    # run cmd: google-pprof --text bin heapFile > textFile
    return "google-pprof --text " + bin + " " + heapFile + " > " + textFile


def convert_heap_to_raw_cmd(bin, heapFile, rawFile):
    # run cmd: google-pprof --raw bin heapFile > rawFile
    return "google-pprof --raw " + bin + " " + heapFile + " > " + rawFile


def convert_raw_to_text_cmd(rawFile, textFile):
    # run cmd: google-pprof --text rawFile > textFile
    return "google-pprof --text " + rawFile + " > " + textFile


def compare_heaps_cmd(baseRawFile, rawFile, outputType, outputFile):
    # run cmd: google-pprof --base baseRawFile rawFile --text > outputFile
    return (
        "google-pprof --base "
        + baseRawFile
        + " "
        + rawFile
        + " --"
        + outputType
        + " > "
        + outputFile
    )
