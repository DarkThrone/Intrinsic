// Copyright 2016 Benjamin Glatzel
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Precompiled header file
#include "stdafx.h"

#if defined(_INTR_PROFILING_ENABLED)
MICROPROFILE_DEFINE(MAIN, "MAIN", "Main", 0x00ff00);
#endif // _INTR_PROFILING_ENABLED

namespace Intrinsic
{
namespace Core
{
namespace
{
float _stepAccum = 0.0f;
const float _stepSize = 0.016f;
}

// Static members
float TaskManager::_lastDeltaT = 0.016f;
float TaskManager::_totalTimePassed = 0.0f;
uint32_t TaskManager::_frameCounter = 0u;

void TaskManager::executeTasks()
{
  if (_frameCounter > 0u)
  {
    _lastDeltaT = std::min(TimingHelper::timerEnd() * 0.001f, 0.3f);
    _totalTimePassed += _lastDeltaT;

    if (_lastDeltaT < Settings::Manager::_targetFrameRate)
    {
      TimingHelper::timerStart();
      std::this_thread::sleep_for(std::chrono::milliseconds((uint32_t)(
          (Settings::Manager::_targetFrameRate - _lastDeltaT) * 1000.0f)));
      float actualTimeSlept = TimingHelper::timerEnd() * 0.001f;

      _lastDeltaT += actualTimeSlept;
    }
  }
  TimingHelper::timerStart();

  {
#if defined(_INTR_PROFILING_ENABLED)
    MICROPROFILE_SCOPE(MAIN);
#endif // _INTR_PROFILING_ENABLED

    _INTR_PROFILE_CPU("TaskManager", "Execute Tasks");

    {
      _INTR_PROFILE_CPU("TaskManager", "Non-Rendering Tasks");

      // Events and input
      {
        Input::System::reset();
        SystemEventProvider::SDL::pumpEvents();
      }

      // Game state update
      {
        GameStates::Manager::update(_lastDeltaT);
      }

      // Physics
      {
        _INTR_PROFILE_CPU("Physics", "Simulate And Update");

        _stepAccum += _lastDeltaT;

        while (_stepAccum > _stepSize)
        {
          Physics::System::dispatchSimulation(_stepSize);
          Physics::System::syncSimulation();

          _stepAccum -= _stepSize;
        }

        Components::RigidBodyManager::updateNodesFromActors(
            Components::RigidBodyManager::_activeRefs);
        Components::RigidBodyManager::updateActorsFromNodes(
            Components::RigidBodyManager::_activeRefs);
        Physics::System::renderLineDebugGeometry();
      }

      // Post effect system
      {
        Components::PostEffectVolumeManager::blendPostEffects(
            Components::PostEffectVolumeManager::_activeRefs);
      }

      // Fire events
      {
        Resources::EventManager::fireEvents();
      }
    }

    {
      _INTR_PROFILE_CPU("TaskManager", "Rendering Tasks");

      // Rendering
      Renderer::Vulkan::DefaultRenderProcess::renderFrame(_lastDeltaT);
    }

    {
      Physics::System::updatePvdCamera();
    }
  }

  ++_frameCounter;
}
}
}
