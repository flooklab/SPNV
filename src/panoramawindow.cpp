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

#include "constants.h"
#include "version.h"

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/WindowStyle.hpp>

#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

/*!
 * \brief Constructor.
 *
 * Note: Does \e not create a window yet. Use run() to create a window for displaying a panorama scene.
 */
PanoramaWindow::PanoramaWindow() :
    window(),
    //
    fileName(""),
    //
    panoTexture(),
    panoSprite(),
    //
    projector(nullptr),
    //
    mouseDragLockThetaAngle(false)
{
}

//Public

/*!
 * \brief Display a picture as panorama scene in a window.
 *
 * Creates a new window to display the panorama scene shown in picture \p pFileName. Required meta information
 * about the scene (such as field of view etc.) is taken from \p pSceneMetaData (see SceneMetaData) and passed
 * to Projector, which takes care of all the graphics transformations (see also Projector()).
 *
 * The scene perspective can be changed via mouse or keyboard in the following ways:
 *
 * - Mouse drag (click + move): Self-explanatory.
 * - Mouse scroll or plus and minus keys: Zoom in and out (and move vertical view angle if necessary to avoid margins).
 * - Arrow keys left/right/up/down: Move view angle left/right/up/down about 5 degrees.
 * - Space key: Vertically center horizon line and adjust zoom if necessary to avoid margins.
 * - CTRL+'0': Vertically center horizon line and adjust zoom to minimum possible one given the centered horizon.
 * - '0': Adjust zoom to minimum possible and move vertical view angle if necessary to avoid margins (then possibly non-centered horizon).
 * - 'H': Adjust zoom for a horizontal field of view of 65 degrees (arbitrary but fixed value).
 * - 'V': Adjust zoom for a vertical field of view of 45 degrees (arbitrary but fixed value).
 * - 'L': Toggles whether the vertical view angle is locked while using the mouse drag feature (default: not locked).
 *
 * The screen is re-drawn using an updated display projection after every of the aforementioned
 * movements/changes as well as every time the window is resized (which keeps the vertical field
 * of view constant and adjusts the horizontal field of view according to the new aspect ratio).
 *
 * The window can be closed again via your preferred operating system functions or by pressing CTRL+'W'.
 *
 * Fullscreen mode of the window can be toggled by pressing 'F' or F11.
 *
 * \param pFileName Panorama picture to load.
 * \param pSceneMetaData Meta data for panorama scene from \p pFileName.
 * \param pFullscreenMode Immediately start in fullscreen mode.
 * \return If could successfully load \p pFileName (and picture dimensions match information from \p pSceneMetaData).
 */
