import matplotlib.pyplot as plt
import pandas as pd
import argparse


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.excel_file: str
        self.output_file: str
        self.title: str
        self.x: str
        self.y: str


def draw_line_chart(excel_file, title, x, y, output_file):
    df = pd.read_excel(excel_file)

    plt.plot(df[x], df[y], color="red", marker="o")
    plt.title(title)
    plt.xlabel(x)
    plt.ylabel(y)

    plt.savefig(output_file)


if __name__ == "__main__":
    argument_info = ArgumentInfo()

    parser = argparse.ArgumentParser()
    parser.add_argument("--excel-file", type=str, required=True)
    parser.add_argument("--output-file", type=str, required=True)
    parser.add_argument("--title", type=str, required=True)
    parser.add_argument("--x", type=str, required=True)
    parser.add_argument("--y", type=str, required=True)
    args = parser.parse_args(namespace=argument_info)

    draw_line_chart(
        argument_info.excel_file,
        argument_info.title,
        argument_info.x,
        argument_info.y,
        argument_info.output_file,
    )
