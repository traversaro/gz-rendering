/*
 * Copyright (C) 2018 Open Source Robotics Foundation
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

#include <ignition/common/Console.hh>

#include "ignition/rendering/Material.hh"

#include "ignition/rendering/ogre2/Ogre2Includes.hh"
#include "ignition/rendering/ogre2/Ogre2RenderEngine.hh"
#include "ignition/rendering/ogre2/Ogre2RenderPass.hh"
#include "ignition/rendering/ogre2/Ogre2Conversions.hh"
#include "ignition/rendering/ogre2/Ogre2Material.hh"
#include "ignition/rendering/ogre2/Ogre2RenderTarget.hh"
#include "ignition/rendering/ogre2/Ogre2Scene.hh"

namespace ignition
{
namespace rendering
{
inline namespace IGNITION_RENDERING_VERSION_NAMESPACE {
//
/// \brief Listener for changing ogre compositor pass properties
class Ogre2RenderTargetCompositorListener :
    public Ogre::CompositorWorkspaceListener
{
  /// \brief Constructor
  /// \param[in] _target ogre render target object
  public: explicit Ogre2RenderTargetCompositorListener(
      Ogre2RenderTarget *_target)
  {
    this->ogreRenderTarget = _target;
  }

  /// \brief Destructor
  public: virtual ~Ogre2RenderTargetCompositorListener() = default;

  // Documentation inherited.
  public: virtual void passPreExecute(Ogre::CompositorPass *_pass)
  {
    if (_pass->getType() == Ogre::PASS_SCENE)
    {
      Ogre::CompositorPassScene *scenePass =
          static_cast<Ogre::CompositorPassScene *>(_pass);
      IGN_ASSERT(scenePass != nullptr, "Unable to get scene pass");
      Ogre::Viewport *vp = scenePass->getCamera()->getLastViewport();
      if (vp == nullptr) return;
      // make sure we do not alter the reserved visibility flags
      uint32_t f = this->ogreRenderTarget->VisibilityMask() |
          ~Ogre::VisibilityFlags::RESERVED_VISIBILITY_FLAGS;
      // apply the new visibility mask
      uint32_t flags = f & vp->getVisibilityMask();
      vp->_setVisibilityMask(flags, vp->getLightVisibilityMask());
    }
  }

  /// \brief Pointer to render target that added this listener
  private: Ogre2RenderTarget *ogreRenderTarget = nullptr;
};
}
}
}

/// \brief Private data class for Ogre2RenderTarget
class ignition::rendering::Ogre2RenderTargetPrivate
{
  /// \brief Listener for chaning compositor pass properties
  public: Ogre2RenderTargetCompositorListener *rtListener = nullptr;

  /// \brief Name of sky box material
  public: const std::string kSkyboxMaterialName = "SkyBox";

  /// \brief Name of base rendering compositor node
  public: const std::string kBaseNodeName = "PbsMaterialsRenderingNode";

  /// \brief Name of final rendering compositor node
  public: const std::string kFinalNodeName = "FinalComposition";

  /// \brief Name of shadow compositor node
  public: const std::string kShadowNodeName = "PbsMaterialsShadowNode";
};

using namespace ignition;
using namespace rendering;

//////////////////////////////////////////////////
// Ogre2RenderTarget
//////////////////////////////////////////////////
Ogre2RenderTarget::Ogre2RenderTarget()
  : dataPtr(new Ogre2RenderTargetPrivate)
{
  this->ogreBackgroundColor = Ogre::ColourValue::Black;
  this->ogreCompositorWorkspaceDefName = "PbsMaterialsWorkspace";
}

//////////////////////////////////////////////////
Ogre2RenderTarget::~Ogre2RenderTarget()
{
  if (this->dataPtr->rtListener)
  {
    delete this->dataPtr->rtListener;
    this->dataPtr->rtListener = nullptr;
  }
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::BuildCompositor()
{
  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();
  Ogre::CompositorManager2 *ogreCompMgr = ogreRoot->getCompositorManager2();

  this->UpdateBackgroundMaterial();

  bool validBackground = this->backgroundMaterial &&
      !this->backgroundMaterial->EnvironmentMap().empty();

  // The function build a similar compositor as the one defined in
  // ogre2/media/2.0/scripts/Compositors/PbsMaterials.compositor
  // but supports an extra quad pass to render the skybox cubemap if
  // sky is enabled.
  // todo(anyone) Note the definition programmatically created here
  // replaces the one defined in the script so it maybe safe to remove the
  // PbsMaterials.compositor file
  std::string wsDefName = "PbsMaterialWorkspace_" + this->Name();
  this->ogreCompositorWorkspaceDefName = wsDefName;
  if (!ogreCompMgr->hasWorkspaceDefinition(wsDefName))
  {
    // PbsMaterialsRenderingNode
    std::string nodeDefName = wsDefName + "/" + this->dataPtr->kBaseNodeName;
    Ogre::CompositorNodeDef *nodeDef =
        ogreCompMgr->addNodeDefinition(nodeDefName);

    // Input texture
    Ogre::TextureDefinitionBase::TextureDefinition *rt0TexDef =
        nodeDef->addTextureDefinition("rt0");
    rt0TexDef->textureType = Ogre::TextureTypes::Type2D;
    rt0TexDef->width = 0;
    rt0TexDef->height = 0;
    rt0TexDef->depthOrSlices = 1;
    rt0TexDef->numMipmaps = 0;
    rt0TexDef->widthFactor = 1;
    rt0TexDef->heightFactor = 1;
    rt0TexDef->format = Ogre::PFG_RGBA8_UNORM_SRGB;
    rt0TexDef->textureFlags &= ~Ogre::TextureFlags::Uav;
    rt0TexDef->depthBufferId = Ogre::DepthBuffer::POOL_DEFAULT;
    rt0TexDef->depthBufferFormat = Ogre::PFG_UNKNOWN;

    Ogre::RenderTargetViewDef *rtv = nodeDef->addRenderTextureView("rt0");
    rtv->setForTextureDefinition("rt0", rt0TexDef);

    Ogre::TextureDefinitionBase::TextureDefinition *rt1TexDef =
        nodeDef->addTextureDefinition("rt1");
    rt1TexDef->textureType = Ogre::TextureTypes::Type2D;
    rt1TexDef->width = 0;
    rt1TexDef->height = 0;
    rt1TexDef->depthOrSlices = 1;
    rt1TexDef->widthFactor = 1;
    rt1TexDef->heightFactor = 1;
    rt1TexDef->format = Ogre::PFG_RGBA8_UNORM_SRGB;
    rt1TexDef->textureFlags &= ~Ogre::TextureFlags::Uav;
    rt1TexDef->depthBufferId = Ogre::DepthBuffer::POOL_DEFAULT;
    rt1TexDef->depthBufferFormat = Ogre::PFG_UNKNOWN;

    Ogre::RenderTargetViewDef *rtv2 = nodeDef->addRenderTextureView("rt1");
    rtv2->setForTextureDefinition("rt1", rt1TexDef);

    nodeDef->setNumTargetPass(2);
    Ogre::CompositorTargetDef *rt0TargetDef =
        nodeDef->addTargetPass("rt0");

    if (validBackground)
      rt0TargetDef->setNumPasses(3);
    else
      rt0TargetDef->setNumPasses(2);
    {
      // clear pass
      Ogre::CompositorPassClearDef *passClear =
          static_cast<Ogre::CompositorPassClearDef *>(
          rt0TargetDef->addPass(Ogre::PASS_CLEAR));
      passClear->setAllClearColours(this->ogreBackgroundColor);

      if (validBackground)
      {
        // quad pass
        Ogre::CompositorPassQuadDef *passQuad =
            static_cast<Ogre::CompositorPassQuadDef *>(
            rt0TargetDef->addPass(Ogre::PASS_QUAD));
        passQuad->mMaterialName = this->dataPtr->kSkyboxMaterialName + "_"
            + this->Name();
        passQuad->mFrustumCorners =
            Ogre::CompositorPassQuadDef::CAMERA_DIRECTION;
      }

      // scene pass
      Ogre::CompositorPassSceneDef *passScene =
          static_cast<Ogre::CompositorPassSceneDef *>(
          rt0TargetDef->addPass(Ogre::PASS_SCENE));
      passScene->mShadowNode = this->dataPtr->kShadowNodeName;
      passScene->mIncludeOverlays = true;
    }

    nodeDef->mapOutputChannel(0, "rt0");
    nodeDef->mapOutputChannel(1, "rt1");

    // Final Composition
    std::string finalNodeDefName = wsDefName + "/" +
        this->dataPtr->kFinalNodeName;
    Ogre::CompositorNodeDef *finalNodeDef =
        ogreCompMgr->addNodeDefinition(finalNodeDefName);
    finalNodeDef->addTextureSourceName("rt_output", 0,
        Ogre::TextureDefinitionBase::TEXTURE_INPUT);
    finalNodeDef->addTextureSourceName("rtN", 1,
        Ogre::TextureDefinitionBase::TEXTURE_INPUT);

    finalNodeDef->setNumTargetPass(2);
    Ogre::CompositorTargetDef *outTargetDef =
        finalNodeDef->addTargetPass("rt_output");
    outTargetDef->setNumPasses(2);
    {
      // quad pass
      Ogre::CompositorPassQuadDef *passQuad =
          static_cast<Ogre::CompositorPassQuadDef *>(
          outTargetDef->addPass(Ogre::PASS_QUAD));
      passQuad->mMaterialName = "Ogre/Copy/4xFP32";
      passQuad->addQuadTextureSource(0, "rtN");

      // scene pass
      Ogre::CompositorPassSceneDef *passScene =
          static_cast<Ogre::CompositorPassSceneDef *>(
          outTargetDef->addPass(Ogre::PASS_SCENE));
      passScene->mUpdateLodLists = false;
      passScene->mIncludeOverlays = true;
      passScene->mFirstRQ = 254;
      passScene->mLastRQ = 255;
    }

    Ogre::CompositorWorkspaceDef *workDef =
        ogreCompMgr->addWorkspaceDefinition(wsDefName);
    workDef->connectExternal(0, finalNodeDefName, 0);
    workDef->connect(nodeDefName, 0, finalNodeDefName, 1);
  }

  this->ogreCompositorWorkspace =
      ogreCompMgr->addWorkspace(
        this->scene->OgreSceneManager(),
        this->RenderTarget(),
        this->ogreCamera,
        this->ogreCompositorWorkspaceDefName,
        true);

  this->dataPtr->rtListener = new Ogre2RenderTargetCompositorListener(this);
  this->ogreCompositorWorkspace->addListener(this->dataPtr->rtListener);
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::DestroyCompositor()
{
  if (!this->ogreCompositorWorkspace)
    return;

  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();
  Ogre::CompositorManager2 *ogreCompMgr = ogreRoot->getCompositorManager2();
  this->ogreCompositorWorkspace->addListener(nullptr);
  ogreCompMgr->removeWorkspace(this->ogreCompositorWorkspace);
  ogreCompMgr->removeWorkspaceDefinition(this->ogreCompositorWorkspaceDefName);
  ogreCompMgr->removeNodeDefinition(this->ogreCompositorWorkspaceDefName +
      "/" + this->dataPtr->kBaseNodeName);
  ogreCompMgr->removeNodeDefinition(this->ogreCompositorWorkspaceDefName +
      "/" + this->dataPtr->kFinalNodeName);

  this->ogreCompositorWorkspace = nullptr;
  delete this->dataPtr->rtListener;
  this->dataPtr->rtListener = nullptr;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::RebuildCompositor()
{
  this->DestroyCompositor();
  this->BuildCompositor();
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::Copy(Image &_image) const
{
  Ogre::TextureGpu *texture = this->RenderTarget();

  // TODO(anyone) handle Bayer conversions
  // TODO(anyone) handle ogre version differences

  if (_image.Width() != this->width || _image.Height() != this->height)
  {
    ignerr << "Invalid image dimensions" << std::endl;
    return;
  }

  Ogre::Image2 image2;
  image2.convertFromTexture(texture, 0u, 0u);
  Ogre::TextureBox box = image2.getData(0);

  auto dataImage = static_cast<unsigned char *>(_image.Data());
  auto dataImage2 = static_cast<Ogre::uint8 *>(box.data);

  unsigned int channelCount = 3u;
  unsigned int rawChannelCount = 4u;
  unsigned int bytesPerChannel = 1u;
  for (unsigned int row = 0; row < this->height; ++row)
  {
    // the texture box step size could be larger than our image buffer step
    // size
    unsigned int rawDataRowIdx = row * box.bytesPerRow / bytesPerChannel;
    for (unsigned int column = 0; column < this->width; ++column)
    {
      unsigned int idx = (row * this->width * channelCount) +
          column * channelCount;
      unsigned int rawIdx = rawDataRowIdx +
          column * rawChannelCount;

      dataImage[idx] = dataImage2[rawIdx];
      dataImage[idx + 1] = dataImage2[rawIdx + 1];
      dataImage[idx + 2] = dataImage2[rawIdx + 2];
    }
  }
}

//////////////////////////////////////////////////
Ogre::Camera *Ogre2RenderTarget::Camera() const
{
  return this->ogreCamera;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetCamera(Ogre::Camera *_camera)
{
  this->ogreCamera = _camera;
  this->targetDirty = true;
}

//////////////////////////////////////////////////
math::Color Ogre2RenderTarget::BackgroundColor() const
{
  return Ogre2Conversions::Convert(this->ogreBackgroundColor);
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetBackgroundColor(math::Color _color)
{
  this->ogreBackgroundColor = Ogre2Conversions::Convert(_color);
  this->colorDirty = true;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetBackgroundMaterial(MaterialPtr _material)
{
  this->backgroundMaterial = _material;
  this->backgroundMaterialDirty = true;
  this->targetDirty = true;
}

//////////////////////////////////////////////////
MaterialPtr Ogre2RenderTarget::BackgroundMaterial() const
{
  return this->backgroundMaterial;
}

//////////////////////////////////////////////////
unsigned int Ogre2RenderTarget::AntiAliasing() const
{
  return this->antiAliasing;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetAntiAliasing(unsigned int _aa)
{
  this->antiAliasing = _aa;
  this->targetDirty = true;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::PreRender()
{
  BaseRenderTarget::PreRender();
  this->UpdateBackgroundColor();

  if (this->material)
  {
    this->material->PreRender();
  }

  this->UpdateRenderPassChain();
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::PostRender()
{
  // do nothing by default
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::Render()
{
  // TODO(anyone)
  // There is current not an easy solution to manually updating
  // render textures:
  // https://forums.ogre3d.org/viewtopic.php?t=84687
  this->ogreCompositorWorkspace->setEnabled(true);
  auto engine = Ogre2RenderEngine::Instance();
  engine->OgreRoot()->renderOneFrame();
  this->ogreCompositorWorkspace->setEnabled(false);

  // The code below for manual updating render textures was suggested in ogre
  // forum but it does not seem to work
  // this->scene->OgreSceneManager()->updateSceneGraph();
  // this->ogreCompositorWorkspace->_validateFinalTarget();
  // engine->OgreRoot()->getRenderSystem()->_beginFrameOnce();
  // this->ogreCompositorWorkspace->_beginUpdate(false);
  // this->ogreCompositorWorkspace->_update();
  // this->ogreCompositorWorkspace->_endUpdate(false);

  // this->scene->OgreSceneManager()->_frameEnded();
  // for (size_t i=0; i < Ogre::HLMS_MAX; ++i)
  // {
  //   Ogre::Hlms *hlms = engine->OgreRoot()->getHlmsManager()->getHlms(
  //       static_cast<Ogre::HlmsTypes>(i));
  //   if(hlms)
  //     hlms->frameEnded();
  // }
  // engine->OgreRoot()->getRenderSystem()->_update();
}

//////////////////////////////////////////////////
uint32_t Ogre2RenderTarget::VisibilityMask() const
{
  return this->visibilityMask;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetVisibilityMask(uint32_t _mask)
{
  this->visibilityMask = _mask;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::UpdateBackgroundColor()
{
  if (this->colorDirty)
  {
    // set background color in compositor clear pass def
    auto nodeSeq = this->ogreCompositorWorkspace->getNodeSequence();
    auto pass = nodeSeq[0]->_getPasses()[0]->getDefinition();
    auto clearPass = dynamic_cast<const Ogre::CompositorPassClearDef *>(pass);
    const_cast<Ogre::CompositorPassClearDef *>(clearPass)->setAllClearColours(
        this->ogreBackgroundColor);

    this->colorDirty = false;
  }
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::UpdateBackgroundMaterial()
{
  if (!this->backgroundMaterialDirty)
    return;

  bool validBackground = this->backgroundMaterial &&
      !this->backgroundMaterial->EnvironmentMap().empty();

  if (validBackground)
  {
    Ogre::MaterialManager &matManager = Ogre::MaterialManager::getSingleton();
    std::string skyMatName = this->dataPtr->kSkyboxMaterialName + "_"
        + this->Name();
    auto mat = matManager.getByName(skyMatName);
    if (!mat)
    {
      auto skyboxMat = matManager.getByName(this->dataPtr->kSkyboxMaterialName);
      if (!skyboxMat)
      {
        ignerr << "Unable to find skybox material" << std::endl;
        return;
      }
      mat = skyboxMat->clone(skyMatName);
    }
    Ogre::TextureUnitState *texUnit =
        mat->getTechnique(0u)->getPass(0u)->getTextureUnitState(0u);
    texUnit->setTextureName(this->backgroundMaterial->EnvironmentMap(),
        Ogre::TextureTypes::TypeCube);
    texUnit->setHardwareGammaEnabled(false);
  }

  this->backgroundMaterialDirty = false;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::UpdateRenderPassChain()
{
  UpdateRenderPassChain(this->ogreCompositorWorkspace,
      this->ogreCompositorWorkspaceDefName,
      this->ogreCompositorWorkspaceDefName + "/" +
      this->dataPtr->kBaseNodeName,
      this->ogreCompositorWorkspaceDefName + "/" +
      this->dataPtr->kFinalNodeName,
      this->renderPasses, this->renderPassDirty);

  this->renderPassDirty = false;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::UpdateRenderPassChain(
    Ogre::CompositorWorkspace *_workspace,
    const std::string &_workspaceDefName,
    const std::string &_baseNode, const std::string &_finalNode,
    const std::vector<RenderPassPtr> &_renderPasses,
    bool _recreateNodes)
{
  if (!_workspace || _workspaceDefName.empty() ||
      _baseNode.empty() || _finalNode.empty() || _renderPasses.empty())
    return;

  // check pass enabled state and update connections if necessary.
  // If render pass is dirty then skip the enabled state check since the whole
  // workspace nodes and connections will be recreated
  bool updateConnection = false;
  if (!_recreateNodes)
  {
    auto nodeSeq = _workspace->getNodeSequence();

    // set node instance to render pass and update enabled state
    for (const auto &pass : _renderPasses)
    {
      Ogre2RenderPass *ogre2RenderPass =
          dynamic_cast<Ogre2RenderPass *>(pass.get());
      Ogre::CompositorNode *node =
          _workspace->findNodeNoThrow(
          ogre2RenderPass->OgreCompositorNodeDefinitionName());

      // check if we need to create all nodes or just update the connections.
      // if node does not exist then it means it has not been added to the
      // chain yet, in which case, we need to recreate the nodes and
      // connections
      if (!node)
      {
        _recreateNodes = true;
      }
      else if (node && node->getEnabled() != ogre2RenderPass->IsEnabled())
      {
        node->setEnabled(ogre2RenderPass->IsEnabled());
        updateConnection = true;
      }
    }
  }

  if (!_recreateNodes && !updateConnection)
    return;

  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();
  Ogre::CompositorManager2 *ogreCompMgr = ogreRoot->getCompositorManager2();

  Ogre::CompositorWorkspaceDef *workspaceDef =
    ogreCompMgr->getWorkspaceDefinition(_workspaceDefName);

  auto nodeSeq = _workspace->getNodeSequence();

  // The first node and final node in the workspace are defined in
  // PbsMaterials.compositor
  // the first node is the base scene pass node
  std::string outNodeDefName = _baseNode;
  // the final compositor node
  std::string finalNodeDefName = _finalNode;
  std::string inNodeDefName;

  // if new nodes need to be added then clear everything,
  // otherwise clear only the node connections
  if (_recreateNodes)
    workspaceDef->clearAll();
  else
    workspaceDef->clearAllInterNodeConnections();

  // chain the render passes by connecting all the ogre compositor nodes
  // in between the base scene pass node and the final compositor node
  for (const auto &pass : _renderPasses)
  {
    Ogre2RenderPass *ogre2RenderPass =
        dynamic_cast<Ogre2RenderPass *>(pass.get());
    ogre2RenderPass->CreateRenderPass();
    inNodeDefName = ogre2RenderPass->OgreCompositorNodeDefinitionName();
    // only connect passes that are enabled
    if (!inNodeDefName.empty() && ogre2RenderPass->IsEnabled())
    {
      workspaceDef->connect(outNodeDefName, inNodeDefName);
      outNodeDefName = inNodeDefName;
    }
  }

  // connect the last render pass to the final compositor node
  workspaceDef->connect(outNodeDefName, 0,  finalNodeDefName, 1);

  // if new node definitions were added then recreate all the compositor nodes,
  // otherwise update the connections
  if (_recreateNodes)
  {
    // clearAll requires the output to be connected again.
    workspaceDef->connectExternal(0, finalNodeDefName, 0);
    _workspace->recreateAllNodes();
  }
  else
  {
    _workspace->reconnectAllNodes();
  }
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::UpdateShadowNode()
{
  ignwarn << "Ogre2RenderTarget::UpdateShadowNode() is deprecated and "
             "replaced by Ogre2Scene:UpdateShadowNode()"
          << std::endl;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::RebuildImpl()
{
  this->RebuildTarget();
  this->RebuildMaterial();
  this->RebuildCompositor();
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetMaterial(MaterialPtr _material)
{
  this->material = _material;

  // Have to rebuild the target so there is something to apply
  // the applicator to
  this->targetDirty = true;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::SetShadowsNodeDefDirty()
{
  this->DestroyCompositor();

  // Have to rebuild the target so there is something to apply the applicator to
  this->targetDirty = true;
}

//////////////////////////////////////////////////
void Ogre2RenderTarget::RebuildMaterial()
{
  if (this->material)
  {
    Ogre2Material *ogreMaterial = dynamic_cast<Ogre2Material *>(
        this->material.get());
    Ogre::MaterialPtr matPtr = ogreMaterial->Material();

    Ogre::SceneManager *sceneMgr = this->scene->OgreSceneManager();
    this->materialApplicator.reset(new Ogre2RenderTargetMaterial(
        sceneMgr, this->ogreCamera, matPtr.get()));
  }
}

//////////////////////////////////////////////////
// Ogre2RenderTexture
//////////////////////////////////////////////////
Ogre2RenderTexture::Ogre2RenderTexture()
{
}

//////////////////////////////////////////////////
Ogre2RenderTexture::~Ogre2RenderTexture()
{
}

//////////////////////////////////////////////////
void Ogre2RenderTexture::Destroy()
{
  this->DestroyTarget();
}

//////////////////////////////////////////////////
Ogre::TextureGpu *Ogre2RenderTexture::RenderTarget() const
{
  return this->ogreTexture;
}

//////////////////////////////////////////////////
void Ogre2RenderTexture::RebuildTarget()
{
  this->DestroyTarget();
  this->BuildTarget();
}

//////////////////////////////////////////////////
void Ogre2RenderTexture::DestroyTarget()
{
  if (nullptr == this->ogreTexture)
    return;

  Ogre::Root *root = Ogre2RenderEngine::Instance()->OgreRoot();
  Ogre::CompositorManager2 *compositorManager = root->getCompositorManager2();
  if (this->ogreCompositorWorkspace)
  {
    compositorManager->removeWorkspace(
        this->ogreCompositorWorkspace);
    this->ogreCompositorWorkspace = nullptr;
  }

  compositorManager->removeAllNodeDefinitions();
  compositorManager->removeAllWorkspaceDefinitions();

  Ogre::TextureGpuManager *textureManager =
    root->getRenderSystem()->getTextureGpuManager();
  textureManager->destroyTexture(this->ogreTexture);

  this->ogreTexture = nullptr;
}

//////////////////////////////////////////////////
void Ogre2RenderTexture::BuildTarget()
{
  // check if target fsaa is supported
  std::vector<unsigned int> fsaaLevels =
      Ogre2RenderEngine::Instance()->FSAALevels();
  unsigned int targetFSAA = this->antiAliasing;
  auto const it = std::find(fsaaLevels.begin(), fsaaLevels.end(), targetFSAA);

  if (it == fsaaLevels.end())
  {
    // output warning but only do it once
    static bool ogre2FSAAWarn = false;
    if (ogre2FSAAWarn)
    {
      ignwarn << "Anti-aliasing level of '" << this->antiAliasing << "' "
              << "is not supported. Setting to 0" << std::endl;
      ogre2FSAAWarn = true;
    }
  }

  auto engine = Ogre2RenderEngine::Instance();
  auto ogreRoot = engine->OgreRoot();
  Ogre::TextureGpuManager *textureMgr =
    ogreRoot->getRenderSystem()->getTextureGpuManager();
  this->ogreTexture = textureMgr->createOrRetrieveTexture(
    this->name,
    Ogre::GpuPageOutStrategy::SaveToSystemRam,
    Ogre::TextureFlags::RenderToTexture,
    Ogre::TextureTypes::Type2D);

  this->ogreTexture->setResolution(this->width, this->height);
  this->ogreTexture->setNumMipmaps(1u);
  this->ogreTexture->setPixelFormat(Ogre::PFG_RGBA8_UNORM_SRGB);

  this->ogreTexture->scheduleTransitionTo(
    Ogre::GpuResidency::Resident);
}

//////////////////////////////////////////////////
unsigned int Ogre2RenderTexture::GLId() const
{
  if (!this->ogreTexture)
    return 0;

  unsigned int texId;
  this->ogreTexture->getCustomAttribute("msFinalTextureBuffer", &texId);
  return static_cast<unsigned int>(texId);
}

//////////////////////////////////////////////////
void Ogre2RenderTexture::PreRender()
{
  Ogre2RenderTarget::PreRender();
}

//////////////////////////////////////////////////
void Ogre2RenderTexture::PostRender()
{
  Ogre2RenderTarget::PostRender();
}

//////////////////////////////////////////////////
// Ogre2RenderWindow
//////////////////////////////////////////////////
Ogre2RenderWindow::Ogre2RenderWindow()
{
}

//////////////////////////////////////////////////
Ogre2RenderWindow::~Ogre2RenderWindow()
{
}

//////////////////////////////////////////////////
Ogre::TextureGpu *Ogre2RenderWindow::RenderTarget() const
{
  return this->ogreRenderWindow;
}

//////////////////////////////////////////////////
void Ogre2RenderWindow::Destroy()
{
  // TODO(anyone)
}

//////////////////////////////////////////////////
void Ogre2RenderWindow::RebuildTarget()
{
  // TODO(anyone): determine when to rebuild
  // ie. only when ratio or handle changes!
  // e.g. sizeDirty?
  if (!this->ogreRenderWindow)
    this->BuildTarget();

  Ogre::Window *window =
      dynamic_cast<Ogre::Window *>(this->ogreRenderWindow);
  window->requestResolution(this->width, this->height);
  window->getTexture()->setResolution(this->width, this->height);
  window->windowMovedOrResized();
}

//////////////////////////////////////////////////
void Ogre2RenderWindow::BuildTarget()
{
  auto engine = Ogre2RenderEngine::Instance();
  engine->CreateRenderWindow(this->handle,
      this->width,
      this->height,
      this->ratio,
      this->antiAliasing);

  this->ogreRenderWindow = engine->OgreWindow()->getTexture();
}
