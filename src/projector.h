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

#ifndef SPNV_PROJECTOR_H
#define SPNV_PROJECTOR_H

#include "scenemetadata.h"

#include <SFML/Config.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/System/Vector2.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

/*!
 * \brief Panorama picture loading and projection onto virtual camera for varying perspectives.
 *
 * Loads a panorama picture and maps it onto a panorama sphere using additional information about
 * the panorama scene provided in form of SceneMetaData. See also Projector(). A limited field of
 * view rectilinear projection of the panorama sphere can be obtained from getDisplayData(), which
 * can be used, for instance, to display the current perspective on a screen or save it as a snapshot.
 * This rectilinear projection is a 2-dim. image and its size must be set via updateDisplaySize().
 *
 * The current perspective (view angle and zoom) can be set using updateView() (which will also
 * update the projection data) and can be queried via getOffsetPhi(), getOffsetTheta() and getZoom().
 * The function centerHorizon() automatically changes perspective such that the horizon line will be centered.
 *
 * Note that these perspective values are automatically adjusted, if the resulting rectilinear
 * projection would include areas not covered by the loaded panorama scene (no margins).
 *
 * Note also that this implies a minimal possible zoom level. A normalized zoom level
 * measured relative to this minimal zoom can be obtained via getNormalizedZoom().
 *
 * For achieving a specific field of view in either horizontal or vertical direction
 * one can use getRequiredZoomFromHFOV() or getRequiredZoomFromVFOV().
 *
 * The function getViewAngle() can be used to get the view angle pointed to by a specific pixel
 * of the rectilinear projection of getDisplayData(). Note that this does \e not include the view
 * angle offsets set by updateView() but it is nevertheless useful for navigation via mouse drag.
 */
