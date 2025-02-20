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
#include "projector.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#ifndef DISABLE_OMP
#ifdef _OPENMP
#define USE_OMP
#endif
#endif

namespace
{

/*
 * Performs a sane, rounded comparison of the two floats 'pLeft' and 'pRight' within 4 decimal places.
 */
bool roundedCompareSmaller(const float pLeft, const float pRight)
{
    return (static_cast<int>(pLeft * 10000) + 1) < static_cast<int>(pRight * 10000);
}

} // namespace

/*!
 * \brief Constructor.
 *
 * Loads the panorama picture \p pFileName and sets up panorama
 * scene-specific configuration using meta data from \p pSceneMetaData.
 *
 * Sets lower/upper "oversampling" thresholds and the target "oversampling" value (see updateDisplayFOV()
 * and calcLowestDisplayTrafoOversampling()) to fixed values of 1.0, 2.0 and 1.5, respectively.
 *
 * In order to fully set up the panorama scene, call updateDisplaySize().
 * Only then the class can be used and display projections be obtained via getDisplayData().
 *
 * \param pFileName The panorama picture to load.
 * \param pSceneMetaData Meta data for the panorama scene shown in \p pFileName.
 *
 * \throws std::runtime_error Picture loading failed (unsupported file format, file does not exist, etc.).
 * \throws std::runtime_error Picture size from \p pSceneMetaData does not match actual size of \p pFileName.
 */
Projector::Projector(const std::string& pFileName, const SceneMetaData& pSceneMetaData) :
    pic(),
    //
    fileName(pFileName),
    //
    projectionType(pSceneMetaData.getProjectionType()),
    //
    picUncroppedSize(pSceneMetaData.getUncroppedSize()),
    picUncroppedFOV(pSceneMetaData.getUncroppedFOV()),
    //
    picCropPosTL(pSceneMetaData.getCropPosTL()),
    picCropPosBR(pSceneMetaData.getCropPosBR()),
    //
    picSize({picCropPosBR.x - picCropPosTL.x, picCropPosBR.y - picCropPosTL.y}),
    //
    fovTL(calcTopLeftFOV()),
    fovBR(calcBottomRightFOV()),
    //
    fovCentHor({fovBR.x - fovTL.x, 2*std::max(fovTL.y, -fovBR.y)}),
    fovCentHorNoMargin({fovBR.x - fovTL.x, 2*std::min(fovTL.y, -fovBR.y)}),
    fovNonCentHorNoMargin({fovBR.x - fovTL.x, fovTL.y - fovBR.y}),
    //
    fovIs360Degrees(::roundedCompareSmaller(fovCentHor.x, 2*Constants::pi) == false),
    //
    f(0),
    zoom(1),
    minZoomCentHor(std::tan(fovCentHor.y / 2.) / std::tan(fovCentHorNoMargin.y / 2.)),
    minZoomNonCentHor(std::tan(fovCentHor.y / 2.) / std::tan(fovNonCentHorNoMargin.y / 2.)),
    viewOffsetPhi(0),
    viewOffsetTheta(0),
    //
    displaySize({0, 0}),
    displayFOV({0, 0}),
    displayData(),
    //
    staticDisplayTrafosX(),
    staticDisplayTrafosY(),
    //
    panoSphereSize({0, 0}),
    panoSphereData(),
    //
    panoSphereRemapHystMinOvers(1.0),
    panoSphereRemapHystTargOvers(1.5),
    panoSphereRemapHystMaxOvers(2.0),
    panoSphereRemapHystMaxF(0),
    //
    updateDisplayThread(),
    stopUpdateDisplayThread(false),
    updateDisplayMutex(),
    updateDisplayCondVar(),
    startUpdateDisplay(false),
    updateDisplayFinished(false)
{
    if (!pic.loadFromFile(fileName))
        throw std::runtime_error("Could not load the picture \"" + fileName + "\"!");

    if (pic.getSize().x != static_cast<unsigned int>(picSize.x) || pic.getSize().y != static_cast<unsigned int>(picSize.y))
        throw std::runtime_error("Loaded picture size does not match specified cropped picture size!");
}

/*!
 * \brief Destructor.
 *
 * Stops the updateDisplayDataLoop() thread that was (likely) started by updateDisplayData().
 */
Projector::~Projector()
{
    stopUpdateDisplayThread.store(true);
    updateDisplayCondVar.notify_one();

    if (updateDisplayThread.joinable())
        updateDisplayThread.join();
}

//Public

/*!
 * \brief Adjust buffers and transformations for a changed display size.
 *
 * Resizes display projection buffer, adjusts transformations (see updateDisplayFOV())
 * and updates the display projection (see updateDisplayData()).
 *
 * \param pDisplaySize New size for display projection.
 * \param pForceAdjustResolution Force re-projection of panorama sphere (see updateDisplayFOV()).
 */
void Projector::updateDisplaySize(const sf::Vector2u pDisplaySize, const bool pForceAdjustResolution)
{
    displaySize.x = static_cast<int>(pDisplaySize.x);
    displaySize.y = static_cast<int>(pDisplaySize.y);

    displayData.resize(4 * displaySize.x * displaySize.y, 255.);

    updateDisplayFOV(pForceAdjustResolution);

    //Changed FOV might exceed available FOV, so check/fix perspective to avoid margins
    fitViewOffset();

    updateDisplayData();
}

