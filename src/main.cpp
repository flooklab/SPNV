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

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

/// \file
///
/// \brief The %main.cpp of SPNV.
///
/// \details See main() for information about what this does.

/*!
 * \brief Print usage information.
 *
 * Prints a description of the program call and its command line options to stderr.
 */
void printHelp()
{
    std::string helpString = "USAGE:\n " + std::string(SPNV_PROGRAM_NAME);

    helpString.append(" [--help]");
    helpString.append(" PANORAMA-PICTURE");
    helpString.append(" [--pto=HUGIN-FILE | -p HUGIN-FILE]");

    helpString.append("\n\nDESCRIPTION:\n");
    helpString.append(" If no options are present, displays the panorama scene in PANORAMA-PICTURE using information from "
                      "previously saved \"PNV\" file (see OPTIONS)\n");

    helpString.append("\nOPTIONS:\n");

    helpString.append(" -h, --help\n        Print a description of the command line options and exit.\n\n");
    helpString.append(" -p, --pto=HUGIN-FILE\n        Extract information from Hugin project needed to properly display "
                      "PANORAMA-PICTURE. Save this information to a \"PNV\" file (same basename as PANORAMA-PICTURE) and exit.\n");

    std::cerr<<helpString;
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
 * - If only a picture file name is present, display the picture's panorama scene with a PanoramaWindow (see PanoramaWindow::run()).
 *   The required panorama scene meta data will be loaded from the corresponding PNV file (see SceneMetaData::loadFromPNVFile()),
 *   which is expected to have the same file name as the picture except for the extension being ".pnv".
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

    //Define PNV file name corresponding to the picture file name (replace extension)
    std::string pnvFileName;
    try
    {
        std::filesystem::path tPath(picFileName);
        tPath.replace_extension("pnv");
        pnvFileName = tPath.string();

        //If no PTO file was specified, there must be an already existing PNV file
        if (ptoFileName == "" && !std::filesystem::exists(tPath))
        {
            std::cerr<<"ERROR: Could not find matching PNV file!"<<std::endl;
            return EXIT_FAILURE;
        }
    }
    catch (const std::exception& exc)
    {
        std::cerr<<"ERROR: Could not find out the PNV file name matching the picture file name!"<<std::endl;
        std::cerr<<exc.what()<<std::endl;
        return EXIT_FAILURE;
    }

    //Setup meta data (loading from file below) required to properly display panorama scene
    SceneMetaData metaData;

    //If PTO file was specified, load required information and save to custom PNV file
    if (ptoFileName != "")
    {
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
    else    //If no PTO file was specified, load required information from matching PNV file
    {
        if (!metaData.loadFromPNVFile(pnvFileName))
        {
            std::cerr<<"ERROR: Could not parse PNV file!"<<std::endl;
            return EXIT_FAILURE;
        }
    }

    //Create window and display the panorama scene using previously loaded meta data

    PanoramaWindow panoWindow;

    if (!panoWindow.run(picFileName, metaData))
    {
        std::cerr<<"ERROR: Could not properly display the panorama scene!"<<std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
