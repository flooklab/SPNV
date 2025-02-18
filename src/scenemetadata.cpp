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

#include "constants.h"
#include "scenemetadata.h"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

/*!
 * \brief Construct meta data for an "empty" panorama scene (for loading useful values from file later).
 *
 * Sets projection type to PanoramaProjection::CentralCylindrical and all sizes and positions to 0.
 * See also SceneMetaData(PanoramaProjection, sf::Vector2i, sf::Vector2f, sf::Vector2i, sf::Vector2i).
 *
 * Useful meta data can be loaded from file via loadFromPTOFile() or loadFromPNVFile().
 */
SceneMetaData::SceneMetaData() :
    SceneMetaData(PanoramaProjection::CentralCylindrical, {0, 0}, {0, 0}, {0, 0}, {0, 0})
{
}

/*!
 * \brief Construct panorama scene meta data from already known values.
 *
 * Sets the used projection type to \p pProjectionType. Note that reconstruction of the panorama sphere in Hugin first
 * yields a raw uncropped panorama picture that is centered around the horizon line and expands the same amount in both
 * north and south directions. This uncropped picture has the size (in pixels) \p pUncroppedSize and spans a total
 * field of view of {HFOV, VFOV} = \p pUncroppedFOV. Because the final panorama picture is a cropped version of this
 * uncropped panorama, one further needs information about the crop rectangle, which is given in terms of its top left
 * (\p pCropPosTL) and bottom right (\p pCropPosBR) corner positions (relative to the uncropped coordinate system).
 *
 * \param pProjectionType Panorama projection type.
 * \param pUncroppedSize Size of (Hugin's) uncropped panorama picture reconstruction.
 * \param pUncroppedFOV Field of view covered by (Hugin's) uncropped panorama picture reconstruction.
 * \param pCropPosTL Position in uncropped panorama picture reconstruction matching top left corner of actual panorama picture.
 * \param pCropPosBR Position in uncropped panorama picture reconstruction matching bottom right corner of actual panorama picture.
 */
SceneMetaData::SceneMetaData(const PanoramaProjection pProjectionType, const sf:: Vector2i pUncroppedSize,
                             const sf::Vector2f pUncroppedFOV, const sf::Vector2i pCropPosTL, const sf::Vector2i pCropPosBR) :
    projectionType(pProjectionType),
    uncroppedSize(pUncroppedSize),
    uncroppedFOV(pUncroppedFOV),
    cropPosTL(pCropPosTL),
    cropPosBR(pCropPosBR)
{
}

//Public

/*!
 * \brief Get the panorama projection type.
 *
 * \return Panorama projection type.
 */
SceneMetaData::PanoramaProjection SceneMetaData::getProjectionType() const
{
    return projectionType;
}

/*!
 * \brief Get the size of the uncropped panorama picture.
 *
 * \return Size of uncropped panorama picture reconstruction.
 */
sf::Vector2i SceneMetaData::getUncroppedSize() const
{
    return uncroppedSize;
}

/*!
 * \brief Get the field of view of the uncropped panorama picture.
 *
 * \return Field of view covered by uncropped panorama picture reconstruction.
 */
sf::Vector2f SceneMetaData::getUncroppedFOV() const
{
    return uncroppedFOV;
}

/*!
 * \brief Get the position of the crop rectangle's top left corner.
 *
 * \return Top left panorama picture crop position.
 */
sf::Vector2i SceneMetaData::getCropPosTL() const
{
    return cropPosTL;
}

/*!
 * \brief Get the position of the crop rectangle's bottom right corner.
 *
 * \return Bottom right panorama picture crop position.
 */
sf::Vector2i SceneMetaData::getCropPosBR() const
{
    return cropPosBR;
}

//

/*!
 * \brief Load the meta data from a Hugin project file.
 *
 * Parses the Hugin project file \p pFileName and extracts the required information from it
 * (see SceneMetaData(PanoramaProjection, sf::Vector2i, sf::Vector2f, sf::Vector2i, sf::Vector2i)).
 *
 * Note that the vertical field of view is not saved in the project files and will hence be automatically
 * calculated from the other available information (calculation depends on the used projection type).
 *
 * \param pFileName File name of the project file.
 * \return If successful.
 */