bool PanoramaWindow::run(const std::string& pFileName, const SceneMetaData& pSceneMetaData, const bool pFullscreenMode)
{
    //Create a new projector for the current panorama scene
    try
    {
        projector = std::make_unique<Projector>(pFileName, pSceneMetaData);
    }
    catch (const std::runtime_error& exc)
    {
        std::cerr<<"ERROR: "<<exc.what()<<std::endl;
        return false;
    }

    fileName = pFileName;

    //Setup a window

    bool fullscreenMode = pFullscreenMode;

    createWindow(fullscreenMode);

    sf::Vector2u currentWindowSize = window.getSize();

    //Initialize panorama scene with current window size and reset perspective

    updateDisplaySize();

    projector->updateView(0, 0, 0);

    renderPanoramaView();

    //Set proper window title
    updateWindowTitle();

    //Event loop control flags for performance-intensive "continuous" user interactions (see below)
    bool windowResizing = false;
    bool mouseDragging = false;

    //Helper variables for view angle manipulation via mouse drag
    sf::Vector2f dragInitialMouseAngle;
    sf::Vector2i dragCurrentMousePos;
    sf::Vector2i dragLastMousePos;
    double dragInitialViewOffsetTheta = 0;
    double dragInitialViewOffsetPhi = 0;
    bool dragWaitForWrap = false;   //Skip mouse move events until current event's mouse position matches set mouse position again

    //Lambda for enabling view angle manipulation via mouse drag
    auto enableMouseDragging = [&]() -> void
    {
        //Set mouse drag flag
        mouseDragging = true;

        //Remember current perspective, current mouse position and associated view angle in order to
        //be able to calculate and set a relative perspective change from a changing mouse position

        dragCurrentMousePos = sf::Mouse::getPosition(window);
        dragLastMousePos = dragCurrentMousePos;

        dragInitialMouseAngle = projector->getViewAngle(dragCurrentMousePos);

        dragInitialViewOffsetPhi = projector->getOffsetPhi();
        dragInitialViewOffsetTheta = projector->getOffsetTheta();
    };

    //Reset locked theta angle mouse drag mode
    mouseDragLockThetaAngle = false;

    sf::Event event;

    while (window.isOpen())
    {
        /*
         * Wait for window events (i.e. user interaction) in a loop and process all of them in series.
         * Depending on control flags, either use
         * - blocking wait for "normal" operation to reduce CPU load from top-level while loop, or
         * - non-blocking wait when "continuous" user interactions are going on, because actual logic
         *   for those is done below the event loop (hence need to exit the loop) in order to skip
         *   unnecessary, expensive recalculations for each of many consecutive events.
         */
        while ((!(mouseDragging || windowResizing) && window.waitEvent(event)) ||
               ( (mouseDragging || windowResizing) && window.pollEvent(event)))
        {
            switch (event.type)
            {
                case sf::Event::Closed:
                {
                    window.close();
                    break;
                }
                case sf::Event::Resized:
                {
                    //Do not resize scene here, but set resize flag so that resizing will be handled once below the event loop after all
                    //pending events have been processed (this ignores/groups consecutive resize events of a single resize operation)
                    windowResizing = true;
                    break;
                }
                case sf::Event::MouseWheelScrolled:
                {
                    if (event.mouseWheelScroll.delta > 0)
                        zoomIn();
                    else if (event.mouseWheelScroll.delta < 0)
                        zoomOut();

                    break;
                }
                case sf::Event::MouseButtonPressed:
                {
                    //Enable view angle manipulation via mouse movement, which will be handled below the event loop each time when all
                    //pending events have been processed (this groups/ignores small consecutive move events from continuous mouse movement)
                    if (event.mouseButton.button == sf::Mouse::Button::Left)
                        enableMouseDragging();
                    break;
                }
                case sf::Event::MouseButtonReleased:
                {
                    //Disable view angle manipulation via mouse movement again
                    if (event.mouseButton.button == sf::Mouse::Button::Left)
                        mouseDragging = false;
                    break;
                }
                case sf::Event::MouseMoved:
                {
                    //Capture current mouse position if view angle manipulation via mouse movement is enabled;
                    //actual movement logic happens below the event loop
                    if (mouseDragging)
                    {
                        //After the mouse left a window edge and hence was reset to the opposite edge, need to skip
                        //pending mouse move events until current event's mouse position matches set mouse position again
                        if (dragWaitForWrap)
                        {
                            if (dragCurrentMousePos.x == event.mouseMove.x && dragCurrentMousePos.y == event.mouseMove.y)
                                dragWaitForWrap = false;
                            else
                                break;
                        }

                        dragCurrentMousePos.x = event.mouseMove.x;
                        dragCurrentMousePos.y = event.mouseMove.y;
                    }
                    break;
                }
                case sf::Event::KeyPressed:
                {
                    switch (event.key.code)
                    {
                        case sf::Keyboard::Key::Left:
                        {
                            //Turn perspective to the left (fixed step size)
                            projector->updateView(projector->getZoom(),
                                                  projector->getOffsetPhi() - 5.*Constants::pi/180.,
                                                  projector->getOffsetTheta());
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::Right:
                        {
                            //Turn perspective to the right (fixed step size)
                            projector->updateView(projector->getZoom(),
                                                  projector->getOffsetPhi() + 5.*Constants::pi/180.,
                                                  projector->getOffsetTheta());
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::Up:
                        {
                            //Turn perspective upwards (fixed step size)
                            projector->updateView(projector->getZoom(),
                                                  projector->getOffsetPhi(),
                                                  projector->getOffsetTheta() - 5*Constants::pi/180.);
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::Down:
                        {
                            //Turn perspective downwards (fixed step size)
                            projector->updateView(projector->getZoom(),
                                                  projector->getOffsetPhi(),
                                                  projector->getOffsetTheta() + 5*Constants::pi/180.);
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::Space:
                        {
                            //Center the horizon line and update window title as this might change zoom level
                            projector->centerHorizon();
                            updateWindowTitle();
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::Add:
                        {
                            zoomIn();
                            break;
                        }
                        case sf::Keyboard::Key::Subtract:
                        {
                            zoomOut();
                            break;
                        }
                        case sf::Keyboard::Key::Num0:
                        case sf::Keyboard::Key::Numpad0:
                        {
                            //Reset to minimum possible zoom level (!CTRL) or center horizon and reset to minimum possible zoom level
                            //that can just preserve the centered horizon (CTRL); update window title for resulting zoom level
                            if (event.key.control)
                                projector->updateView(-1, projector->getOffsetPhi(), 0);
                            else
                                projector->updateView(0, projector->getOffsetPhi(), projector->getOffsetTheta());

                            updateWindowTitle();
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::H:
                        {
                            //Adjust zoom so that horizontal field of view is 65 degrees; update window title for changed zoom level
                            projector->updateView(projector->getRequiredZoomFromHFOV(65.f * Constants::pi / 180.f),
                                                  projector->getOffsetPhi(), projector->getOffsetTheta());
                            updateWindowTitle();
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::V:
                        {
                            //Adjust zoom so that vertical field of view is 45 degrees; update window title for changed zoom level
                            projector->updateView(projector->getRequiredZoomFromVFOV(45.f * Constants::pi / 180.f),
                                                  projector->getOffsetPhi(), projector->getOffsetTheta());
                            updateWindowTitle();
                            renderPanoramaView();
                            break;
                        }
                        case sf::Keyboard::Key::L:
                        {
                            //Toggle locked theta angle mouse drag mode
                            mouseDragLockThetaAngle = !mouseDragLockThetaAngle;
                            updateWindowTitle();
                            break;
                        }
                        case sf::Keyboard::Key::F:
                        case sf::Keyboard::Key::F11:
                        {
                            //Toggle fullscreen mode
                            fullscreenMode = !fullscreenMode;

                            //Create new window
                            createWindow(fullscreenMode);

                            //Update window size
                            currentWindowSize = window.getSize();

                            //Update title and display
                            updateWindowTitle();
                            renderPanoramaView();

                            break;
                        }
                        case sf::Keyboard::Key::W:
                        {
                            if (event.key.control)
                                window.close();
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        //Short sleep and continue if nothing to do in order to reduce CPU load
        if (!windowResizing && !(mouseDragging && (dragCurrentMousePos != dragLastMousePos)))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{2});
            continue;
        }

        //If at least one window resize event was collected in the event loop above, resize the panorama scene now
        if (windowResizing)
        {
            windowResizing = false;

            currentWindowSize = window.getSize();

            updateDisplaySize();

            renderPanoramaView();
        }

        //If view angle manipulation via mouse drag is active, change scene perspective according to initial (at mouse
        //drag activation) and current mouse position so that mouse pointer stays aligned with same spot in the scene
        if (mouseDragging && (dragCurrentMousePos != dragLastMousePos))
        {
            //Calculate relative movement of mouse position between start of mouse drag and now in terms of panorama sphere angles

            sf::Vector2f dragCurrentMouseAngle = projector->getViewAngle(dragCurrentMousePos);

            float deltaPhi = dragInitialMouseAngle.x - dragCurrentMouseAngle.x;
            float deltaTheta = dragInitialMouseAngle.y - dragCurrentMouseAngle.y;

            if (mouseDragLockThetaAngle)
                deltaTheta = 0;

            //Move the perspective about the same relative view angle
            projector->updateView(projector->getZoom(), dragInitialViewOffsetPhi + deltaPhi, dragInitialViewOffsetTheta + deltaTheta);

            renderPanoramaView();

            //If the mouse leaves a window edge while dragging, move the mouse to the opposite
            //edge and reset the dragging origin in order to allow for a continuous movement
            if (dragCurrentMousePos.x <= 0 || static_cast<unsigned int>(dragCurrentMousePos.x) >= currentWindowSize.x-1 ||
                    dragCurrentMousePos.y <= 0 || static_cast<unsigned int>(dragCurrentMousePos.y) >= currentWindowSize.y-1)
            {
                sf::Vector2i mouseOffs;

                if (dragCurrentMousePos.x <= 0)
                    mouseOffs.x = static_cast<int>(currentWindowSize.x)-2;
                else if (static_cast<unsigned int>(dragCurrentMousePos.x) >= currentWindowSize.x-1)
                    mouseOffs.x = -static_cast<int>(currentWindowSize.x)+2;

                if (dragCurrentMousePos.y <= 0)
                    mouseOffs.y = static_cast<int>(currentWindowSize.y)-2;
                else if (static_cast<unsigned int>(dragCurrentMousePos.y) >= currentWindowSize.y-1)
                    mouseOffs.y = -static_cast<int>(currentWindowSize.y)+2;

                sf::Mouse::setPosition(dragCurrentMousePos + mouseOffs, window);

                //Reset dragging origin
                enableMouseDragging();

                //Skip pending mouse move events until event triggered by sf::Mouse::setPosition is reached
                dragWaitForWrap = true;
            }

            dragLastMousePos = dragCurrentMousePos;
        }
    }

    //Delete the projector
    projector.reset();

    return true;
}

//Private

/*!
 * \brief Create a new window or recreate the old window.
 *
 * Enables vertical synchronization.
 *
 * Enables fullscreen mode if this is supported and \p pFullscreenMode is true.
 *
 * \param pFullscreenMode Use fullscreen mode, if available.
 */
void PanoramaWindow::createWindow(bool pFullscreenMode)
{
    sf::VideoMode videoMode = sf::VideoMode::getDesktopMode();

    if (pFullscreenMode && sf::VideoMode::getFullscreenModes().size() > 0)
        videoMode = sf::VideoMode::getFullscreenModes()[0];
    else
        pFullscreenMode = false;

    window.create(videoMode, "", pFullscreenMode ? sf::Style::Fullscreen : sf::Style::Default);

    window.setVerticalSyncEnabled(true);
}

/*!
 * \brief Update the window title with current file name and zoom level.
 *
 * Sets the window title according to the current file name and zoom level.
 * Appends an 'L' to the title if the "lock vertical view angle during mouse drag" mode is active.
 *
 * Note: The shown zoom level is a percentage measured relative to the minimal possible zoom
 * that just fits the vertical window field of view. See also Projector::getNormalizedZoom().
 *
 * Note: Returns immediately, if no projector is defined (no panorama window running (see run()).
 */
void PanoramaWindow::updateWindowTitle()
{
    if (!projector)
        return;

    window.setTitle(std::string(Version::programName) + " " + Version::toString() + " - \"" + fileName + "\" - " +
                    std::to_string(std::lroundf(100*projector->getNormalizedZoom())) + "%" + (mouseDragLockThetaAngle ? " L" : ""));
}

//

/*!
 * \brief Adjust window settings and Projector projection to current window resolution.
 *
 * Updates Projector to use the current window resolution in order to generate properly sized display projections.
 * Changes size of window's view frame and of the texture used to display the scene accordingly.
 *
 * See also Projector::updateDisplaySize().
 *
 * Note: Returns immediately, if no projector is defined (no panorama window running (see run()).
 */
void PanoramaWindow::updateDisplaySize()
{
    if (!projector)
        return;

    //Need to explicitly update resolution of displayed window content
    sf::View view({0, 0, static_cast<float>(window.getSize().x), static_cast<float>(window.getSize().y)});
    window.setView(view);

    projector->updateDisplaySize(window.getSize());

    panoTexture.create(window.getSize().x, window.getSize().y);
}

//

/*!
 * \brief Zoom into the scene.
 *
 * Increases the focal length of the "virtual camera" by a factor of 1.1,
 * which changes the field of view by approximately the same factor:
 *
 * FOV / 2 = atan(tan(FOV / 2) / factor) ~ (FOV / 2) / factor
 *
 * See also Projector::updateView().
 *
 * Note: Returns immediately, if no projector is defined (no panorama window running (see run()).
 */
void PanoramaWindow::zoomIn()
{
    if (!projector)
        return;

    //Zoom in
    projector->updateView(projector->getZoom() * 1.1, projector->getOffsetPhi(), projector->getOffsetTheta());

    //Update display
    updateWindowTitle();
    renderPanoramaView();
}

/*!
 * \brief Zoom out of the scene.
 *
 * Same as zoomIn() with a factor of 1/1.1 instead.
 *
 * Note: Returns immediately, if no projector is defined (no panorama window running (see run()).
 */
void PanoramaWindow::zoomOut()
{
    if (!projector)
        return;

    //Zoom out
    projector->updateView(projector->getZoom() / 1.1, projector->getOffsetPhi(), projector->getOffsetTheta());

    //Update display
    updateWindowTitle();
    renderPanoramaView();
}

//

/*!
 * \brief Draw the current scene projection.
 *
 * Gets the current display projection from Projector::getDisplayData(),
 * updates the corresponding texture and displays it in the window.
 *
 * Note: Returns immediately, if no projector is defined (no panorama window running (see run()).
 */
void PanoramaWindow::renderPanoramaView()
{
    if (!projector)
        return;

    //Load current projection data into texture and update sprite accordingly
    panoTexture.update(projector->getDisplayData().data());
    panoSprite.setTexture(panoTexture, true);

    //Display the scene

    window.clear();

    window.draw(panoSprite);

    window.display();
}
