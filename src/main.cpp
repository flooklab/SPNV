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

#include "panoramawindow.h"
#include "scenemetadata.h"
#include "version.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/// \file
///
/// \brief The %main.cpp of SPNV.
///
/// \details See main() for information about what this does.

void printHelp();
std::string replaceWithPNVExtension(const std::string& pPictureFileName);
std::vector<std::pair<std::string, SceneMetaData>> getAllPanoramaPictures(const std::string& pRefPicFileName);

/*!
 * \brief Print usage information.
 *
 * Prints a description of the program call and its command line options to stderr.
 */
void printHelp()
{
    std::string helpString = std::string(Version::programName) + " " + Version::toString() + "\n\n";

    helpString.append("USAGE:\n " + std::string(Version::programName));

    helpString.append(" [-h | --help]");
    helpString.append(" [-f | --fullscreen]");
    helpString.append(" PANORAMA-PICTURE");
    helpString.append(" [--pto=HUGIN-FILE | -p HUGIN-FILE]");

    helpString.append("\n\nDESCRIPTION:\n");
    helpString.append(" If no options are present, displays the panorama scene in PANORAMA-PICTURE using information from "
                      "previously saved \"PNV\" file (see OPTIONS)\n");

    helpString.append("\nOPTIONS:\n");

    helpString.append(" -h, --help\n        Print a description of the command line options and exit.\n\n");
    helpString.append(" -f, --fullscreen\n        Start in fullscreen mode.\n\n");
    helpString.append(" -p, --pto=HUGIN-FILE\n        Extract information from Hugin project needed to properly display "
                      "PANORAMA-PICTURE. Save this information to a \"PNV\" file (same basename as PANORAMA-PICTURE) and exit.\n");

    std::cerr<<helpString;
}

/*!
 * \brief Generate a PNV file name corresponding to a picture file name.
 *
 * Replaces the extension of \p pPictureFileName with ".pnv".
 *
 * \return PNV file name corresponding to picture file name \p pPictureFileName.
 *
 * \throws std::runtime_error On failure.
 */
std::string replaceWithPNVExtension(const std::string& pPictureFileName)
{
    try
    {
        std::filesystem::path tPath(pPictureFileName);
        tPath.replace_extension(".pnv");
        return tPath.string();
    }
    catch (const std::exception& exc)
    {
        throw std::runtime_error(exc.what());
    }
}

/*!
 * \brief Determine all panorama picture and meta data pairs in a directory.
 *
 * Searches all files in the directory of the reference picture \p pRefPicFileName (including the reference picture itself)
 * that have the same extension as \p pRefPicFileName, tries to find their matching PNV files (see replaceWithPNVExtension())
 * and to load the panorama scene meta data from them. A sorted list of those picture file names that have valid
 * matching meta data together with that meta data is returned. Returns an empty list if any error occurs.
 *
 * \param pRefPicFileName File name of a panorama picture to determine the search directory and picture file extension.
 * \return Pairs of picture file names and corresponding meta data loaded from corresponding PNV files.
 */
std::vector<std::pair<std::string, SceneMetaData>> getAllPanoramaPictures(const std::string& pRefPicFileName)
{
    std::vector<std::pair<std::string, std::string>> picFilesWithPNVFiles;

    try
    {
        const std::filesystem::path refPicFilePath = pRefPicFileName;

        const std::string picFileExt = refPicFilePath.extension();
        const std::filesystem::path picFilesDirPath = refPicFilePath.parent_path();

        std::set<std::filesystem::path> picPaths;
        std::set<std::filesystem::path> pnvPaths;

        for (const std::filesystem::directory_entry& dirEntry : std::filesystem::directory_iterator(picFilesDirPath))
        {
            if (!dirEntry.is_regular_file())
                continue;

            const std::filesystem::path dirEntryPath = dirEntry.path();

            if (dirEntryPath.extension() == picFileExt)
                picPaths.insert(dirEntryPath);
            else if (dirEntryPath.extension() == ".pnv")
                pnvPaths.insert(dirEntryPath);
        }

        for (const auto& picPath : picPaths)
        {
            auto matchingPNVIt = std::find_if(pnvPaths.begin(), pnvPaths.end(),
                                              [picPath](const std::filesystem::path& pPath) { return pPath.stem() == picPath.stem(); });

            if (matchingPNVIt != pnvPaths.end())
                picFilesWithPNVFiles.push_back({picPath.string(), matchingPNVIt->string()});
        }
    }
    catch (const std::exception&)
    {
        return {};
    }

    std::vector<std::pair<std::string, SceneMetaData>> picFilesWithMetaData;

    for (const auto& [picFile, pnvFile] : picFilesWithPNVFiles)
    {
        SceneMetaData metaData;
        if (metaData.loadFromPNVFile(pnvFile))
            picFilesWithMetaData.push_back({picFile, metaData});
    }

    return picFilesWithMetaData;
}