bool SceneMetaData::loadFromPTOFile(const std::string& pFileName)
{
    //Information to be parsed from project file
    PanoramaProjection tProjectionType;
    sf::Vector2i tUncroppedSize;
    sf::Vector2f tUncroppedFOV;
    sf::Vector2i tCropPosTL;
    sf::Vector2i tCropPosBR;

    try
    {
        std::ifstream file;
        file.exceptions(std::ios_base::badbit | std::ios_base::failbit);
        file.open(pFileName, std::ios_base::in);

        std::string line;

        //Read first three lines of file (third line contains required information), check if file is Hugin project file

        std::getline(file, line);
        if (line != "# hugin project file")
            throw std::runtime_error("Not a Hugin project file!");
        std::getline(file, line);
        if (line != "#hugin_ptoversion 2")
            throw std::runtime_error("Project file is of unsupported Hugin version!");
        std::getline(file, line);

        file.close();

        //Check for correct line
        if (line.substr(0, 3) != "p f")
            throw std::runtime_error("Unexpected project file content!");

        //Decompose line into substrings containing required information

        std::string substrProj, substrH, substrW, substrHFOV, substrCrop;

        std::istringstream sstrm(line);

        std::getline(sstrm, line, ' ');
        std::getline(sstrm, substrProj, ' ');
        std::getline(sstrm, substrW, ' ');
        std::getline(sstrm, substrH, ' ');
        std::getline(sstrm, substrHFOV, ' ');
        std::getline(sstrm, line, ' ');
        std::getline(sstrm, line, ' ');
        std::getline(sstrm, line, ' ');
        std::getline(sstrm, line, ' ');
        std::getline(sstrm, substrCrop, ' ');

        //Split key chars from numeric data
        substrProj = substrProj.substr(1);
        substrH = substrH.substr(1);
        substrW = substrW.substr(1);
        substrHFOV = substrHFOV.substr(1);
        substrCrop = substrCrop.substr(1);

        //Special treatment for crop information (is comma separated list)

        std::string substrCropL, substrCropR, substrCropT, substrCropB;

        sstrm = std::istringstream(substrCrop);

        std::getline(sstrm, substrCropL, ',');
        std::getline(sstrm, substrCropR, ',');
        std::getline(sstrm, substrCropT, ',');
        sstrm>>substrCropB;

        //Convert to actual numbers

        int proj, h, w, cropL, cropR, cropT, cropB;
        float hfov, vfov;

        proj = std::atoi(substrProj.c_str());
        h = std::atoi(substrH.c_str());
        w = std::atoi(substrW.c_str());
        hfov = std::atoi(substrHFOV.c_str()) * Constants::pi / 180.f;
        cropL = std::atoi(substrCropL.c_str());
        cropR = std::atoi(substrCropR.c_str());
        cropT = std::atoi(substrCropT.c_str());
        cropB = std::atoi(substrCropB.c_str());

        //Need to calculate VFOV from other numbers (depends on projection type)
        if (proj == 1)
        {
            tProjectionType = PanoramaProjection::CentralCylindrical;

            vfov = 2.f * std::atan(hfov * h / w / 2.f);
        }
        else if (proj == 2)
        {
            tProjectionType = PanoramaProjection::Equirectangular;

            vfov = hfov * h / w;
        }
        else
            throw std::runtime_error("Unsupported projection type!");

        tUncroppedSize = {w, h};
        tUncroppedFOV = {hfov, vfov};
        tCropPosTL = {cropL, cropT};
        tCropPosBR = {cropR, cropB};
    }
    catch (const std::ios_base::failure& exc)
    {
        std::cerr<<"ERROR: Could not open PTO file \"" + pFileName + "\"!"<<std::endl;
        std::cerr<<exc.what()<<std::endl;
        return false;
    }
    catch (const std::exception& exc)
    {
        std::cerr<<"ERROR: Could not parse PTO file \"" + pFileName + "\"!"<<std::endl;
        std::cerr<<exc.what()<<std::endl;
        return false;
    }

    projectionType = tProjectionType;
    uncroppedSize = tUncroppedSize;
    uncroppedFOV = tUncroppedFOV;
    cropPosTL = tCropPosTL;
    cropPosBR = tCropPosBR;

    return true;
}

/*!
 * \brief Load the meta data from a "PNV file".
 *
 * Instead of by parsing a Hugin project file (loadFromPTOFile()), the meta data can
 * also be read from a file of custom format, the "PNV file". Such a file can be
 * generated via saveToPNVFile() (see there for information about the file format).
 *
 * \param pFileName File name of the "PNV file".
 * \return If successful.
 */
