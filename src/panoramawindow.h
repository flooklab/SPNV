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

#ifndef SPNV_PANORAMAWINDOW_H
#define SPNV_PANORAMAWINDOW_H

#include "projector.h"
#include "scenemetadata.h"

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <memory>
#include <string>

/*!
 * \brief Display panorama scenes on screen from variable perspectives.
 *
 * Displays panorama scenes that are defined by a loaded panorama picture and corresponding SceneMetaData in a window (see run()).
 * A panorama scene can be shown from different perspectives (view angle, zoom), which is controlled by user input via mouse
 * or keyboard. To obtain a flat rectangular image to display on screen for every perspective a rectilinear projection of
 * the panorama sphere is used. The window that displays the scene is created by an sf::RenderWindow from the SFML library.
 *
 * Processing user input and displaying the current perspective in the window is handled by this class,
 * while Projector is used to load the picture and transform it to different perspectives.
 *
 * A single PanoramaWindow can be used to subsequently display different panorama scenes
 * as the used window and Projector instances will be dynamically created by run().
 */
class PanoramaWindow
{
public:
    PanoramaWindow();                           ///< Constructor.
    //
    bool run(const std::string& pFileName, const SceneMetaData& pSceneMetaData);    ///< Display a picture as panorama scene in a window.

private:
    void createWindow(bool pFullscreenMode);    ///< Create a new window or recreate the old window.
    //
    void updateWindowTitle();                   ///< Update the window title with current file name and zoom level.
    //
    void updateDisplaySize();                   ///< Adjust window settings and Projector projection to current window resolution.
    //
    void zoomIn();                              ///< Zoom into the scene.
    void zoomOut();                             ///< Zoom out of the scene.
    //
    void renderPanoramaView();                  ///< Draw the current scene projection.

private:
    sf::RenderWindow window;                ///< Window used to display the panorama scene.
    //
    std::string fileName;                   ///< File name of current panorama picture.
    //
    sf::Texture panoTexture;                ///< Texture used to draw the panorama scene.
    sf::Sprite panoSprite;                  ///< Sprite used to draw the panorama scene.
    //
    std::unique_ptr<Projector> projector;   ///< Projector for picture loading, perspective transformation and display projection.
    //
    bool mouseDragLockThetaAngle;           ///< Lock the vertical view angle during mouse drag.
};

#endif // SPNV_PANORAMAWINDOW_H
