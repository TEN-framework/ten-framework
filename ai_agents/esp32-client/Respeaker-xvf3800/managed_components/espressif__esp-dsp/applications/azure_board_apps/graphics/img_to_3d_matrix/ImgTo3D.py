# SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: CC0-1.0

import sys
import os
import subprocess

PIL_INSTALLED = True
PYTHON_FILE_NAME = os.path.basename(__file__)           # name of the Python file itself
OUTPUT_FILE_NAME = "image_to_3d_matrix"                 # generated output .c and .h file names
OUTPUT_3D_MATRIX_NAME = "image_to_3d_matrix_custom"     # generated output 3D matrix in the .c and .h file
OUTPUT_BMP_ARRAY_NAME = "image_to_bmp_array_custom"     # generated output BMP array in the .c and .h file
TEMPLATE_FILE_NAME = "template_img_to_3d"               # name of the .c and .h template files

ESP_IOT_AZURE_DISPLAY_WIDTH = 128                       # ESP_IOT_AZURE board display width
ESP_IOT_AZURE_DISPLAY_HEIGHT = 64                       # ESP_IOT_AZURE board display height

try:
    from PIL import Image, ImageOps
except ImportError as e:
    PIL_INSTALLED = False
    print(e)

if not PIL_INSTALLED:
    print("Installing Pillow package by running command -m pip install --upgrade Pillow")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "--upgrade", "Pillow"])
    except subprocess.CalledProcessError as e:
        print(e)


