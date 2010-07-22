/*
    Copyright 2008 Brain Research Institute, Melbourne, Australia

    Written by J-Donald Tournier, 27/06/08.

    This file is part of MRtrix.

    MRtrix is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MRtrix is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MRtrix.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "app.h"
#include "progressbar.h"
#include "image/voxel.h"
#include "image/axis.h"
#include "dataset/copy.h"
#include "dataset/extract.h"

using namespace MR; 

SET_VERSION_DEFAULT;
SET_AUTHOR (NULL);
SET_COPYRIGHT (NULL);

DESCRIPTION = {
  "perform conversion between different file types and optionally extract a subset of the input image.",
  "If used correctly, this program can be a very useful workhorse. In addition to converting images between different formats, it can be used to extract specific studies from a data set, extract a specific region of interest, flip the images, or to scale the intensity of the images.",
  NULL
};

ARGUMENTS = {
  Argument ("input", "input image", "the input image.").type_image_in (),
  Argument ("ouput", "output image", "the output image.").type_image_out (),
  Argument::End
};


OPTIONS = {
  Option ("coord", "select coordinates", "extract data only at the coordinates specified.", Optional | AllowMultiple)
    .append (Argument ("axis", "axis", "the axis of interest").type_integer (0, std::numeric_limits<int>::max(), 0))
    .append (Argument ("coord", "coordinates", "the coordinates of interest").type_sequence_int()),

  Option ("vox", "voxel size", "change the voxel dimensions.")
    .append (Argument ("sizes", "new dimensions", "A comma-separated list of values. Only those values specified will be changed. For example: 1,,3.5 will change the voxel size along the x & z axes, and leave the y-axis voxel size unchanged.")
        .type_sequence_float ()),

  Option ("datatype", "data type", "specify output image data type.")
    .append (Argument ("spec", "specifier", "the data type specifier.").type_choice (DataType::identifiers)),

  Option ("stride", "data strides", "specify the strides of the data in memory. The actual strides produced will depend on whether the output image format can support it.")
    .append (Argument ("spec", "specifier", "a comma-separated list of data strides.").type_string ()),

  Option ("prs", "DW gradient specified as PRS", "assume that the DW gradients are specified in the PRS frame (Siemens DICOM only)."),

  Option::End
};





EXECUTE {
  OptionList opt = get_options ("vox");
  std::vector<float> vox;
  if (opt.size()) 
    vox = parse_floats (opt[0][0].get_string());

  const Image::Header header_in = argument[0].get_image ();
  Image::Header header (header_in);
  header.reset_scaling();

  opt = get_options ("datatype");
  if (opt.size()) header.datatype().parse (DataType::identifiers[opt[0][0].get_int()]);

  for (size_t n = 0; n < vox.size(); n++) 
    if (std::isfinite (vox[n])) header.axes.vox(n) = vox[n];

  opt = get_options ("stride");
  if (opt.size()) {
    std::vector<int> ax = parse_ints (opt[0][0].get_string());
    size_t i;
    for (i = 0; i < std::min (ax.size(), header.ndim()); i++)
      header.axes.stride(i) = ax[i];
    for (; i < header.ndim(); i++)
      header.axes.stride(i) = 0;
  }


  opt = get_options ("prs");
  if (opt.size() && header.DW_scheme.rows() && header.DW_scheme.columns()) {
    for (size_t row = 0; row < header.DW_scheme.rows(); row++) {
      double tmp = header.DW_scheme(row, 0);
      header.DW_scheme(row, 0) = header.DW_scheme(row, 1);
      header.DW_scheme(row, 1) = tmp;
      header.DW_scheme(row, 2) = -header.DW_scheme(row, 2);
    }
  }

  std::vector<std::vector<int> > pos;

  opt = get_options ("coord");
  for (size_t n = 0; n < opt.size(); n++) {
    pos.resize (header.ndim());
    int axis = opt[n][0].get_int();
    if (pos[axis].size()) throw Exception ("\"coord\" option specified twice for axis " + str (axis));
    pos[axis] = parse_ints (opt[n][1].get_string());
  }

  assert (!header_in.is_complex());
  Image::Voxel<float> in (header_in);

  if (pos.size()) { 
    // extract specific coordinates:
    for (size_t n = 0; n < header_in.ndim(); n++) {
      if (pos[n].empty()) { 
        pos[n].resize (header_in.dim(n));
        for (size_t i = 0; i < pos[n].size(); i++) 
          pos[n][i] = i;
      }
    }
    DataSet::Extract<Image::Voxel<float> > extract (in, pos);
    for (size_t n = 0; n < extract.ndim(); ++n)
      header.axes.dim(n) = extract.dim(n);
    const Image::Header header_out = argument[1].get_image (header);
    Image::Voxel<float> out (header_out);
    DataSet::copy_with_progress (out, extract);
  }
  else { 
    // straight copy:
    const Image::Header header_out = argument[1].get_image (header);
    Image::Voxel<float> out (header_out);
    DataSet::copy_with_progress (out, in);
  }
}