/*!
 * \brief Change the current perspective of the display projection.
 *
 * If \p pZoom is different from current zoom level, changes zoom and adjusts transformations (see updateDisplaySize()).
 * Then changes view angle and updates the display projection according to the new perspective (see updateDisplayData()).
 *
 * If \p pZoom is 0, the zoom will be set to the minimum possible value (to obtain the largest available field of view without margins).
 * If \p pZoom is -1, the vertical view angle offset will be set to 0 (centered horizon) and the zoom will be set to the minimum possible
 * value given the centered horizon condition (to obtain the largest available field of view without margins given a centered horizon).
 *
 * \param pZoom New zoom level (or 0 or -1, see function description).
 * \param pOffsetPhi New horizontal view angle offset.
 * \param pOffsetTheta New vertical view angle offset.
 * \param pForceAdjustResolution Force re-projection of panorama sphere if zoom is changed (see updateDisplayFOV()).
 */
void Projector::updateView(float pZoom, const float pOffsetPhi, float pOffsetTheta, const bool pForceAdjustResolution)
{
    if (pZoom == -1)
    {
        //Set minimum possible zoom under centered horizon condition
        pZoom = minZoomCentHor;

        pOffsetTheta = 0;
    }
    else if (pZoom == 0)
    {
        //Set minimum possible zoom under condition of "perfect" theta angle (that just avoids
        //both top and bottom margins and which will be automatically figured out below)
        pZoom = minZoomNonCentHor;
    }
    else if (pZoom < minZoomNonCentHor) //Constrain zoom level with lower limit to avoid margins
        pZoom = minZoomNonCentHor;

    //Only need to adjust FOV of display projection if zoom level did actually change
    if (pZoom != zoom)
    {
        zoom = pZoom;
        updateDisplayFOV(pForceAdjustResolution);
    }

    viewOffsetPhi = pOffsetPhi;
    viewOffsetTheta = pOffsetTheta;

    //Changed perspective might exceed available FOV, so check/fix perspective to avoid margins
    fitViewOffset();

    updateDisplayData();
}

/*!
 * \brief Vertically center the horizon line.
 *
 * The vertical view angle offset will be set to 0 and the zoom level adjusted as necessary to avoid margins. See also updateView().
 *
 * \param pForceAdjustResolution Force re-projection of panorama sphere if zoom is changed (see updateDisplayFOV()).
 */
void Projector::centerHorizon(const bool pForceAdjustResolution)
{
    updateView(std::max(zoom, minZoomCentHor), viewOffsetPhi, 0, pForceAdjustResolution);
}

//

/*!
 * \brief Get the current horizontal view angle.
 *
 * \return Horizontal view angle offset 'phi'.
 */
float Projector::getOffsetPhi() const
{
    return viewOffsetPhi;
}

/*!
 * \brief Get the current vertical view angle.
 *
 * \return Vertical view angle offset 'theta'.
 */
float Projector::getOffsetTheta() const
{
    return viewOffsetTheta;
}

/*!
 * \brief Get the current zoom level.
 *
 * \return Current zoom level ("arbitrary" units).
 */
float Projector::getZoom() const
{
    return zoom;
}

/*!
 * \brief Get the current zoom level relative to the minimum possible one.
 *
 * \return Current zoom level divided by minimum possible zoom level for no margins.
 */
float Projector::getNormalizedZoom() const
{
    return zoom / minZoomNonCentHor;
}

//

/*!
 * \brief Calculate the zoom level needed to obtain a specific horizontal field of view.
 *
 * Using the return value with updateView() yields a visible horizontal field of view of \p pHFOV.
 *
 * Note that unlike with getRequiredZoomFromVFOV() and the \e vertical field of view, the required
 * zoom for a specific \e horizontal field of view depends on the current display size aspect ratio.
 *
 * \param pHFOV Desired horizontal field of view.
 * \return Required zoom level.
 */
float  Projector::getRequiredZoomFromHFOV(const float pHFOV) const
{
    float aspect = static_cast<float>(displaySize.x) / displaySize.y;

    return std::tan(fovCentHor.y / 2.) * aspect / std::tan(pHFOV / 2.);
}

/*!
 * \brief Calculate the zoom level needed to obtain a specific vertical field of view.
 *
 * Using the return value with updateView() yields a visible vertical field of view of \p pVFOV.
 *
 * Note that unlike with getRequiredZoomFromHFOV() and the \e horizontal field of view, the required
 * zoom for a specific \e vertical field of view is \e independent of the display size aspect ratio.
 *
 * \param pVFOV Desired vertical field of view.
 * \return Required zoom level.
 */
float  Projector::getRequiredZoomFromVFOV(const float pVFOV) const
{
    return std::tan(fovCentHor.y / 2.) / std::tan(pVFOV / 2.);
}

//

/*!
 * \brief Get angle pointed to by specific pixel in the display projection.
 *
 * Calculates the view angle that corresponds to the display position \p pDisplayPosition.
 * Note that current view angle offsets (getOffsetPhi(), getOffsetTheta()) are ignored.
 *
 * \param pDisplayPosition Position in the display projection.
 * \return Corresponding view angle without offsets.
 */
