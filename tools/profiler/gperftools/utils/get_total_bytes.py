"""get the total bytes from the pprof text file."""


def get_total_bytes_from_text(pprof_file, output_unit):
    with open(pprof_file, "r", encoding="utf-8") as f:
        # the file content is like:

        # Total: 19.7 MB
        # 16.0  81.3%  81.3%     16.0  81.3% lws_zalloc
        # 2.9  14.6%  95.9%      3.3  16.7% setAgoraStreamChannelParameters

        # or
        # Total: 24411038 samples
        # 16792864  68.8%  68.8% 16792864  68.8% lws_zalloc
        # 6106459  25.0%  93.8%  6925684  28.4% setAgoraStreamChannelParameters
        # 676509   2.8%  96.6%  4902452  20.1% createAgoraMediaComponentFactory

        # we need to get the first line
        first_line = f.readline()

        # remove the \n
        first_line = first_line.replace("\n", "")

        first_line_split = first_line.split(" ")
        if len(first_line_split) != 3:
            raise Exception(
                "first_line_split length is not 3, first_line_split:"
                f" {first_line_split}"
            )

        # get the total bytes
        unit = first_line_split[2]
        total_bytes = float(first_line_split[1])
        if unit == "MB":
            total_bytes = total_bytes * 1024 * 1024
        elif unit == "KB":
            total_bytes = total_bytes * 1024
        elif unit == "B":
            total_bytes = total_bytes
        else:
            total_bytes = total_bytes

        if output_unit == "MB":
            return total_bytes / (1024 * 1024)
        elif output_unit == "KB":
            return total_bytes / 1024
        elif output_unit == "B":
            return total_bytes
        else:
            return total_bytes
