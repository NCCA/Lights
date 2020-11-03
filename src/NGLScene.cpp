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
  ngl::NGLInit::initialize();

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

  ngl::ShaderLib::createShaderProgram(PBRShader);

  ngl::ShaderLib::attachShader(VertexShader,ngl::ShaderType::VERTEX);
  ngl::ShaderLib::attachShader(FragmentShader,ngl::ShaderType::FRAGMENT);
  ngl::ShaderLib::loadShaderSource(VertexShader,"shaders/PBRVertex.glsl");
  ngl::ShaderLib::loadShaderSource(FragmentShader,"shaders/PBRFragment.glsl");
  // the shader has a tag called @numLights, edit this and set to 8
  ngl::ShaderLib::editShader(VertexShader,"@numLights","8");
  ngl::ShaderLib::editShader(FragmentShader,"@numLights","8");
  ngl::ShaderLib::compileShader(VertexShader);
  ngl::ShaderLib::compileShader(FragmentShader);
  ngl::ShaderLib::attachShaderToProgram(PBRShader,VertexShader);
  ngl::ShaderLib::attachShaderToProgram(PBRShader,FragmentShader);

  ngl::ShaderLib::linkProgramObject(PBRShader);
  ngl::ShaderLib::use(PBRShader);

  loadShaderDefaults();
  // now set the material and light values

  // create the lights
  createLights();
  m_rotationTimer=startTimer(20);
  m_lightChangeTimer=startTimer(1000);


}

void NGLScene::loadShaderDefaults()
{
  ngl::ShaderLib::use(PBRShader);
  ngl::ShaderLib::setUniform( "camPos", 0.0f,10.0f,20.0f );
  ngl::ShaderLib::setUniform("exposure",2.2f);
  ngl::ShaderLib::setUniform("albedo",0.950f, 0.71f, 0.29f);

  ngl::ShaderLib::setUniform("metallic",1.02f);
  ngl::ShaderLib::setUniform("roughness",0.38f);
  ngl::ShaderLib::setUniform("ao",0.2f);

}

void NGLScene::loadMatricesToColourShader(const ngl::Vec4 &_colour)
{
  ngl::ShaderLib::use(ngl::nglColourShader);
  ngl::Mat4 MVP=m_project*m_view*m_mouseGlobalTX*m_transform.getMatrix();
  ngl::ShaderLib::setUniform("MVP",MVP);
  ngl::ShaderLib::setUniform("Colour",_colour);
}

void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib::use(PBRShader);

  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M=m_mouseGlobalTX*m_transform.getMatrix();
  MV=m_view*M ;
  MVP=m_project*MV ;
  normalMatrix=MV;
  normalMatrix.inverse().transpose();
  ngl::ShaderLib::setUniform("MVP",MVP);
  ngl::ShaderLib::setUniform("normalMatrix",normalMatrix);
  ngl::ShaderLib::setUniform("M",M);
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
  if(m_showLights)
  {
    m_transform.reset();

    for(auto &light : m_lightArray)
    {
      m_transform.setPosition(light.position);
      loadMatricesToColourShader(light.colour);
      ngl::VAOPrimitives::draw("cube");
    }
  }
  m_transform.reset();
  m_transform.setScale(m_scale,m_scale,m_scale);
  m_transform.setRotation(m_teapotRotation,m_teapotRotation,m_teapotRotation);
  // now set this value in the shader for the current ModelMatrix
  loadMatricesToShader();
  ngl::VAOPrimitives::draw("teapot");
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
  ngl::ShaderLib::use(PBRShader);
  // loop for the NumLights lights and set the position and colour
  int i=0;
  for(auto &light : m_lightArray)
  {
    // get a random light position
    light.position=ngl::Random::getRandomPoint(20,20,20);
    // create random colour
    light.colour=ngl::Vec3(0.1f,0.1f,0.1f)+ngl::Random::getRandomColour3()*200;
    ngl::ShaderLib::setUniform(fmt::format("lightPositions[{0}]",i),light.position);
    ngl::ShaderLib::setUniform(fmt::format("lightColours[{0}]",i),light.colour);
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
  m_numLights=std::min(m_numLights, std::max(1ul, 120ul));
  auto editString=fmt::format("{0}",m_numLights);
  ngl::ShaderLib::editShader(VertexShader,"@numLights",editString);
  ngl::ShaderLib::editShader(FragmentShader,"@numLights",editString);
  ngl::ShaderLib::compileShader(VertexShader);
  ngl::ShaderLib::compileShader(FragmentShader);
  ngl::ShaderLib::linkProgramObject(PBRShader);
  ngl::ShaderLib::use(PBRShader);
  m_lightArray.resize(m_numLights);
  createLights();
  setTitle(QString(fmt::format("Number of Light {0}",m_numLights).c_str()));
  loadShaderDefaults();

}