sf::Vector2f Projector::getViewAngle(const sf::Vector2i pDisplayPosition) const
{
    return {staticDisplayTrafoX(pDisplayPosition.x), staticDisplayTrafoY(pDisplayPosition.y, pDisplayPosition.x)};
}

//

/*!
 * \brief Get the display projection of the panorama sphere for current perspective.
 *
 * Returns the image data of the current rectilinear display projection.
 * The image data are stored as a flat array containing continuous rows,
 * with each column occupying four continuous values for "rgba" encoding:
 * - r[x,y] = data[4*(displaySize.x*y + x)]
 * - g[x,y] = data[4*(displaySize.x*y + x) + 1]
 * - b[x,y] = data[4*(displaySize.x*y + x) + 2]
 * - a[x,y] = data[4*(displaySize.x*y + x) + 3] = 255.0
 *
 * The alpha channel is unused!
 *
 * Note that the calculation of the projection and the image data is \e not done
 * by this function but directly by those functions that change perspective.
 *
 * \return Image data of display projection as flat array.
 */
const std::vector<std::uint8_t>& Projector::getDisplayData() const
{
    return displayData;
}

//Private

/*!
 * \brief Calculate 'phi' and 'theta' angle of cropped picture's top left corner.
 *
 * Calculates the point in panorama sphere coordinates ('phi', 'theta') of the uncropped picture where
 * the top left corner of the cropped picture's crop rectangle points to. The result depends on the
 * panorama projection type, uncropped picture's size and field of view and the crop corner position.
 *
 * \return View angle corresponding to top left corner of cropped picture.
 */
sf::Vector2f Projector::calcTopLeftFOV() const
{
    if (projectionType == SceneMetaData::PanoramaProjection::CentralCylindrical)
    {
        return {picUncroppedFOV.x * picCropPosTL.x / picUncroppedSize.x, std::atan(std::tan(picUncroppedFOV.y / 2.f) /
                                                                                   (picUncroppedSize.y / 2.f) *
                                                                                   (picUncroppedSize.y / 2.f - picCropPosTL.y))};
    }
    else if (projectionType == SceneMetaData::PanoramaProjection::Equirectangular)
    {
        return {picUncroppedFOV.x * picCropPosTL.x / picUncroppedSize.x, (picUncroppedFOV.y / 2.f) *
                    (picUncroppedSize.y / 2.f - picCropPosTL.y) / (picUncroppedSize.y / 2.f)};
    }
    else
        return {0, 0};
}

/*!
 * \brief Calculate 'phi' and 'theta' angle of cropped picture's bottom right corner.
 *
 * Calculates the point in panorama sphere coordinates ('phi', 'theta') of the uncropped picture where
 * the bottom right corner of the cropped picture's crop rectangle points to. The result depends on the
 * panorama projection type, uncropped picture's size and field of view and the crop corner position.
 *
 * \return View angle corresponding to bottom right corner of cropped picture.
 */
sf::Vector2f Projector::calcBottomRightFOV() const
{
    if (projectionType == SceneMetaData::PanoramaProjection::CentralCylindrical)
    {
        return {picUncroppedFOV.x * picCropPosBR.x / picUncroppedSize.x, -std::atan(std::tan(picUncroppedFOV.y / 2.f) /
                                                                                    (picUncroppedSize.y / 2.f) *
                                                                                    (picCropPosBR.y - picUncroppedSize.y / 2.f))};
    }
    else if (projectionType == SceneMetaData::PanoramaProjection::Equirectangular)
    {
        return {picUncroppedFOV.x * picCropPosBR.x / picUncroppedSize.x, -(picUncroppedFOV.y / 2.f) *
                    (picUncroppedSize.y / 2.f - (picUncroppedSize.y - picCropPosBR.y)) / (picUncroppedSize.y / 2.f)};
    }
    else
        return {0, 0};
}

//

/*!
 * \brief Calculate panorama sphere 'phi' angle pointed to by a pixel in the display projection (without added variable view angle offset).
 *
 * Assuming the panorama sphere being at the origin (no view angle offset, i.e. centered horizon
 * and centered prime meridian), calculates the 'phi' angle of that point of the panorama sphere
 * that corresponds to a point in the display projection with horizontal position \p pX.
 *
 * Note that the results only change when the zoom or display size change and hence
 * it may be useful to cache them (see also updateStaticDisplayTrafoCache()).
 *
 * \param pX Horizontal display position.
 * \return Corresponding 'phi' angle in the centered panorama sphere.
 */
float Projector::staticDisplayTrafoX(const int pX) const
{
    return std::atan2(static_cast<float>(pX) - static_cast<float>(displaySize.x) / 2., f);
}

/*!
 * \brief Calculate panorama sphere 'theta' angle pointed to by a pixel in the display projection
 *        (without added variable view angle offset).
 *
 * Assuming the panorama sphere being at the origin (no view angle offset, i.e. centered horizon and centered
 * prime meridian), calculates the 'theta' angle of that point of the panorama sphere that corresponds
 * to a point in the display projection with horizontal position \p pX and vertical position \p pY.
 *
 * Note that the results only change when the zoom or display size change and hence
 * it may be useful to cache them (see also updateStaticDisplayTrafoCache()).
 *
 * \param pY Vertical display position.
 * \param pX Horizontal display position.
 * \return Corresponding 'theta' angle in the centered panorama sphere.
 */