class Image3DMatrix:
    """ class for 3d image matrix """
    def __init__(self):
        self.terminal_args = sys.argv   # get command line arguments
        self.img_path = None            # path to the image
        self.img_name = None            # image name
        self.img_ext = None             # image extension
        self.img_width = None           # image target width
        self.img_height = None          # image target height
        self.img_invert = False         # invert monochromatic
        self.img_include_bmp = False    # include BMP form of the image
        self.converted_img_dir = None   # location to save the converted image
        self.result_dir = None          # location to save the .c and .h files
        self.templates_dir = None       # location where template files are saved
        self.z_dist = 0                 # Z distance of the 3D matrix
        self.get_termianl_input()

    def get_optional_args(self):
        """ take optional arguments from the terminal, if there are any """

        # take all the optional arguments
        if len(self.terminal_args) > 4:
            opt_args = self.terminal_args[4:len(self.terminal_args)]

            # check the optional arguments "invert" "bmp" and z_dist
            for arg in opt_args:
                if arg.isdecimal():
                    self.z_dist = int(arg)
                elif arg.lower() == "invert":
                    self.img_invert = True
                elif arg.lower() == "bmp":
                    self.img_include_bmp = True

    @staticmethod
    def check_if_img_file_exists(path: str):
        """ check, if provided image file path from terminal input is valid and an image file exits """

        # check if a file exists on the provided absolute path
        if os.path.isabs(path):
            if not os.path.isfile(path):
                print(f"Provided file {path} does not exist")
                sys.exit()

        # check if a file exists on the provided relative path
        else:
            absolute_path = os.path.join(os.getcwd(), path)
            if not os.path.isfile(absolute_path):
                print(f"Provided file {path} does not exist")
                sys.exit()
            else:
                return absolute_path

        return path

    @staticmethod
    def check_if_templates_exist():
        """ Check if template .c and .h files exist """

        # check if templates folder exists
        templates_dir = os.path.join(os.getcwd(), "templates")
        if not os.path.exists(templates_dir):
            print(f"templates folder does not exist at location: {templates_dir}")
            sys.exit()

        # check if template .c file exists
        template_c_file = os.path.join(templates_dir, f"{TEMPLATE_FILE_NAME}.c")
        if not os.path.isfile(template_c_file):
            print(f".c template file does not exist at location {template_c_file}")
            sys.exit()

        # check if template .h file exists
        template_h_file = os.path.join(templates_dir, f"{TEMPLATE_FILE_NAME}.h")
        if not os.path.isfile(template_h_file):
            print(f".h template file does not exist at location {template_h_file}")
            sys.exit()

        return templates_dir

    def get_img_name_extension(self):
        """ extract image file name and image file extension """

        img_file = os.path.basename(self.img_path)      # image file with extension
        self.img_name = os.path.splitext(img_file)[0]   # extract file name

        # Extract the file extension, to CAPS and remove the first character being dot
        self.img_ext = os.path.splitext(img_file)[1].upper()[1:]

    @staticmethod
    def check_if_is_img(path: str, filename: str):
        """ check if the provided file is an image and if its extension is supported """

        # check if a file (which exits) is an image file 
        if not path.lower().endswith(('.png', '.jpg', '.jpeg', '.tiff', '.bmp', '.gif')):
            print(f"provided file {filename} is not a supported image file")
            sys.exit()

    @staticmethod
    def check_pixels(height: str, width: str):
        """ check if the provided result image height and width are decimal numbers """

        # check if provided resulting image width and height are decimal numbers
        if not height.isdecimal() and width.isdecimal():
            print(f"Provided image target width {width} and/or height {height} not valid")
            sys.exit()

    @staticmethod
    def check_if_pixels_in_range(height: int, width: int):
        """ check if the provided result image height and width are in a valid range """

        # check if the height and width are in the display's range
        if (height > ESP_IOT_AZURE_DISPLAY_HEIGHT + 1 or width > ESP_IOT_AZURE_DISPLAY_WIDTH + 1 or
                height < 1 or width < 1):
            print(f"Provided image target width {width} and/or height {height} \
                  is out of range of the display's dimensions ({ESP_IOT_AZURE_DISPLAY_WIDTH}x{ESP_IOT_AZURE_DISPLAY_HEIGHT})")
            sys.exit()

        if height % 2 or width % 2:
            print(f"Provided image target width {width} and height {height} must be even")
            sys.exit()

    def create_paths(self):
        """ create paths for the result files """

        # if running from the example folder, leave the results there
        # if running from anywhere else, change cwd to dir, where the python is located
        cwd = os.getcwd()                       # current working directory
        abspath = os.path.abspath(__file__)     # get the absolute path of the python script
        python_dir = os.path.dirname(abspath)   # get the absolute path of the folder, in which the python script is located

        if os.path.join("img_to_3d_matrix", "example") not in cwd:
            os.chdir(python_dir)                # go to a directory where the python is located

        # create an absolute path for the folder in which the image after conversion will be saved
        self.converted_img_dir = os.path.join(os.getcwd(), "converted_image")
        if not os.path.exists(self.converted_img_dir):
            # if the path does not exist, create it
            os.makedirs(self.converted_img_dir)

        # create an absolute path for the folder, to which the resulting .c and .h files will be saved
        self.result_dir = os.path.join(os.path.dirname(os.getcwd()), "3d_matrix", "3d_matrix_data")
        if not os.path.exists(self.result_dir):
            # if the path does not exist, use the current path instead
            self.result_dir = os.getcwd()

        if os.path.join("img_to_3d_matrix", "example") in cwd:
            os.chdir(python_dir)                # go to a directory where the python is located

    def get_termianl_input(self):
        """ get input parameters from terminal """

        if ((len(self.terminal_args) == 2) and (str(self.terminal_args[1]) == "--help")):
            query = '\n'.join([
                f"\npython {PYTHON_FILE_NAME} -image_path -image_width -image_height -z_distance -invert -bmp\n\n"
                "-image_path          absolute or relative path to an image to be processed",
                "-image_width         width (in pixels) of the resulting 3D image",
                "-image_height        height (in pixels) of the resulting 3D image",
                "-z_distance          (optional) Z distance (in pixels) of the resulting 3D image, leave blank otherwise",
                "-invert              (optional) to invert colors, leave blank otherwise",
                "-bmp                 (optional) to include BMP from of the provided image, leave blank otherwise"])
            print(query)
            sys.exit()

        elif len(self.terminal_args) < 4:
            print(f"Too few arguments run following command for help\npython {PYTHON_FILE_NAME} --help")
            sys.exit()

        else:
            self.img_path = self.terminal_args[1]
            self.img_width = self.terminal_args[2]
            self.img_height = self.terminal_args[3]

            self.get_optional_args()
            self.img_path = self.check_if_img_file_exists(self.img_path)
            self.get_img_name_extension()
            self.check_if_is_img(self.img_path, self.img_name)
            self.check_pixels(self.img_height, self.img_width)
            self.img_height = int(self.img_height)
            self.img_width = int(self.img_width)
            self.check_if_pixels_in_range(self.img_height, self.img_width)
            self.create_paths()
            self.templates_dir = self.check_if_templates_exist()


