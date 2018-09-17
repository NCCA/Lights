#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Transformation.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/Random.h>
#include <ngl/ShaderLib.h>

NGLScene::NGLScene()
{
  setTitle("Multiple Point Lights");
}


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

  shader->createShaderProgram("Phong");

  shader->attachShader("PhongVertex",ngl::ShaderType::VERTEX);
  shader->attachShader("PhongFragment",ngl::ShaderType::FRAGMENT);
  shader->loadShaderSource("PhongVertex","shaders/PhongVert.glsl");
  shader->loadShaderSource("PhongFragment","shaders/PhongFrag.glsl");

  shader->compileShader("PhongVertex");
  shader->compileShader("PhongFragment");
  shader->attachShaderToProgram("Phong","PhongVertex");
  shader->attachShaderToProgram("Phong","PhongFragment");

  shader->linkProgramObject("Phong");
  (*shader)["Phong"]->use();

  // now pass the modelView and projection values to the shader
  shader->setUniform("Normalize",1);
  shader->setUniform("viewerPos",from);
  shader->setUniform("material.ambient",0.19225f,0.19225f,0.19225f,1.0f);
  shader->setUniform("material.diffuse",0.50754f,0.50754f,0.50754f,1.0f);
  shader->setUniform("material.specular",0.508273f,0.508273f,0.508273f,1.0f);
  shader->setUniform("material.shininess",51.2f);

  // now set the material and light values

  // create the lights
  createLights();
  m_rotationTimer=startTimer(20);
  m_lightChangeTimer=startTimer(1000);


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
  shader->use("Phong");

  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M=m_mouseGlobalTX*m_transform.getMatrix();
  MV=m_view*M ;
  MVP=m_project*MV ;
  normalMatrix=MV;
  normalMatrix.inverse().transpose();
  shader->setUniform("MV",MV);
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
  ngl::VAOPrimitives *prim=ngl::VAOPrimitives::instance();

  m_transform.reset();

  for(auto &light : m_lightArray)
  {
    m_transform.setPosition(light.position.toVec3());
    loadMatricesToColourShader(light.diffuse);
    prim->draw("cube");
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

    case Qt::Key_Minus :
          --m_scale;
    break;

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
  shader->use("Phong");
  // loop for the NumLights lights and set the position and colour
  int i=0;
  for(auto &light : m_lightArray)
  {
    // get a random light position
    light.position=rand->getRandomPoint(20,20,20);
    // create random colour
    light.diffuse=rand->getRandomColour4();
    light.diffuse.clamp(0.05f,0.3f);
    light.specular=rand->getRandomColour4();
    light.specular.clamp(0.1f,0.2f);
    shader->setUniform(fmt::format("light[{0}].position",i),light.position);
    shader->setUniform(fmt::format("light[{0}].diffuse",i),light.diffuse);
    shader->setUniform(fmt::format("light[{0}].specular",i),light.specular);

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