float Projector::staticDisplayTrafoY(const int pY, const int pX) const
{
    return std::atan((static_cast<float>(pY) - static_cast<float>(displaySize.y) / 2.) / f *
                     std::sin(std::atan2(f, static_cast<float>(pX) - static_cast<float>(displaySize.x) / 2.)));
}

/*!
 * \brief Calculate horizontal position in the panorama sphere buffer that corresponds to a pixel in the display projection
 *        (with view angle offset).
 *
 * Calculates that horizontal position in the panorama sphere buffer that corresponds to a point in the display projection with
 * horizontal position \p pX. The transformation takes the current perspective into account by including the view angle offset.
 *
 * Note: Requires an up to date cache of display projection to panorama sphere angle transformations
 * in the form of 'staticDisplayTrafosX[\p pX] = staticDisplayTrafoX(\p pX)' for all \p pX.
 * See updateStaticDisplayTrafoCache().
 *
 * Note: Does not check or correct for values outside of [0, panoSphereSize.x).
 *
 * \param pX Horizontal display position.
 * \return Corresponding horizontal panorama sphere buffer position.
 */
float Projector::displayTrafoX(const int pX) const
{
    //Take cached phi angle and add horizontal view angle offset; scale this result by available buffer pixels vs. available FOV
    return (staticDisplayTrafosX[pX] + viewOffsetPhi) * panoSphereSize.x / fovCentHor.x;
}

/*!
 * \brief Calculate vertical position in the panorama sphere buffer that corresponds to a pixel in the display projection
 *        (with view angle offset).
 *
 * Calculates that vertical position in the panorama sphere buffer that corresponds to a point
 * in the display projection with horizontal position \p pX and vertical position \p pY. The
 * transformation takes the current perspective into account by including the view angle offset.
 *
 * Note: Requires an up to date cache of display projection to panorama sphere angle transformations in the form of
 * 'staticDisplayTrafosY[(displaySize.x+1) * \p pY + \p pX] = staticDisplayTrafoY(\p pY, \p pX)' for all \p pX and \p pY.
 * See updateStaticDisplayTrafoCache().
 *
 * Note: Does not check or correct for values outside of [0, panoSphereSize.y) or [0, panoSphereSize.x).
 *
 * \param pY Vertical display position.
 * \param pX Horizontal display position.
 * \return Corresponding vertical panorama sphere buffer position.
 */
float Projector::displayTrafoY(const int pY, const int pX) const
{
    //Take cached theta angle and add vertical view angle offset; scale this result by available buffer pixels vs.
    //available FOV and add an offset to vertically align center of projection with zero theta angle (sphere origin)
    return (staticDisplayTrafosY[(displaySize.x+1)*pY + pX] + viewOffsetTheta) * panoSphereSize.y / fovCentHor.y + panoSphereSize.y / 2.;
}

//

/*!
 * \brief Re-calculate cache of display projection to panorama sphere angle transformations used by displayTrafoX() and displayTrafoY().
 *
 * Updates the cache for display projection to panorama sphere angle transformations (see staticDisplayTrafoX(), staticDisplayTrafoY()):
 * - 'staticDisplayTrafosX[\p pX] = staticDisplayTrafoX(\p pX)'
 * - 'staticDisplayTrafosY[(displaySize.x+1) * \p pY + \p pX] = staticDisplayTrafoY(\p pY, \p pX)'
 *
 * An up to date cache is needed by displayTrafoX() and displayTrafoY().
 * The transformations must change when the zoom or display size change.
 */
void Projector::updateStaticDisplayTrafoCache()
{
    staticDisplayTrafosX.resize(displaySize.x+1, 0);
    staticDisplayTrafosY.resize((displaySize.x+1)*(displaySize.y+1), 0);

    for (int x = 0; x <= displaySize.x; ++x)
        staticDisplayTrafosX[x] = staticDisplayTrafoX(x);

    for (int y = 0; y <= displaySize.y; ++y)
        for (int x = 0; x <= displaySize.x; ++x)
            staticDisplayTrafosY[(displaySize.x+1)*y + x] = staticDisplayTrafoY(y, x);
}

//

/*!
 * \brief Calculate smallest ratio of delta(panorama sphere pixels) vs. delta(display projection pixels)
 *        of all positions for both directions.
 *
 * In order to have good performance of updateDisplayData() while maintaining sufficient resolution of the display projection one
 * may want to target a low "oversampling" of the projection close to but larger than one for every zoom level. The oversampling
 * estimate may then be used to adjust the panorama sphere size in such a way as to obtain a desired oversampling value.
 *
 * This function estimates the oversampling one-dimensionally in terms of the number of panorama sphere pixels projected
 * onto a single display projection pixel for horizontal and vertical directions and returns the minimum of both results.
 * The estimation is done for the top left corner of the display projection, as the oversampling is the lowest in the corners.
 *
 * Note: Requires an up to date cache of display projection to panorama sphere angle transformations
 * (see both displayTrafoX() and displayTrafoY()).
 *
 * \return Lowest oversampling of panorama sphere to display projection (for metric see function description).
 */