def fromat_comas_spaces(element_index: int, array_len: int, elements_per_line: int, c_file):
    """ Formating of the spaces, commas and new lines for arrays in the .c file """

    # Dont put comma after the last array member
    if element_index != array_len:
        c_file.write(",")

    # new line after each elements_per_line
    if not element_index % elements_per_line:
        c_file.write("\n")

        # start a new line with tab
        if element_index != array_len:
            c_file.write("    ")

    # put space after each coma
    # except after the last array member and before a new line
    else:
        if element_index != array_len:
            c_file.write(" ")


def ouptput_files_gen(img3d: Image3DMatrix, bmp_arr: list, matrix_arr: list):
    """ write 3d matrix array and optional BMP array to .c and .h files """

    # generate a c file and save it to the main folder (if exists in the file structure)
    output_c_file_name = f"{OUTPUT_FILE_NAME}.c"
    with open(os.path.join(img3d.templates_dir, f"{TEMPLATE_FILE_NAME}.c"), "r", encoding="utf8") as c_template_file:
        with open(os.path.join(img3d.result_dir, output_c_file_name), "w+", encoding="utf8") as c_file:

            c_file.write(f"// File generated by {PYTHON_FILE_NAME}\n")  # write first line to c_file
            c_file.write(f"// Image file converted to 3D matrix {img3d.img_name}.{img3d.img_ext.lower()}\n")
            next(c_template_file)
            next(c_template_file)                                       # skip first 2 lines from c_template_file
            for line in c_template_file:

                if "const" in line:
                    # include BMP form if requested
                    if img3d.img_include_bmp:
                        c_file.write(f"const uint8_t {OUTPUT_BMP_ARRAY_NAME}[{len(bmp_arr)}] = {{\n\n    ")
                        for index, bmp_byte in enumerate(bmp_arr, start=1):
                            c_file.write(f"0x{bmp_byte:02x}")
                            fromat_comas_spaces(index, len(bmp_arr), 16, c_file)
                        c_file.write("\n};\n\n")

                    # write 3d matrix form
                    c_file.write(f"const float {OUTPUT_3D_MATRIX_NAME}[{len(matrix_arr)}][4] = {{\n\n    ")
                    for index, line in enumerate(matrix_arr, start=1):
                        line_joined = ', '.join(map(str, line))
                        c_file.write(f"{{{line_joined}}}")
                        fromat_comas_spaces(index, len(matrix_arr), 6, c_file)
                    c_file.write("\n};\n")
                    next(c_template_file)
                    continue

                # copy line from c_template_file to c_file
                c_file.write(line)

            c_file.close()
        c_template_file.close()

    # generate an h file and save it to the main folder (if exists in the file structure)
    output_h_file_name = f"{OUTPUT_FILE_NAME}.h"
    with open(os.path.join(img3d.templates_dir, f"{TEMPLATE_FILE_NAME}.h"), "r", encoding="utf8") as h_template_file:
        with open(os.path.join(img3d.result_dir, output_h_file_name), "w+", encoding="utf8") as h_file:

            h_file.write(f"// File generated by {PYTHON_FILE_NAME}\n")  # write first line to h_file
            h_file.write(f"// Image file converted to 3D matrix: {img3d.img_name}.{img3d.img_ext.lower()}\n")
            next(h_template_file)
            next(h_template_file)                                       # skip first 2 lines from h_template_file
            for line in h_template_file:

                if "extern const" in line:
                    # include BMP header if requested
                    if img3d.img_include_bmp:
                        h_file.write(f"extern const uint8_t {OUTPUT_BMP_ARRAY_NAME}[{len(bmp_arr)}];\n")

                    # include 3d matrix header
                    h_file.write(f"extern const float {OUTPUT_3D_MATRIX_NAME}[{len(matrix_arr)}][4];\n")
                    next(h_template_file)
                    continue

                h_file.write(line)

        h_file.close()
    h_template_file.close()

    query = '\n'.join([
        "\nOutput files generation done!",
        f"Output files {output_c_file_name} {output_h_file_name}",
        f"Output files location {img3d.result_dir}"])
    print(query)


