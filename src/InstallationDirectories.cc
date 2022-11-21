/*
 * Copyright (C) 2022 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <ignition/rendering/config.hh>
#include <ignition/rendering/InstallationDirectories.hh>

#include <filesystem>

namespace ignition
{
namespace rendering
{
inline namespace IGNITION_RENDERING_VERSION_NAMESPACE {

std::string getResourcePath()
{
  std::filesystem::path relative_resource_path(IGN_RENDERING_RELATIVE_RESOURCE_PATH);
  std::filesystem::path install_prefix(getInstallPrefix());
  std::filesystem::path resource_path = install_prefix / relative_resource_path;
  return resource_path.string();
}

std::string getEngineInstallDir()
{
  std::filesystem::path engine_relative_install_dir(IGNITION_RENDERING_ENGINE_RELATIVE_INSTALL_DIR);
  std::filesystem::path install_prefix(getInstallPrefix());
  std::filesystem::path engine_install_dir = install_prefix / engine_relative_install_dir;
  return engine_install_dir.string();
}

}
}
}
