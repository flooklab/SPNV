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

#ifndef SPNV_VERSION_VERSION_H
#define SPNV_VERSION_VERSION_H

#include <cstdint>
#include <string>

/*!
 * \brief Program version information.
 *
 * The current version of SPNV is defined here by \ref spnvVersionMajor, \ref spnvVersionMinor,
 * \ref spnvVersionPatch and \ref spnvVersionType.
 *
 * Use toString() to obtain the current version as a combined string.
 *
 * Additionally, the expected name of the compiled executable file is defined by \ref programName.
 */
namespace Version
{

/*!
 * \brief Release type of the released program version.
 *
 * Before a stable release of the program, a release can have one of the
 * listed pre-release states to specify the current development status.
 */
enum class ReleaseType : std::uint8_t
{
    Alpha = 0,              ///< Pre-release \e alpha status.
    Beta = 1,               ///< Pre-release \e beta status.
    ReleaseCandidate = 2,   ///< Pre-release \e release \e candidate status.
    Normal = 3              ///< %Normal/final release.
};

inline constexpr char programName[] = "SPNV";                       ///< Name of the program/executale.

inline constexpr int spnvVersionMajor = 0;                          ///< Program version major number.
inline constexpr int spnvVersionMinor = 2;                          ///< Program version minor number.
inline constexpr int spnvVersionPatch = 0;                          ///< Program version patch number.
inline constexpr ReleaseType spnvVersionType = ReleaseType::Normal; ///< Program version release type.

std::string toString();                                             ///< Get the program version formatted as string.

} // namespace Version

#endif // SPNV_VERSION_VERSION_H