def create_matrix(img3d: Image3DMatrix, pixels: list, bmp_img_array: list, matrix_img_array: list):
    """ Create a 3D matrix and optional BMP form of the converted image """

    multiplier = pow(2, 7)
    byte = 0

    # convert pixels to both, bmp and 3d from
    for index, pixel in enumerate(pixels, start=1):
        if pixel:
            byte += multiplier
            y_coord = int((index - 1) / img3d.img_width)
            x_coord = (index - 1) - (y_coord * img3d.img_width) - (img3d.img_width / 2)
            y_coord = y_coord - (img3d.img_height / 2)
            matrix_img_array.append((x_coord, y_coord, img3d.z_dist, 1))

        multiplier >>= 1

        if not index % 8:
            multiplier = pow(2, 7)
            bmp_img_array.append(byte)
            byte = 0


def convert_img(img3d: Image3DMatrix):
    """ image conversion to list of monochromatic pixels """

    # open an image and convert the image to monochromatic
    img = Image.open(img3d.img_path)
    img = img.convert(mode="1", dither=Image.NONE)

    # if requested, invert black and white
    if img3d.img_invert:
        img = ImageOps.invert(img)

    # resize the image and get the width and height of the resized image, to check
    img = img.resize((int(img3d.img_width), int(img3d.img_height)), resample=Image.NEAREST)
    res_width, res_height = img.size

    # get image pixels as a list, after converting the image to monochromatic - pixels are either 0 or 255
    pixels = list(img.getdata())

    # check if the resized image has the same dimensions as the required dimensions
    if (res_width, res_height) != (img3d.img_width, img3d.img_height):
        query = '\n'.join([
            "Image conversion was not successful!",
            f"Required width and height: {img3d.img_width} {img3d.img_height}",
            f"Real width and height:     {res_width} {res_height}"])
        print(query)
        sys.exit()
    else:
        # if converted correctly, save the converted image
        if img3d.img_invert:
            invert = "_invert_conv."
        else:
            invert = "_conv."

        converted_img_name = ''.join([img3d.img_name, invert, img3d.img_ext.lower()])
        converted_img_location = os.path.join(img3d.converted_img_dir, converted_img_name)
        img.save(converted_img_location, format=img3d.img_ext, subsampling=0, quality=100)
        query = '\n'.join([
            "Image conversion done!",
            f"Converted image {converted_img_name}",
            f"Converted image location {os.path.join(img3d.converted_img_dir)}"])
        print(query)
        return pixels


def main():
    """ main function of the image_to_3d_matrix """

    img3d = Image3DMatrix()
    pixels = convert_img(img3d)

    bmp_img_array = list()
    matrix_img_array = list()
    create_matrix(img3d, pixels, bmp_img_array, matrix_img_array)
    ouptput_files_gen(img3d, bmp_img_array, matrix_img_array)
    print("\nAll done!")


if __name__ == '__main__':
    main()