float Projector::calcLowestDisplayTrafoOversampling() const
{
    //Lowest resolution at corners of projection/FOV
    float tx0 = displayTrafoX(0);       //Left edge
    float tx1 = displayTrafoX(1);       //One pixel to the right
    float ty0 = displayTrafoY(0, 0);    //Top edge
    float ty1 = displayTrafoY(1, 0);    //One pixel downwards

    float overX = tx1 - tx0;
    float overY = ty1 - ty0;

    return std::min(overX, overY);
}

//

/*!
 * \brief Clip view angle offset as necessary to stay within available field of view.
 *
 * Calculates minimum/maximum 'phi' and 'theta' angles for keeping the field of view of the
 * display projection within the available field of view of the panorama scene (no margins).
 *
 * Clips the current view angle offset at these values. Also ensures 'phi' within [0, 2*pi).
 *
 * Additionally, centers 'phi' if the horizontal field of view of the display projection
 * is actually wider than the total available field of view of the panorama scene.
 */
void Projector::fitViewOffset()
{
    //Limit horizontal view angle offset such that no point in display projection is beyond available FOV;
    //in case of a 360 degree panorama do not limit horizontal view angle offset, but keep it within [0, 2*PI)
    if (!fovIs360Degrees)
    {
        if (fovCentHor.x < displayFOV.x)
            viewOffsetPhi = fovCentHor.x / 2.;  //Horizontally center the scene if display FOV is wider than scene FOV
        else if (viewOffsetPhi < displayFOV.x / 2.)
            viewOffsetPhi = displayFOV.x / 2.;
        else if (viewOffsetPhi > fovCentHor.x - displayFOV.x / 2.)
            viewOffsetPhi = fovCentHor.x - displayFOV.x / 2.;
    }
    else
    {
        if (viewOffsetPhi < 0)
            viewOffsetPhi += 2*Constants::pi;
        else if (viewOffsetPhi >= 2*Constants::pi)
            viewOffsetPhi -= 2*Constants::pi;
    }

    //Also limit vertical view angle offset such that no point in display projection is beyond available FOV

    if (displayFOV.y / 2. - viewOffsetTheta > fovTL.y)
        viewOffsetTheta = (displayFOV.y / 2. - fovTL.y);
    else if (displayFOV.y / 2. + viewOffsetTheta > -fovBR.y)
        viewOffsetTheta = -(displayFOV.y / 2. + fovBR.y);
}

//

/*!
 * \brief Adjust parameters and transformations after display size or zoom change.
 *
 * Updates field of view parameters and according display projection cache (see updateStaticDisplayTrafoCache()).
 *
 * Then uses calcLowestDisplayTrafoOversampling() to check if the panorama sphere needs to be resized (see mapPicToPanoSphere())
 * in order to balance projection performance and resolution. Resizing is done above and below a fixed oversampling threshold
 * and targets a fixed oversampling value between the thresholds. For their values see Projector().
 *
 * If the resizing e.g. after zooming in does not restore an oversampling above the lower threshold
 * (limited picture resolution), the current "virtual camera focal length" parameter is remembered
 * and then used on further calls to this function to skip the resizing beyond that point.
 *
 * The parameter \p pForceRemapSphere is used to enforce sphere resizing.
 *
 * \param pForceRemapSphere Force resizing and re-mapping of the panorama sphere.
 */
void Projector::updateDisplayFOV(const bool pForceRemapSphere)
{
    float aspect = static_cast<float>(displaySize.x) / displaySize.y;

    //Always normalize visible FOV via the constant maximum symmetric FOV and use a zoom parameter to obtain any other FOV

    const float tanFOV2 = std::tan(fovCentHor.y / 2.);

    displayFOV.x = 2. * std::atan(tanFOV2 * aspect / zoom); //Derive horizontal FOV from vertical FOV and aspect ratio
    displayFOV.y = 2. * std::atan(tanFOV2          / zoom); //Use a vertical FOV that is independent of display size

    //Define focal length of a virtual camera that is scaled according to a fixed "sensor size" equal to 'displaySize'
    const float f0 = displaySize.y / 2. / tanFOV2;

    //Displayed FOV is controlled via a changing focal length, which in turn is changed by changing the multiplied zoom factor
    f = f0 * zoom;

    //Transformations depend on current FOV, hence cache needs update
    updateStaticDisplayTrafoCache();

    //Automatically remap panorama sphere if displayed resolution went too low (except if not useful anymore) or unnecessarily large

    bool remapSphere = ((calcLowestDisplayTrafoOversampling() < panoSphereRemapHystMinOvers) &&
                        (panoSphereRemapHystMaxF == 0 || f < panoSphereRemapHystMaxF)               //Skip, if FOV "too large" (see below)
                        ) || (calcLowestDisplayTrafoOversampling() > panoSphereRemapHystMaxOvers);

    if (remapSphere || pForceRemapSphere)
    {
        mapPicToPanoSphere();

        //If zoomed resolution could not be further increased by remapping sphere, set remap limit for current focal length parameter 'f'
        if (remapSphere && (calcLowestDisplayTrafoOversampling() < panoSphereRemapHystMinOvers))
            panoSphereRemapHystMaxF = f;
    }
}

//

/*!
 * \brief Project current panorama sphere perspective to display projection buffer.
 *
 * Starts updateDisplayDataLoop() as thread if not already running and triggers its next iteration to fill the
 * display projection buffer with a rectilinear projection of the panorama sphere at the current perspective.
 * See updateDisplayDataLoop().
 *
 * Note: Waits for the update iteration to finish and only then returns.
 */