/*!
 * \brief The main function.
 *
 * Parses the command line arguments first and depending on them does the following:
 *
 * - Print usage information (see printHelp()), if help requested ("-h" or "--help") or the arguments are invalid.
 * - If both a picture file and a PTO file (via option "-p" or "--pto=") were provided, read panorama scene meta data
 *   from PTO file, write it to a "PNV file" and exit (see SceneMetaData::loadFromPTOFile() and SceneMetaData::saveToPNVFile()).
 *   The PNV file name will be the same as the picture's file name except for the extension being replaced by ".pnv".
 * - If only a picture file name (optionally preceeded by the fullscreen option "-f" or "--fullscreen") is present, display the
 *   picture's panorama scene with a PanoramaWindow (see PanoramaWindow::run()). The required panorama scene meta data will be loaded
 *   from the corresponding PNV file (see SceneMetaData::loadFromPNVFile()), which is expected to have the same file name as the picture
 *   except for the extension being ".pnv". Whenever requested by the user through PanoramaWindow::run(), other panorama pictures from the
 *   same directory will be shown in an equivalent manner instead of stopping the application, until the last window gets closed normally.
 *
 * \param argc Command line argument count.
 * \param argv Array of command line arguments.
 * \return If successful.
 */
int main(int argc, const char* argv[])
{
    std::vector<std::string> args(argv, argv+argc);

    if (args.empty())
    {
        std::cerr<<"ERROR: You are trying to execute the progam in some weird execution environment. Missing argv[0]."<<std::endl;
        return EXIT_FAILURE;
    }

    //File names of panorama picture and (optionally) the corresponding Hugin project file
    std::string picFileName, ptoFileName;

    //Immediately start in fullscreen mode when displaying the panorama scene
    bool startWithFullscreen = false;

    //Parse command line arguments
    if (args.size() == 2)
    {
        if (args[1] == "-h" || args[1] == "--help")
        {
            printHelp();
            return EXIT_SUCCESS;
        }
        else
            picFileName = args[1];
    }
    else if (args.size() == 3 && (args[1] == "-f" || args[1] == "--fullscreen"))
    {
        picFileName = args[2];
        startWithFullscreen = true;
    }
    else if (args.size() == 3)
    {
        if (args[2].find("--pto=") != 0)
            goto _wrongCmdArg;

        picFileName = args[1];
        ptoFileName = args[2].substr(6);
    }
    else if (args.size() == 4)
    {
        if (args[2] != "-p")
            goto _wrongCmdArg;

        picFileName = args[1];
        ptoFileName = args[3];
    }
    else
    {
        _wrongCmdArg:
        std::cerr<<"ERROR: Wrong or missing command line arguments!\n"<<std::endl;

        printHelp();

        return EXIT_FAILURE;
    }

    //If PTO file was specified, load required information, save to custom PNV file and exit
    if (ptoFileName != "")
    {
        //Use picture file name with replaced extension as PNV file name
        std::string pnvFileName;
        try
        {
            pnvFileName = replaceWithPNVExtension(picFileName);
        }
        catch (const std::runtime_error& exc)
        {
            std::cerr<<"ERROR: Could not determine the PNV file name matching the picture file name: "<<exc.what()<<std::endl;
            return EXIT_FAILURE;
        }

        SceneMetaData metaData;

        if (!metaData.loadFromPTOFile(ptoFileName))
        {
            std::cerr<<"ERROR: Could not parse PTO file!"<<std::endl;
            return EXIT_FAILURE;
        }

        if (!metaData.saveToPNVFile(pnvFileName))
        {
            std::cerr<<"ERROR: Could not save PNV file!"<<std::endl;
            return EXIT_FAILURE;
        }

        std::cout<<"Panorama scene meta data written to PNV file \""<<pnvFileName<<"\".\n";

        return EXIT_SUCCESS;
    }

    //If no PTO file was specified, display the panorama scene using an existing, matching PNV file;
    //also get a list of other panorama pictures in the same directory (those pictures with the same file extension
    //and a matching PNV file), try to load meta data for each of them and display them when requested by the user

    std::vector<std::pair<std::string, SceneMetaData>> dirPicFilesWithMetaData = getAllPanoramaPictures(picFileName);

    auto currentPic = std::find_if(dirPicFilesWithMetaData.begin(), dirPicFilesWithMetaData.end(),
                                   [picFileName](const std::pair<std::string, SceneMetaData>& pPair) { return pPair.first == picFileName; });

    if (currentPic == dirPicFilesWithMetaData.end())
    {
        std::cerr<<"ERROR: Could not find/load/parse a PNV file matching the picture file name!"<<std::endl;
        return EXIT_FAILURE;
    }

    //Create a window
    PanoramaWindow panoWindow;

    //Display the panorama scene for the picture passed to the command line; if requested by the window on its exit, switch
    //to the next/previous picture from the same directory and display it in a new window; otherwise just close the program
    for (;;)
    {
        //File name of the current panorama picture file and corresponding meta data required to properly display the panorama scene
        const std::string& currentFileName = currentPic->first;
        const SceneMetaData& currentMetaData = currentPic->second;

        bool prevRequested = false;
        bool nextRequested = false;

        //Display the panorama scene
        if (!panoWindow.run(currentFileName, currentMetaData, prevRequested, nextRequested, startWithFullscreen))
        {
            std::cerr<<"ERROR: Could not properly display the panorama scene!"<<std::endl;
            return EXIT_FAILURE;
        }

        if (!prevRequested && !nextRequested)   //Exit loop because no other picture requested
            break;
        else if (nextRequested)     //Switch to the next picture for the next loop iteration
        {
            if (++currentPic == dirPicFilesWithMetaData.end())
                currentPic = dirPicFilesWithMetaData.begin();
        }
        else if (prevRequested)     //Switch to the previous picture for the next loop iteration
        {
            if (currentPic == dirPicFilesWithMetaData.begin())
                currentPic = dirPicFilesWithMetaData.end() - 1;
            else
                --currentPic;
        }
    }

    return EXIT_SUCCESS;
}
