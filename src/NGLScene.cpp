#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Transformation.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/Random.h>
#include <ngl/ShaderLib.h>
#ifdef WIN32
    #define NOMINMAX
#endif


NGLScene::NGLScene()
{
  setTitle("Multiple Point Lights");
  m_lightArray.resize(m_numLights);
}

constexpr auto PBRShader="PBR";
constexpr auto VertexShader="PBRVertex";
constexpr auto FragmentShader="PBRFragment";
NGLScene::~NGLScene()
{
  ngl::msg->addMessage("Shutting down NGL, removing VAO's and Shaders");
}

void NGLScene::resizeGL( int _w, int _h )
{
  m_project=ngl::perspective(45.0f, static_cast<float>( _w ) / _h, 0.05f, 350.0f );
  m_win.width  = static_cast<int>( _w * devicePixelRatio() );
  m_win.height = static_cast<int>( _h * devicePixelRatio() );
}

void NGLScene::initializeGL()
{
  ngl::NGLInit::instance();

  glClearColor(0.4f, 0.4f, 0.4f, 1.0f);			   // Grey Background
  // enable depth testing for drawing
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_MULTISAMPLE);
  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0,10,20);
  ngl::Vec3 to(0,0,0);
  ngl::Vec3 up(0,1,0);

  m_view=ngl::lookAt(from,to,up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_project=ngl::perspective(45.0f,720.0f/576.0f,0.5f,150.0f);
  // now to load the shader and set the values
  // grab an instance of shader manager
  auto *shader=ngl::ShaderLib::instance();

  shader->createShaderProgram(PBRShader);

  shader->attachShader(VertexShader,ngl::ShaderType::VERTEX);
  shader->attachShader(FragmentShader,ngl::ShaderType::FRAGMENT);
  shader->loadShaderSource(VertexShader,"shaders/PBRVertex.glsl");
  shader->loadShaderSource(FragmentShader,"shaders/PBRFragment.glsl");
  // the shader has a tag called @numLights, edit this and set to 8
  shader->editShader(VertexShader,"@numLights","8");
  shader->editShader(FragmentShader,"@numLights","8");
  shader->compileShader(VertexShader);
  shader->compileShader(FragmentShader);
  shader->attachShaderToProgram(PBRShader,VertexShader);
  shader->attachShaderToProgram(PBRShader,FragmentShader);

  shader->linkProgramObject(PBRShader);
  (*shader)[PBRShader]->use();

  loadShaderDefaults();
  // now set the material and light values

  // create the lights
  createLights();
  m_rotationTimer=startTimer(20);
  m_lightChangeTimer=startTimer(1000);


}

void NGLScene::loadShaderDefaults()
{
  auto *shader=ngl::ShaderLib::instance();
  shader->use(PBRShader);
  shader->setUniform( "camPos", 0.0f,10.0f,20.0f );
  shader->setUniform("exposure",2.2f);
  shader->setUniform("albedo",0.950f, 0.71f, 0.29f);

  shader->setUniform("metallic",1.02f);
  shader->setUniform("roughness",0.38f);
  shader->setUniform("ao",0.2f);

}

void NGLScene::loadMatricesToColourShader(const ngl::Vec4 &_colour)
{
  auto *shader=ngl::ShaderLib::instance();
  shader->use(ngl::nglColourShader);
  ngl::Mat4 MVP=m_project*m_view*m_mouseGlobalTX*m_transform.getMatrix();
  shader->setUniform("MVP",MVP);
  shader->setUniform("Colour",_colour);
}

void NGLScene::loadMatricesToShader()
{
  auto *shader=ngl::ShaderLib::instance();
  shader->use(PBRShader);

  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M=m_mouseGlobalTX*m_transform.getMatrix();
  MV=m_view*M ;
  MVP=m_project*MV ;
  normalMatrix=MV;
  normalMatrix.inverse().transpose();
  shader->setUniform("MVP",MVP);
  shader->setUniform("normalMatrix",normalMatrix);
  shader->setUniform("M",M);
}

