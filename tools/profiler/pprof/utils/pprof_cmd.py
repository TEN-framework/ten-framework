import shlex


def convert_heap_to_text_cmd(heapFile, textFile):
    # run cmd: go tool pprof -text ${heapFile} > ${textFile}
    return (
        f"go tool pprof -text {shlex.quote(heapFile)}"
        f" > {shlex.quote(textFile)}"
    )


def show_heap_in_browser_cmd(port, heapFile):
    # run cmd: go tool pprof -http=:${port} ${heapFile}
    return (
        f"go tool pprof -http=:{shlex.quote(str(port))} {shlex.quote(heapFile)}"
    )