class Projector
{
public:
    Projector(const std::string& pFileName, const SceneMetaData& pSceneMetaData);   ///< Constructor.
    ~Projector();                                                                   ///< Destructor.

public:
    void updateDisplaySize(sf::Vector2u pDisplaySize,
                           bool pForceAdjustResolution = false);        ///< Adjust buffers and transformations for a changed display size.
    void updateView(float pZoom, float pOffsetPhi, float pOffsetTheta,
                    bool pForceAdjustResolution = false);               ///< Change the current perspective of the display projection.
    void centerHorizon(bool pForceAdjustResolution = false);            ///< Vertically center the horizon line.
    //
    float getOffsetPhi() const;                         ///< Get the current horizontal view angle.
    float getOffsetTheta() const;                       ///< Get the current vertical view angle.
    float getZoom() const;                              ///< Get the current zoom level.
    float getNormalizedZoom() const;                    ///< Get the current zoom level relative to the minimum possible one.
    //
    float getRequiredZoomFromHFOV(float pHFOV) const;   ///< Calculate the zoom level needed to obtain a specific horizontal field of view.
    float getRequiredZoomFromVFOV(float pVFOV) const;   ///< Calculate the zoom level needed to obtain a specific vertical field of view.
    //
    sf::Vector2f getViewAngle(sf::Vector2i pDisplayPosition) const; ///< Get angle pointed to by specific pixel in the display projection.
    //
    const std::vector<sf::Uint8>& getDisplayData() const;   ///< Get the display projection of the panorama sphere for current perspective.

private:
    sf::Vector2f calcTopLeftFOV() const;                    ///< Calculate 'phi' and 'theta' angle of cropped picture's top left corner.
    sf::Vector2f calcBottomRightFOV() const;                ///< Calculate 'phi' and 'theta' angle of cropped picture's bottom right corner.
    //
    float staticDisplayTrafoX(int pX) const;                ///< \brief Calculate panorama sphere 'phi' angle pointed to by a pixel
                                                            ///  in the display projection (without added variable view angle offset).
    float staticDisplayTrafoY(int pY, int pX) const;        ///< \brief Calculate panorama sphere 'theta' angle pointed to by a pixel
                                                            ///  in the display projection (without added variable view angle offset).
    float displayTrafoX(int pX) const;                      ///< \brief Calculate horizontal position in the panorama sphere buffer that
                                                            ///  corresponds to a pixel in the display projection (with view angle offset).
    float displayTrafoY(int pY, int pX) const;              ///< \brief Calculate vertical position in the panorama sphere buffer that
                                                            ///  corresponds to a pixel in the display projection (with view angle offset).
    //
    void updateStaticDisplayTrafoCache();                   ///< \brief Re-calculate cache of display projection to panorama sphere angle
                                                            ///  transformations used by displayTrafoX() and displayTrafoY().
    //
    float calcLowestDisplayTrafoOversampling() const;       ///< \brief Calculate smallest ratio of delta(panorama sphere pixels) vs.
                                                            ///  delta(display projection pixels) of all positions for both directions.
    //
    void fitViewOffset();                                   ///< Clip view angle offset as necessary to stay within available field of view.
    //
    void updateDisplayFOV(bool pForceRemapSphere = false);  ///< Adjust parameters and transformations after display size or zoom change.
    //
    void updateDisplayData();                               ///< Project current panorama sphere perspective to display projection buffer.
    void updateDisplayDataLoop();                           ///< Repeatedly project panorama sphere perspective to display projection buffer.
    void mapPicToPanoSphere();                              ///< Project the loaded picture onto the panorama sphere.
    //
    void interpolatePixel(sf::Vector2i pSourceImageSize, const sf::Uint8 *const pSourcePixels,
                          sf::Uint8& pTargetPixelR, sf::Uint8& pTargetPixelG, sf::Uint8& pTargetPixelB,
                          float pTLx, float pTLy, float pBRx, float pBRy);      ///< \brief Interpolate target pixel color from
                                                                                ///  rectangle in source image by area weighting.

private:
    sf::Image pic;                                          ///< Loaded panorama picture.
    //
    const std::string fileName;                             ///< File name of the panorama picture.
    //
    const SceneMetaData::PanoramaProjection projectionType; ///< Panorama picture projection type.
    //
    const sf::Vector2i picUncroppedSize;            ///< Size of panorama picture if it was uncropped (size symmetric about horizon line).
    const sf::Vector2f picUncroppedFOV;             ///< FOV of panorama picture if it was uncropped (FOV symmetric about horizon line).
    //
    const sf::Vector2i picCropPosTL;                ///< Position in uncropped picture corresponding to top left corner of cropped picture.
    const sf::Vector2i picCropPosBR;                ///< Position in uncropped picture corresponding to bottom right corner of cropped picture.
    //
    const sf::Vector2i picSize;                     ///< Size of cropped picture derived from crop information.
    //
    const sf::Vector2f fovTL;                       ///< Horizontal/vertical FOV angle corresponding to top left corner of cropped picture.
    const sf::Vector2f fovBR;                       ///< Horizontal/vertical FOV angle corresponding to bottom right corner of cropped picture.
    //
    const sf::Vector2f fovCentHor;                  ///< Maximum symmetric FOV covered by cropped picture (possibly with one-sided margin).
    const sf::Vector2f fovCentHorNoMargin;          ///< Maximum symmetric FOV fully covered by cropped picture without visible margins.
    const sf::Vector2f fovNonCentHorNoMargin;       ///< Maximum asymmetric FOV fully covered by cropped picture without visible margins.
    //
    float f;                                    ///< Focal length-like parameter used for camera-like rectilinear display projection.
    float zoom;                                 ///< Controls the visible amount of field of view.
    const float minZoomCentHor;                 ///< Minimal allowed zoom for display projections without margins (given centered horizon).
    const float minZoomNonCentHor;              ///< Minimal allowed zoom for display projections without margins ("optimal" theta angle).
    float viewOffsetPhi;                        ///< Phi rotation of camera/projection with respect to panorama sphere.
    float viewOffsetTheta;                      ///< Theta rotation of camera/projection with respect to panorama sphere.
    //
    sf::Vector2i displaySize;                   ///< Target size for the rectilinear display projection.
    sf::Vector2f displayFOV;                    ///< Field of view covered by projection (depends on 'displaySize' aspect ratio and 'zoom').
    std::vector<sf::Uint8> displayData;         ///< Data buffer for display projection.
    //
    std::vector<float> staticDisplayTrafosX;    ///< Cache for view angle-indep. part of horizontal trafo from display pos. to pano. sphere.
    std::vector<float> staticDisplayTrafosY;    ///< Cache for view angle-indep. part of vertical trafo from display pos. to pano. sphere.
    //
    sf::Vector2i panoSphereSize;                ///< Image size of the panorama sphere.
    std::vector<sf::Uint8> panoSphereData;      ///< Data buffer for the panorama sphere.
    //
    const float panoSphereRemapHystMinOvers;    ///< Min. projection oversampling thresh. (increase pano. sphere resolution when zoom in more).
    const float panoSphereRemapHystTargOvers;   ///< Target projection oversampling (try reach this value when adjusting pano. sphere resol.).
    const float panoSphereRemapHystMaxOvers;    ///< Max. projection oversampling thresh. (decrease pano. sphere resolution when zoom out more).
    float panoSphereRemapHystMaxF;      ///< Max. f beyond which oversampl. cannot be restored by pano. sphere re-calc. (limited picture res.).
    //
    std::thread updateDisplayThread;                ///< Thread to update display projection to current perspective when triggered to do so.
    std::atomic_bool stopUpdateDisplayThread;       ///< Flag to stop the display projection updater thread.
    std::mutex updateDisplayMutex;                  ///< Mutex for synchronization between updateDisplayData() and updateDisplayDataLoop().
    std::condition_variable updateDisplayCondVar;   ///< Cond. var. for synchronization between updateDisplayData() and updateDisplayDataLoop().
    bool startUpdateDisplay;                        ///< Flag for triggering one update cycle of updateDisplayDataLoop().
    bool updateDisplayFinished;                 ///< Flag for signalling to updateDisplayData() that the triggered update cycle has finished.
};

#endif // SPNV_PROJECTOR_H
