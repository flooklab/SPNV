# SPNV

SPNV is a simple viewer for panorama pictures that were created with [Hugin](https://hugin.sourceforge.io/).
It uses information from the Hugin project files to properly display the panorama scenes on screen
and to identify and optionally center the scenes' horizon lines. Pictures of two different panorama
projection types, either the central cylindrical or the equirectangular projection, can be displayed.  

The idea behind SPNV was that it shall properly display panorama pictures in the most typically
used projections, regardless of a possibly asymmetric crop, and that it shall be quite simple
(in terms of a basic functionality and a simple user interface as well as in terms of a well
structured and documented program code). SPNV shall also not use OpenGL directly and therefore
the SFML library is used to display the panorama scenes from explicitly computed two-dimensional
projections of the respectively current view. While this approach may indeed imply a suboptimal
performance the actual performance of SPNV appears to be sufficient for being usable.

## Usage

For each panorama picture there must be a corresponding `.pnv` file in the same directory for loading
the required meta data (same file name except for extension, i.e. `panorama-1.jpg` needs `panorama-1.pnv`).
The `.pnv` file can be created using the Hugin project file as input by running one of the following commands:

    SPNV picture-filename -p hugin-project-filename
    SPNV picture-filename --pto=hugin-project-filename

A panorama picture with an existing `.pnv` file can then be shown with SPNV either by
opening that picture with SPNV from your desktop environment or by directly calling

    SPNV picture-filename

from the command line.  

Note that the supported image formats are restricted to the ones supported by SFML,
which includes JPEG and PNG but does not include TIFF, for instance.

## Build

Building the project can be configured with [CMake](https://cmake.org/).  

Requires [SFML](https://github.com/SFML/SFML).  

Notes:
- Building the documentation requires [Doxygen](https://github.com/doxygen/doxygen) (optional).

## License information

Copyright (C) 2022 M. Frohne  

SPNV is free software: you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published
by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version.  

SPNV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Affero General Public License for more details.  

You should have received a copy of the GNU Affero General Public License
along with SPNV. If not, see <https://www.gnu.org/licenses/>.
