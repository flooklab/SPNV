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

#include "version.h"

namespace Version
{

/*!
 * \brief Get the program version formatted as string.
 *
 * The format is "MAJ.MIN.PATCH" plus "-alpha" / "-beta" / "-rc" for the pre-release types
 * ReleaseType::Alpha / ReleaseType::Beta / ReleaseType::ReleaseCandidate.
 *
 * \return The current program version.
 */
std::string toString()
{
    std::string verStr = std::to_string(spnvVersionMajor) + "." + std::to_string(spnvVersionMinor) + "." +
                         std::to_string(spnvVersionPatch);

    if (spnvVersionType == ReleaseType::Alpha)
        verStr += "-alpha";
    else if (spnvVersionType == ReleaseType::Beta)
        verStr += "-beta";
    else if (spnvVersionType == ReleaseType::ReleaseCandidate)
        verStr += "-rc";

    return verStr;
}

} // namespace Version