void Projector::updateDisplayData()
{
    if (!updateDisplayThread.joinable())
        updateDisplayThread = std::thread(&Projector::updateDisplayDataLoop, this);

    {
        std::unique_lock lock(updateDisplayMutex);
        startUpdateDisplay = true;
        lock.unlock();
        updateDisplayCondVar.notify_one();
        std::this_thread::yield();
        lock.lock();
        updateDisplayCondVar.wait(lock, [this](){ return updateDisplayFinished; });
        updateDisplayFinished = false;
    }
}

/*!
 * \brief Repeatedly project panorama sphere perspective to display projection buffer.
 *
 * Repeatedly updates the display projection buffer whenever triggered
 * by updateDisplayData() and returns as soon as stopped by ~Projector().
 *
 * For each triggered update iteration it fills the display projection buffer with a
 * rectilinear projection of the panorama sphere at the current perspective defined by
 * zoom level and view angle offset. Pixel colors are interpolated between the two buffers
 * of arbitrary resolution via a simple area weighting (see also interpolatePixel()).
 */
void Projector::updateDisplayDataLoop()
{
    //Flat array of panorama sphere image data
    const std::uint8_t* sourcePixels = nullptr;

    //Cache for projection transformation values
    std::vector<float> displayTrafosX;
    std::vector<float> displayTrafosY;

    #ifdef USE_OMP
    #pragma omp parallel
    #endif
    while (!stopUpdateDisplayThread.load())
    {
        #ifdef USE_OMP
        #pragma omp single
        #endif
        {
            {
                std::unique_lock lock(updateDisplayMutex);
                updateDisplayCondVar.wait(lock, [this](){ return (startUpdateDisplay || stopUpdateDisplayThread.load()); });
                startUpdateDisplay = false;
            }

            if (!stopUpdateDisplayThread.load())
            {
                sourcePixels = panoSphereData.data();

                //Cache final projection transformation values for current perspective as they are reused for every projection pixel below

                displayTrafosX.resize(displaySize.x+1, 0);
                displayTrafosY.resize((displaySize.x+1)*(displaySize.y+1), 0);

                for (int x = 0; x <= displaySize.x; ++x)
                    displayTrafosX[x] = displayTrafoX(x);

                for (int y = 0; y <= displaySize.y; ++y)
                    for (int x = 0; x <= displaySize.x; ++x)
                        displayTrafosY[(displaySize.x+1)*y + x] = displayTrafoY(y, x);
            }
        }

        if (stopUpdateDisplayThread.load())
            break;

        //Go through every projection pixel coordinate, calculate the rectangle in the panorama sphere
        //corresponding to the pixel's square and interpolate the pixel color as the mean color of the rectangle
        #ifdef USE_OMP
        #pragma omp for collapse(2)
        #endif
        for (int y = 0; y < displaySize.y; ++y)
        {
            for (int x = 0; x < displaySize.x; ++x)
            {
                //Top left and bottom right corner coordinates of the projection pixel transformed to the panorama sphere
                float tLx = displayTrafosX[x];
                float bRx = displayTrafosX[x+1];
                const float tLy = displayTrafosY[(displaySize.x+1)*y + x];
                const float bRy = displayTrafosY[(displaySize.x+1)*(y+1) + x + 1];

                bool pixelOutOfRange = false;

                //In case of a 360 degree panorama the transformations might output values that exceed FOV of the scene;
                //at this point only fix lower boundary to avoid negative individual values but also avoid wrong negative difference (bRx-tLx)
                if (fovIs360Degrees)
                {
                    if (tLx < 0)
                        tLx += panoSphereSize.x;
                    if (bRx - tLx < 0)
                        bRx += panoSphereSize.x;
                }
                else    //For horizontally finite (i.e. below 360 degrees) panoramas there might also be values that exceed FOV of the scene
                {       //if the window/display aspect ratio is very wide; limit boundaries then and detect pixels fully out of bounds
                    if (tLx < 0)
                    {
                        tLx = 0;
                        if (bRx < 0)
                        {
                            bRx = 0;
                            pixelOutOfRange = true;
                        }
                    }
                    else if (static_cast<int>(bRx) >= panoSphereSize.x)
                    {
                        bRx = std::nextafter(static_cast<float>(panoSphereSize.x), 0.f);
                        if (tLx > bRx)
                        {
                            tLx = bRx;
                            pixelOutOfRange = true;
                        }
                    }
                }

                if (!pixelOutOfRange)
                {
                    //Interpolate current display pixel color from panorama sphere pixels covered by the transformed pixel rectangle
                    interpolatePixel(panoSphereSize, sourcePixels,
                                     displayData[4*(displaySize.x*y + x)],
                                     displayData[4*(displaySize.x*y + x) + 1],
                                     displayData[4*(displaySize.x*y + x) + 2],
                                     tLx, tLy, bRx, bRy);
                }
                else
                {
                    //Make fully out of bounds pixels black for a horizontally finite panorama (as it cannot be wrapped around 360 degrees)
                    displayData[4*(displaySize.x*y + x)] = 0;
                    displayData[4*(displaySize.x*y + x) + 1] = 0;
                    displayData[4*(displaySize.x*y + x) + 2] = 0;
                }
            }
        }

        #ifdef USE_OMP
        #pragma omp single
        #endif
        {
            std::unique_lock lock(updateDisplayMutex);
            updateDisplayFinished = true;
            lock.unlock();
            updateDisplayCondVar.notify_one();
        }
    }
}