bool SceneMetaData::loadFromPNVFile(const std::string& pFileName)
{
    //Information to be read from file
    PanoramaProjection tProjectionType;
    sf::Vector2i tUncroppedSize;
    sf::Vector2f tUncroppedFOV;
    sf::Vector2i tCropPosTL;
    sf::Vector2i tCropPosBR;

    try
    {
        std::ifstream file;
        file.exceptions(std::ios_base::badbit | std::ios_base::failbit);
        file.open(pFileName, std::ios_base::in);

        //Decompose first line into substrings containing required information

        std::string substrSig, substrProj, substrW, substrH, substrHFOV, substrVFOV,
                    substrCropL, substrCropT, substrCropR, substrCropB;

        //Check PNV file signature first
        std::getline(file, substrSig, ',');
        if (substrSig != "PanoramaViewerAuxiliaryFile")
            throw std::runtime_error("Not a PNV file!");

        std::getline(file, substrProj, ',');
        std::getline(file, substrW, ',');
        std::getline(file, substrH, ',');
        std::getline(file, substrHFOV, ',');
        std::getline(file, substrVFOV, ',');
        std::getline(file, substrCropL, ',');
        std::getline(file, substrCropT, ',');
        std::getline(file, substrCropR, ',');
        std::getline(file, substrCropB, ';');

        //Convert to required numbers

        int h, w, cropL, cropR, cropT, cropB;
        float hfov, vfov;

        h = std::atoi(substrH.c_str());
        w = std::atoi(substrW.c_str());
        hfov = std::atof(substrHFOV.c_str());
        vfov = std::atof(substrVFOV.c_str());
        cropL = std::atoi(substrCropL.c_str());
        cropR = std::atoi(substrCropR.c_str());
        cropT = std::atoi(substrCropT.c_str());
        cropB = std::atoi(substrCropB.c_str());

        //Determine used panorama projection type
        if (substrProj == "CYL")
            tProjectionType = PanoramaProjection::CentralCylindrical;
        else if (substrProj == "EQR")
            tProjectionType = PanoramaProjection::Equirectangular;
        else
            throw std::runtime_error("Unsupported projection type!");

        tUncroppedSize = {w, h};
        tUncroppedFOV = {hfov, vfov};
        tCropPosTL = {cropL, cropT};
        tCropPosBR = {cropR, cropB};
    }
    catch (const std::ios_base::failure& exc)
    {
        std::cerr<<"ERROR: Could not open file \"" + pFileName + "\"!"<<std::endl;
        std::cerr<<exc.what()<<std::endl;
        return false;
    }
    catch (const std::exception& exc)
    {
        std::cerr<<"ERROR: Could not parse file \"" + pFileName + "\"!"<<std::endl;
        std::cerr<<exc.what()<<std::endl;
        return false;
    }

    projectionType = tProjectionType;
    uncroppedSize = tUncroppedSize;
    uncroppedFOV = tUncroppedFOV;
    cropPosTL = tCropPosTL;
    cropPosBR = tCropPosBR;

    return true;
}

/*!
 * \brief Write the meta data to a "PNV file".
 *
 * Writes the meta data to a "PNV file" of the following custom format:
 *
 * "PanoramaViewerAuxiliaryFile," + PROJECTION_TYPE + "," + UNCROPPED_SIZE_X + "," + UNCROPPED_SIZE_Y + "," +
 * UNCROPPED_FOV_X + "," + UNCROPPED_FOV_Y + "," + CROP_POS_L + "," + CROP_POS_T + "," + CROP_POS_R + "," + CROP_POS_B + ";\n"
 *
 * The meta data from such a file can be loaded via loadFromPNVFile().
 *
 * \param pFileName File name of the "PNV file".
 * \return If successful.
 */
bool SceneMetaData::saveToPNVFile(const std::string& pFileName) const
{
    try
    {
        std::ofstream file;
        file.exceptions(std::ios_base::badbit | std::ios_base::failbit);
        file.open(pFileName, std::ios_base::out);

        //Do not truncate floating point numbers
        file<<std::fixed<<std::setprecision(std::numeric_limits<float>::digits10 + 1);

        //Start with file signature
        file<<"PanoramaViewerAuxiliaryFile"<<",";

        //Write main file content

        if (projectionType == PanoramaProjection::CentralCylindrical)
            file<<"CYL"<<",";
        else if (projectionType == PanoramaProjection::Equirectangular)
            file<<"EQR"<<",";
        else
            file<<",";

        file<<uncroppedSize.x<<","<<uncroppedSize.y<<",";
        file<<uncroppedFOV.x<<","<<uncroppedFOV.y<<",";
        file<<cropPosTL.x<<","<<cropPosTL.y<<",";
        file<<cropPosBR.x<<","<<cropPosBR.y<<";\n";

        file.close();
    }
    catch (const std::ios_base::failure& exc)
    {
        std::cerr<<"ERROR: Could not write to file \"" + pFileName + "\"!"<<std::endl;
        std::cerr<<exc.what()<<std::endl;
        return false;
    }

    return true;
}
