/*
////////////////////////////////////////////////////////////////////////////////////////
//
//  This file is part of SPNV, a simple panorama viewer for Hugin panoramas.
//  Copyright (C) 2022, 2025 M. Frohne
//
//  SPNV is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as published
//  by the Free Software Foundation, either version 3 of the License,
//  or (at your option) any later version.
//
//  SPNV is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//  See the GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with SPNV. If not, see <https://www.gnu.org/licenses/>.
//
////////////////////////////////////////////////////////////////////////////////////////
*/

#ifndef SPNV_SCENEMETADATA_H
#define SPNV_SCENEMETADATA_H

#include <SFML/System/Vector2.hpp>

#include <cstdint>
#include <string>

/*!
 * \brief Handling of panorama scene-specific meta data for proper display on screen.
 *
 * Gathers meta information about a panorama scene that can be used to display the scene on a screen in correct perspective,
 * even in case of an unevenly cropped panorama picture (which particularly enables to identify (and center) the horizon line).
 *
 * The required information includes projection type, size (in pixels) and field of view of the uncropped scene
 * (assumed to be symmetric about the horizon line!) and the crop rectangle used to crop the output picture.
 * For more details see SceneMetaData(PanoramaProjection, sf::Vector2i, sf::Vector2f, sf::Vector2i, sf::Vector2i).
 * The information can be either parsed from a corresponding Hugin project file (see loadFromPTOFile()) or loaded from
 * (and saved to) a custom file format, which is here simply called "PNV file" (see loadFromPNVFile() and saveToPNVFile()).
 */
class SceneMetaData
{
public:
    /*!
     * \brief Panorama projection type.
     *
     * Type of projection used to map the reconstructed panorama sphere to the final output picture.
     */
    enum class PanoramaProjection : std::uint8_t
    {
        CentralCylindrical,     ///< The "central cylindrical" projection.
        Equirectangular         ///< The "equirectangular" projection.
    };

public:
    SceneMetaData();                                                                                ///< \brief Construct meta data for an
                                                                                                    ///  "empty" panorama scene (for loading
                                                                                                    ///  useful values from file later).
    SceneMetaData(PanoramaProjection pProjectionType, sf::Vector2i pUncroppedSize,
                  sf::Vector2f pUncroppedFOV, sf::Vector2i pCropPosTL, sf::Vector2i pCropPosBR);    ///< \brief Construct panorama scene
                                                                                                    ///  meta data from already known values.
    //
    PanoramaProjection getProjectionType() const;   ///< Get the panorama projection type.
    sf::Vector2i getUncroppedSize() const;          ///< Get the size of the uncropped panorama picture.
    sf::Vector2f getUncroppedFOV() const;           ///< Get the field of view of the uncropped panorama picture.
    sf::Vector2i getCropPosTL() const;              ///< Get the position of the crop rectangle's top left corner.
    sf::Vector2i getCropPosBR() const;              ///< Get the position of the crop rectangle's bottom right corner.
    //
    bool loadFromPTOFile(const std::string& pFileName);     ///< Load the meta data from a Hugin project file.
    bool loadFromPNVFile(const std::string& pFileName);     ///< Load the meta data from a "PNV file".
    bool saveToPNVFile(const std::string& pFileName) const; ///< Write the meta data to a "PNV file".

private:
    PanoramaProjection projectionType;  //Panorama sphere to output picture projection type
    sf::Vector2i uncroppedSize;         //Size of panorama picture if it was uncropped (size symmetric about horizon line)
    sf::Vector2f uncroppedFOV;          //FOV of panorama picture if it was uncropped (FOV symmetric about horizon line)
    sf::Vector2i cropPosTL;             //Position in uncropped picture corresponding to top left corner of cropped picture
    sf::Vector2i cropPosBR;             //Position in uncropped picture corresponding to bottom right corner of cropped picture
};

#endif // SPNV_SCENEMETADATA_H
