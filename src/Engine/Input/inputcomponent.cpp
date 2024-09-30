#include "inputcomponent.h"

#include "../UI/uisystem.h"

#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <SDL2/SDL.h>
#include <SDL_opengles2.h>
// #include <SDL_opengl_glext.h>
// #include <GLES2/gl2.h>
// #include <GLFW/glfw3.h>
#else
    #ifdef __linux__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_opengl.h>
    #elif _WIN32
    #include <SDL.h>
    #include <SDL_opengl.h>
    #endif
#endif

#include "serialization.h"

namespace pg
{
    namespace
    {
    }

    template<>
    void serialize(Archive& archive, const MouseLeftClickComponent& component)
    {
        archive.startSerialization("Mouse Left Click Component");

        component.callback->serialize(archive);

        archive.endSerialization();
    }

    void MouseClickSystem::init()
    {
        LOG_THIS_MEMBER("MouseClickSystem");

        pressedList.emplace(SDL_BUTTON_LEFT, false);
        pressedList.emplace(SDL_BUTTON_RIGHT, false);

        auto leftGroup = registerGroup<UiComponent, MouseLeftClickComponent>();

        // Todo change this (useless and error prone, just loop over the group view)
        // To do this change we need to make the view orderable (maybe more complicated than expected)
        leftGroup->addOnGroup([this](EntityRef entity) {
            LOG_MILE("MouseLeftClickSystem", "Add entity " << entity->id << " to ui - mouse left click group !");

            const auto& ui = entity->get<UiComponent>();
            auto mouse = entity->get<MouseLeftClickComponent>();
            
            if (mouse->trigger == MouseStateTrigger::OnPress)
            {
                mouseLeftAreaPressHolder.emplace(entity->id, ui);
            }
            else if (mouse->trigger == MouseStateTrigger::OnRelease)
            {
                mouseLeftAreaReleaseHolder.emplace(entity->id, ui);
            }
            else
            {
                mouseLeftAreaPressHolder.emplace(entity->id, ui);
                mouseLeftAreaReleaseHolder.emplace(entity->id, ui);
            }
        });

        leftGroup->removeOfGroup([this](EntitySystem*, _unique_id id) {
            LOG_MILE("MouseLeftClickSystem", "Remove entity " << id << " to ui - mouse left click group !");

            const auto& it = std::find_if(mouseLeftAreaPressHolder.begin(), mouseLeftAreaPressHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it != mouseLeftAreaPressHolder.end())
            {
                mouseLeftAreaPressHolder.erase(it);
            }

            const auto& it2 = std::find_if(mouseLeftAreaReleaseHolder.begin(), mouseLeftAreaReleaseHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it2 != mouseLeftAreaReleaseHolder.end())
            {
                mouseLeftAreaReleaseHolder.erase(it2);
            }
        });

        auto rightGroup = registerGroup<UiComponent, MouseRightClickComponent>();

        rightGroup->addOnGroup([this](EntityRef entity) {
            LOG_MILE("MouseRightClickSystem", "Add entity " << entity->id << " to ui - mouse right click group !");

            auto ui = entity->get<UiComponent>();
            auto mouse = entity->get<MouseRightClickComponent>();
            
            if (mouse->trigger == MouseStateTrigger::OnPress)
            {
                mouseRightAreaPressHolder.emplace(entity->id, ui);
            }
            else if (mouse->trigger == MouseStateTrigger::OnRelease)
            {
                mouseRightAreaReleaseHolder.emplace(entity->id, ui);
            }
            else
            {
                mouseRightAreaPressHolder.emplace(entity->id, ui);
                mouseRightAreaReleaseHolder.emplace(entity->id, ui);
            }
        });

        rightGroup->removeOfGroup([this](EntitySystem*, _unique_id id) {
            LOG_MILE("MouseRightClickSystem", "Remove entity " << id << " to ui - mouse right click group !");

            const auto& it = std::find_if(mouseRightAreaPressHolder.begin(), mouseRightAreaPressHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it != mouseRightAreaPressHolder.end())
            {
                mouseRightAreaPressHolder.erase(it);
            }

            const auto& it2 = std::find_if(mouseRightAreaReleaseHolder.begin(), mouseRightAreaReleaseHolder.end(), [id](const MouseAreaZ& area) { return area.id == id; });

            if (it2 != mouseRightAreaReleaseHolder.end())
            {
                mouseRightAreaReleaseHolder.erase(it2);
            }
        });
    }

    void MouseClickSystem::execute()
    {
        handleClick(SDL_BUTTON_LEFT, mouseLeftAreaPressHolder, mouseLeftAreaReleaseHolder);
        handleClick(SDL_BUTTON_RIGHT, mouseRightAreaPressHolder, mouseRightAreaReleaseHolder);
    }

    void MouseClickSystem::handleClick(const MouseButton& button, const std::set<MouseAreaZ, std::greater<>>& pressAreas, const std::set<MouseAreaZ, std::greater<>>& releaseAreas)
    {
        int highestZ = INT_MIN;
        const auto& mousePos = inputHandler->getMousePos();

        if (oldMousePos.x != mousePos.x or oldMousePos.y != mousePos.y)
        {
            ecsRef->sendEvent(OnMouseMove{mousePos, inputHandler});
        }

        oldMousePos = mousePos;

        if (inputHandler->isButtonPressed(button))
        {
            if (not pressedList[button])
            {
                ecsRef->sendEvent(OnMouseClick{mousePos, button});
            }

            for (const auto& mouseArea : pressAreas)
            {
                auto ui = mouseArea.ui.component;

                if (ui->pos.z < highestZ)
                    break;

                if (ui->inClipBound(mousePos.x, mousePos.y))
                {
                    highestZ = ui->pos.z;

                    callCallback(button, mouseArea.id);
                }
            }

            pressedList[button] = true;
        }

        highestZ = INT_MIN;

        if (not inputHandler->isButtonPressed(button))
        {
            if (pressedList[button])
            {
                for (const auto& mouseArea : releaseAreas)
                {
                    auto ui = mouseArea.ui.component;

                    if (ui->pos.z < highestZ)
                        break;

                    if (ui->inClipBound(mousePos.x, mousePos.y))
                    {
                        highestZ = ui->pos.z;

                        callCallback(button, mouseArea.id);
                    }
                }
            }

            pressedList[button] = false;
        }
    }
    
    void MouseClickSystem::callCallback(const MouseButton& button, _unique_id id)
    {
        if (button == SDL_BUTTON_LEFT)
        {
            auto comp = static_cast<Own<MouseLeftClickComponent>*>(this)->getComponent(id);

            comp->callback->call(world());
        }
        else if (button == SDL_BUTTON_RIGHT)
        {
            auto comp = static_cast<Own<MouseRightClickComponent>*>(this)->getComponent(id);

            comp->callback->call(world());
        }
        else
        {
            LOG_ERROR("MouseClickSystem", "Unknown mouse button pressed: " << button);
        }
    }

    bool operator<(MouseAreaZ lhs, MouseAreaZ rhs)
    {
        const auto& z = lhs.ui->pos.z;
        const auto& rhsZ = rhs.ui->pos.z;

        if (z == rhsZ)
            return lhs.id < rhs.id;
        else
            return z < rhsZ;
    }

    bool operator>(MouseAreaZ lhs, MouseAreaZ rhs)
    {
        const auto& z = lhs.ui->pos.z;
        const auto& rhsZ = rhs.ui->pos.z;

        if (z == rhsZ)
            return lhs.id > rhs.id;
        else
            return z > rhsZ;
    }
}