void NGLScene::paintGL()
{
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  ngl::Transformation trans;
  // Rotation based on the mouse position for our global
  // transform
  ngl::Mat4 rotX;
  ngl::Mat4 rotY;
  // create the rotation matrices
  rotX.rotateX(m_win.spinXFace);
  rotY.rotateY(m_win.spinYFace);
  // multiply the rotations
  m_mouseGlobalTX=rotY*rotX;
  // add the translations
  m_mouseGlobalTX.m_m[3][0] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[3][1] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[3][2] = m_modelPos.m_z;
  // grab an instance of the primitives for drawing
  auto *prim=ngl::VAOPrimitives::instance();
  if(m_showLights)
  {
    m_transform.reset();

    for(auto &light : m_lightArray)
    {
      m_transform.setPosition(light.position);
      loadMatricesToColourShader(light.colour);
      prim->draw("cube");
    }
  }
  m_transform.reset();
  m_transform.setScale(m_scale,m_scale,m_scale);
  m_transform.setRotation(m_teapotRotation,m_teapotRotation,m_teapotRotation);
  // now set this value in the shader for the current ModelMatrix
  loadMatricesToShader();
  prim->draw("teapot");
}


//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  // this method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch (_event->key())
  {
  // escape key to quite
  case Qt::Key_Escape : QGuiApplication::exit(EXIT_SUCCESS); break;
  // turn on wirframe rendering
  case Qt::Key_W : glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); break;
  // turn off wire frame
  case Qt::Key_S : glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); break;
  // show full screen
  case Qt::Key_F : showFullScreen(); break;
  // show windowed
  case Qt::Key_N : showNormal(); break;
  case Qt::Key_Equal :
  case Qt::Key_Plus :
        ++m_scale;
  break;
  case Qt::Key_1 : updateLights(-1); break;
  case Qt::Key_2 :  updateLights(1); break;
    case Qt::Key_Minus :
          --m_scale;
    break;
  case Qt::Key_Space : m_showLights^=true; break;
  default : break;
  }
  // finally update the GLWindow and re-draw
  //if (isExposed())
    update();
}


void NGLScene::createLights()
{
  // light colour
  ngl::Random *rand=ngl::Random::instance();
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  shader->use(PBRShader);
  // loop for the NumLights lights and set the position and colour
  int i=0;
  for(auto &light : m_lightArray)
  {
    // get a random light position
    light.position=rand->getRandomPoint(20,20,20);
    // create random colour
    light.colour=ngl::Vec3(0.1f,0.1f,0.1f)+rand->getRandomColour3()*200;
    shader->setUniform(fmt::format("lightPositions[{0}]",i),light.position);
    shader->setUniform(fmt::format("lightColours[{0}]",i),light.colour);
    ++i;
  }
}

void NGLScene::timerEvent( QTimerEvent *_event )
{
  if(_event->timerId() == m_rotationTimer)
  {
    ++m_teapotRotation;
    // re-draw GL
    update();
  }

  else if(_event->timerId() == m_lightChangeTimer)
  {
    createLights();
    // re-draw GL
    update();
  }
}

void NGLScene::updateLights(int _amount)
{
  m_numLights+=_amount;
//  m_numLights=clamp(m_numLights,1,120);
  m_numLights=std::min(m_numLights, std::max(1u, 120u));
  auto *shader=ngl::ShaderLib::instance();
  auto editString=fmt::format("{0}",m_numLights);
  shader->editShader(VertexShader,"@numLights",editString);
  shader->editShader(FragmentShader,"@numLights",editString);
  shader->compileShader(VertexShader);
  shader->compileShader(FragmentShader);
  shader->linkProgramObject(PBRShader);
  shader->use(PBRShader);
  m_lightArray.resize(m_numLights);
  createLights();
  setTitle(QString(fmt::format("Number of Light {0}",m_numLights).c_str()));
  loadShaderDefaults();

}
