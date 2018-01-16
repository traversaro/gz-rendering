/*
 * Copyright (C) 2015 Open Source Robotics Foundation
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

#include "ignition/rendering/ogre/OgreCamera.hh"
#include "ignition/rendering/ogre/OgreConversions.hh"
#include "ignition/rendering/ogre/OgreIncludes.hh"
#include "ignition/rendering/ogre/OgreRenderTarget.hh"
#include "ignition/rendering/ogre/OgreScene.hh"

using namespace ignition;
using namespace rendering;

//////////////////////////////////////////////////
OgreCamera::OgreCamera()
{
}

//////////////////////////////////////////////////
OgreCamera::~OgreCamera()
{
}

//////////////////////////////////////////////////
void OgreCamera::SetHFOV(const math::Angle &_angle)
{
  BaseCamera::SetHFOV(_angle);
  double hfov = _angle.Radian();
  double vfov = 2.0 * atan(tan(hfov / 2.0) / this->aspect);
  this->ogreCamera->setFOVy(Ogre::Radian(vfov));
}

//////////////////////////////////////////////////
double OgreCamera::AspectRatio() const
{
  return this->ogreCamera->getAspectRatio();
}

//////////////////////////////////////////////////
void OgreCamera::SetAspectRatio(const double _ratio)
{
  BaseCamera::SetAspectRatio(_ratio);
  return this->ogreCamera->setAspectRatio(_ratio);
}

//////////////////////////////////////////////////
unsigned int OgreCamera::AntiAliasing() const
{
  return this->renderTexture->AntiAliasing();
}

//////////////////////////////////////////////////
void OgreCamera::SetAntiAliasing(const unsigned int _aa)
{
  BaseCamera::SetAntiAliasing(_aa);
  this->renderTexture->SetAntiAliasing(_aa);
}

//////////////////////////////////////////////////
math::Color OgreCamera::BackgroundColor() const
{
  return this->renderTexture->BackgroundColor();
}

//////////////////////////////////////////////////
void OgreCamera::SetBackgroundColor(const math::Color &_color)
{
  this->renderTexture->SetBackgroundColor(_color);
}

//////////////////////////////////////////////////
void OgreCamera::Render()
{
  this->renderTexture->Render();
}

//////////////////////////////////////////////////
RenderTargetPtr OgreCamera::RenderTarget() const
{
  return this->renderTexture;
}

//////////////////////////////////////////////////
ScenePtr OgreCamera::GetScene() const
{
  return this->scene;
}

//////////////////////////////////////////////////
VisualPtr OgreCamera::VisualAt(const ignition::math::Vector2i &_mousePos)
{
  ignition::math::Vector3d origin;
  ignition::math::Vector3d dir;
  this->CameraToViewportRay(_mousePos.X(), _mousePos.Y(), origin, dir);
  return this->scene->VisualAt(origin, dir);
}

//////////////////////////////////////////////////
void OgreCamera::Init()
{
  BaseCamera::Init();
  this->CreateCamera();
  this->CreateRenderTexture();
  this->Reset();
}

//////////////////////////////////////////////////
void OgreCamera::CreateCamera()
{
  // create ogre camera object
  Ogre::SceneManager *ogreSceneManager;
  ogreSceneManager = this->scene->OgreSceneManager();
  this->ogreCamera = ogreSceneManager->createCamera(this->name);
  this->ogreNode->attachObject(this->ogreCamera);

  // rotate to Gazebo coordinate system
  this->ogreCamera->yaw(Ogre::Degree(-90.0));
  this->ogreCamera->roll(Ogre::Degree(-90.0));
  this->ogreCamera->setFixedYawAxis(false);

  // TODO: provide api access
  this->ogreCamera->setAutoAspectRatio(true);
  this->ogreCamera->setRenderingDistance(0);
  this->ogreCamera->setPolygonMode(Ogre::PM_SOLID);
  this->ogreCamera->setProjectionType(Ogre::PT_PERSPECTIVE);
  this->ogreCamera->setCustomProjectionMatrix(false);
}

//////////////////////////////////////////////////
void OgreCamera::CreateRenderTexture()
{
  RenderTexturePtr base = this->scene->CreateRenderTexture();
  this->renderTexture = std::dynamic_pointer_cast<OgreRenderTexture>(base);
  this->renderTexture->SetCamera(this->ogreCamera);
  this->renderTexture->SetFormat(PF_R8G8B8);
  this->renderTexture->SetBackgroundColor(this->scene->BackgroundColor());
}

//////////////////////////////////////////////////
void OgreCamera::CameraToViewportRay(const int _screenx, const int _screeny,
    ignition::math::Vector3d &_origin,
    ignition::math::Vector3d &_dir) const
{
  Ogre::Ray ray = this->ogreCamera->getCameraToViewportRay(
      static_cast<float>(_screenx) / this->ogreCamera->getViewport()->getActualWidth(),
      static_cast<float>(_screeny) / this->ogreCamera->getViewport()->getActualHeight());

  _origin.Set(ray.getOrigin().x, ray.getOrigin().y, ray.getOrigin().z);
  _dir.Set(ray.getDirection().x, ray.getDirection().y, ray.getDirection().z);
}

//////////////////////////////////////////////////
RenderWindowPtr OgreCamera::CreateRenderWindow()
{
  RenderWindowPtr base = this->scene->CreateRenderWindow();
  OgreRenderWindowPtr renderWindow =
      std::dynamic_pointer_cast<OgreRenderWindow>(base);
  renderWindow->SetWidth(this->ImageWidth());
  renderWindow->SetHeight(this->ImageHeight());
  renderWindow->SetDevicePixelRatio(1);
  renderWindow->SetCamera(this->ogreCamera);
  renderWindow->SetBackgroundColor(this->scene->BackgroundColor());

  //this->renderTexture = renderWindow;
  return base;
}

//////////////////////////////////////////////////
math::Matrix4d OgreCamera::ProjectionMatrix() const
{
  return OgreConversions::Convert(this->ogreCamera->getProjectionMatrix());
}

//////////////////////////////////////////////////
math::Matrix4d OgreCamera::ViewMatrix() const
{
  return OgreConversions::Convert(this->ogreCamera->getViewMatrix(true));
}

//////////////////////////////////////////////////
void OgreCamera::SetNearClipPlane(const double _near)
{
  // this->nearClip = _near;
  BaseCamera::SetNearClipPlane(_near);
  this->ogreCamera->setNearClipDistance(_near);
}

//////////////////////////////////////////////////
void OgreCamera::SetFarClipPlane(const double _far)
{
  BaseCamera::SetFarClipPlane(_far);
  this->ogreCamera->setFarClipDistance(_far);
}