/*!
 * \brief Project the loaded picture onto the panorama sphere.
 *
 * Fills the panorama sphere buffer with a spherical projection of the loaded panorama picture. The used projection
 * transformation is selected according to the scene's panorama projection type (see SceneMetaData::PanoramaProjection).
 *
 * The size of the panorama sphere is set such that a target "oversampling" of the panorama sphere
 * to display projection (see calcLowestDisplayTrafoOversampling() and also updateDisplayFOV())
 * is obtained, except if the limited panorama picture resolution and current perspective
 * do not allow to do so. For the target oversampling value see Projector().
 *
 * The pixel colors are interpolated between the two buffers of arbitrary
 * resolution via a simple area weighting (see also interpolatePixel()).
 */
void Projector::mapPicToPanoSphere()
{
    //Initially set sphere width to loaded picture width (max. useful size); scale height via FOV, as sphere coordinates are simply angles
    panoSphereSize.x = picSize.x;
    panoSphereSize.y = static_cast<int>(picSize.x * fovCentHor.y / fovCentHor.x + 1.);

    //Define scale factor to possibly lower sphere size below
    float scaleFactor = 1;

    const float picHorizonY = picUncroppedSize.y / 2. - picCropPosTL.y;   //Horizon position in loaded cropped picture
    const float tanFOV2 = std::tan(picUncroppedFOV.y / 2.);

    //Transformations from loaded picture to panorama sphere buffer coordinates (they depend on above scale factor and scaled sphere size)

    //Equirectangular projection (horizontal component); equivalent to central cylindrical projection
    auto equirectToSphereTrafoX = [&scaleFactor](float pX) -> float
    {
        return pX / scaleFactor;
    };

    //Equirectangular projection (vertical component)
    auto equirectToSphereTrafoY = [this, &scaleFactor, picHorizonY](float pY) -> float
    {
        return (pY - panoSphereSize.y / 2.) / scaleFactor + picHorizonY;
    };

    //Central cylindrical projection (vertical component)
    auto cylindToSphereTrafoY = [this, &scaleFactor, picHorizonY, tanFOV2](float pY) -> float
    {
        return std::tan((pY - panoSphereSize.y / 2.) * fovCentHor.y / panoSphereSize.y) / tanFOV2 * picUncroppedSize.y / 2. + picHorizonY;
    };

    //Same x-transformation for central cylindrical and equirectangular projections
    const auto& sphereTrafoX = equirectToSphereTrafoX;

    //Choose y-tranformation according to projection type
    auto sphereTrafoY = [equirectToSphereTrafoY, cylindToSphereTrafoY,
                         cylindrical = (projectionType == SceneMetaData::PanoramaProjection::CentralCylindrical)](float pY) -> float
    {
        if (cylindrical)
            return cylindToSphereTrafoY(pY);
        else
            return equirectToSphereTrafoY(pY);
    };

    //If finally display pixels transform to unnecessarily many sphere pixels (too high "oversampling"), reduce sphere size (or resolution)

    float over = calcLowestDisplayTrafoOversampling();

    if (over > panoSphereRemapHystTargOvers)
    {
        //Change scale factor and sphere size accordingly (note: transformations above capture these by reference or this pointer)

        scaleFactor = panoSphereRemapHystTargOvers / over;

        panoSphereSize.x = static_cast<int>(scaleFactor * picSize.x + 1.);
        panoSphereSize.y = static_cast<int>(scaleFactor * picSize.x * fovCentHor.y / fovCentHor.x + 1.);
    }

    panoSphereData.resize(4 * panoSphereSize.x * panoSphereSize.y, 255.);

    //Flat array of loaded panorama picture data
    const std::uint8_t* sourcePixels = pic.getPixelsPtr();

    //Cache transformation values as they are reused for every sphere pixel below

    std::vector<float> sphereTrafosX(panoSphereSize.x+1, 0);
    std::vector<float> sphereTrafosY(panoSphereSize.y+1, 0);

    for (int x = 0; x <= panoSphereSize.x; ++x)
        sphereTrafosX[x] = sphereTrafoX(x);
    for (int y = 0; y <= panoSphereSize.y; ++y)
        sphereTrafosY[y] = sphereTrafoY(y);

    //Go through every sphere pixel coordinate, calculate the rectangle in the picture corresponding
    //to the pixel's square and interpolate the pixel color as the mean color of the rectangle
    #ifdef USE_OMP
    #pragma omp parallel for collapse(2)
    #endif
    for (int y = 0; y < panoSphereSize.y; ++y)
    {
        for (int x = 0; x < panoSphereSize.x; ++x)
        {
            //Top left and bottom right corner coordinates of the sphere pixel transformed to the loaded picture
            float tLx = sphereTrafosX[x];
            float tLy = sphereTrafosY[y];
            float bRx = sphereTrafosX[x+1];
            float bRy = sphereTrafosY[y+1];

            //Pixels might point to out of bounds part of the picture, because picture not always symmetric but sphere is (just ignore them)
            if (bRy <= 0 || tLy >= picSize.y)
                continue;

            //Interpolate current panorama sphere pixel color from panorama picture pixels covered by the transformed pixel rectangle
            interpolatePixel(picSize, sourcePixels,
                             panoSphereData[4*(panoSphereSize.x*y + x)],
                             panoSphereData[4*(panoSphereSize.x*y + x) + 1],
                             panoSphereData[4*(panoSphereSize.x*y + x) + 2],
                             tLx, tLy, bRx, bRy);
        }
    }
}

