# Getting the 3D matrix
`ImgTo3D.py` generates a 3D matrix for the monochromatic display, from a provided 2D graphics - image. As the output, `.c` and `.h` files containing the 3D matrix, are already included in the project structure, to `CMakeList.txt` and `.cpp` files. The output arrays are named `image_to_3d_matrix_custom` and `image_to_bmp_array_custom` by default. The converted image is saved for a preview.

The script can be called by following commands:

    python ImgTo3D.py image_path image_width image_height 

* `ImgTo3D.py` - absolute path to the python file
* `image_path` - absolute or relative path to the image, to be converted
* `image_width` - resulting width of the converted image (in pixels)
* `image_height` - resulting height of the converted image (in pixels)

The python script converts the provided image first to a monochromatic image, then resizes the image according to the provided dimensions, then converts the image to a 3D matrix and finally saves the 3D into a `.c` file.

    python ImgTo3D.py --help

Run for help - prints hints, how to run the file and information about the arguments.

### Optional arguments
    python ImgTo3D.py image_path image_width image_height z_dist invert bmp

* `z_dist` puts a distance (in pixels) into the Z direction of each matrix row - each 3D point. 0 is used otherwise
* `invert` inverts black and white in the converted monochromatic image, causing inverting pixels that will be lit on the display
* `bmp` includes a BMP form of the converted image into the output `.c` and `.h` files

The optional arguments can be added in any sequence.

# Example

To get example results, in the example folder, run the script from the example folder like so:

    /example $ python ../ImgTo3D.py cpu_logo.png 64 64 invert bmp

To get the results already integrated into the project, run the script from any other location:

    /any_location $ python abs_path/ImgTo3D.py cpu_logo.png 64 64 bmp invert

Converts `cpu_logo.png` image into a 3D matrix with dimensions of `64x64` pixels, with inverted colors and the BMP is included into the output file.

## Output files

python script produces the following output:

* `converted_image/cpu_logo_invert_conv.png` image after the conversion (monochromatic and resized image)
* `image_to_3d_matrix.c` c file with 3D and BMP matrices in `const` arrays
* `image_to_3d_matrix.h` h file with 3D and BMP arrays declarations in `extern const`

The output `.c` and `.h` files are based on template files `template_img_to_3d.c` and `template_img_to_3d.h`. From which, only the variables declarations are changed according to the converted image.

The expected output from the python script is the following:

    Image conversion done!
    Converted image cpu_logo_invert_conv.png
    Converted image location ${workspaceFolder}/graphics/img_to_3d_matrix/example/converted_image

    Output files generation done!
    Output files image_to_3d_matrix.c image_to_3d_matrix.h
    Output files location ${workspaceFolder}/graphics/img_to_3d_matrix/example

    All done!

## Image conversion output example

<div align="center">
  <table>
    <tr>
      <td style="height: 15px;"> 
          <p align = "center">
              Converted image (64x64)
          </p>
       </td>
      <td style="height: 15px;">
          <p align = "center">
              Inverted colors (64x64)
          </p>
      </td>
    </tr>
    <tr>
      <td>
          <div align="center">
              <img src= "converted_image/cpu_logo_conv.png"> 
          </div>
      </td>
      <td>
          <div align="center">
              <img src= "converted_image/cpu_logo_invert_conv.png"> 
          </div>
      </td>
    </tr>
    <tr>
      <td colspan="2" style="height: 15px;">
          <p align = "center">
              Original image (256x256)
          </p>
      </td>
    </tr>
    <tr>
      <td colspan="2">
          <div align="center">
              <img src= "image/cpu_logo.png" style="width:50%"> 
          </div>
      </td>
    </tr>
  </table>
</div>