//

/*!
 * \brief Interpolate target pixel color from rectangle in source image by area weighting.
 *
 * The color values of pixels from \p pSourcePixels within the rectangle {{\p pTLx, \p pTLy}, {\p pBRx, \p pBRy}}
 * are averaged and the resulting color is applied to \p pTargetPixelR, \p pTargetPixelG and \p pTargetPixelB.
 * The color averaging is area-weighted. It uses the intersection of the source pixel and rectangle areas as weights.
 *
 * The format of the source image data \p pSourcePixels must be equivalent to the format used for the data
 * returned by getDisplayData() (see there) with the source image size being \p pSourceImageSize here.
 *
 * \p pTargetPixelR / \p pTargetPixelG / \p pTargetPixelB are references
 * to the individual red/green/blue color values of the target pixel.
 *
 * \param pSourceImageSize Size of the source image.
 * \param pSourcePixels Source image data as flat array.
 * \param pTargetPixelR Target image pixel red color value.
 * \param pTargetPixelG Target image pixel green color value.
 * \param pTargetPixelB Target image pixel blue color value.
 * \param pTLx Horizontal coordinate of source image rectangle's top left corner.
 * \param pTLy Vertical coordinate of source image rectangle's top left corner.
 * \param pBRx Horizontal coordinate of source image rectangle's bottom right corner.
 * \param pBRy Vertical coordinate of source image rectangle's bottom right corner.
 */
void Projector::interpolatePixel(const sf::Vector2i pSourceImageSize, const std::uint8_t *const pSourcePixels,
                                 std::uint8_t& pTargetPixelR, std::uint8_t& pTargetPixelG, std::uint8_t& pTargetPixelB,
                                 const float pTLx, const float pTLy, const float pBRx, const float pBRy)
{
    //Coordinates of topmost and leftmost source pixels that are at least partially covered by the transformed rectangle
    int tLxi = static_cast<int>(pTLx);
    int tLyi = static_cast<int>(pTLy);

    //Coordinates of bottommost and rightmost source pixels that are at least partially covered by the transformed rectangle
    int bRxi = static_cast<int>(pBRx);
    int bRyi = static_cast<int>(pBRy);

    //Integer width and height of a rectangle that fully covers all relevant pixels (the edge pixels then may only count partially)
    int nx = bRxi - tLxi + 1;
    int ny = bRyi - tLyi + 1;

    //Area-weighted sum of RGB color values
    float r = 0;
    float g = 0;
    float b = 0;

    //Total area of transformed rectangle
    float totalWeight = 0;

    //Go through every source pixel that is at least partially covered by the transformed rectangle;
    //sum the pixels' color values, each weighted by the intersection of the pixel and rectangle areas
    for (int iy = 0; iy < ny; ++iy)
    {
        //Vertical bounds check (pixels might be out of bounds due to rounding effects, just ignore them)
        if (tLyi+iy < 0 || tLyi+iy >= pSourceImageSize.y)
            continue;

        //Calculate height of intersection of pixel and rectangle
        float yWeight = 1;
        if (iy == 0)
            yWeight = 1 + tLyi - pTLy;
        else if (iy == ny - 1)
            yWeight = pBRy - bRyi;

        for (int ix = 0; ix < nx; ++ix)
        {
            //Horizontal bounds check (pixels out of bounds only relevant for 360 degree panoramas, so just add full width offset)
            int wrap = 0;
            if (tLxi+ix < 0)
                wrap = pSourceImageSize.x;
            else if (tLxi+ix >= pSourceImageSize.x)
                wrap = -pSourceImageSize.x;

            //Must never wrap around for horizontally finite panoramas (i.e. ignore those pixels)
            if (!fovIs360Degrees && wrap != 0)
                continue;

            //Calculate width of intersection of pixel and rectangle
            float xWeight = 1;
            if (ix == 0)
                xWeight = 1 + tLxi - pTLx;
            else if (ix == nx - 1)
                xWeight = pBRx - bRxi;

            //Weight is intersection of pixel and rectangle areas
            float weight = xWeight * yWeight;

            totalWeight += weight;

            r += weight * pSourcePixels[4*(pSourceImageSize.x*(tLyi+iy) + tLxi+ix + wrap)];
            g += weight * pSourcePixels[4*(pSourceImageSize.x*(tLyi+iy) + tLxi+ix + wrap) + 1];
            b += weight * pSourcePixels[4*(pSourceImageSize.x*(tLyi+iy) + tLxi+ix + wrap) + 2];
        }
    }

    //Set target pixel color to area-weighted color of the transformed source rectangle

    pTargetPixelR = r / totalWeight;
    pTargetPixelG = g / totalWeight;
    pTargetPixelB = b / totalWeight;
}
