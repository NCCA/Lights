#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Camera.h>
#include <ngl/Light.h>
#include <ngl/Transformation.h>
#include <ngl/Material.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/Random.h>
#include <ngl/ShaderLib.h>

NGLScene::NGLScene()
{
  setTitle("Using ngl::Light as a point light");
  m_teapotRotation=0.0f;
  m_scale=8.0f;
}


NGLScene::~NGLScene()
{
  std::cout<<"Shutting down NGL, removing VAO's and Shaders\n";

}

void NGLScene::resizeGL( int _w, int _h )
{
  m_cam.setShape( 45.0f, static_cast<float>( _w ) / _h, 0.05f, 350.0f );
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

  std::cout << "OpenGL Version : " << glGetString(GL_VERSION) << std::endl;
  m_cam.set(from,to,up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_cam.setShape(45.0f,(float)720.0/576.0f,0.5f,150.0f);
  // now to load the shader and set the values
  // grab an instance of shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

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
  shader->setUniform("viewerPos",m_cam.getEye().toVec3());

  // now set the material and light values
  ngl::Material m(ngl::STDMAT::POLISHEDSILVER);
  m.loadToShader("material");
  // create the lights
  createLights();
  m_rotationTimer=startTimer(20);
  m_lightChangeTimer=startTimer(1000);


}


void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M=m_transform.getMatrix()*m_mouseGlobalTX;
  MV=M*m_cam.getViewMatrix() ;
  MVP=MV*m_cam.getProjectionMatrix() ;
  normalMatrix=MV;
  normalMatrix.inverse();
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
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  shader->use("Phong");

  // grab an instance of the primitives for drawing
  ngl::VAOPrimitives *prim=ngl::VAOPrimitives::instance();

  m_transform.reset();

  for(auto &light : m_lightArray)
  {
    // enable the light (this will set the values)
    light.enable();
    m_transform.setPosition(light.getPos());
    loadMatricesToShader();
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
  // light position
  ngl::Vec3 pos;
  // light colour
  ngl::Colour col,speccol;
  ngl::Random *rand=ngl::Random::instance();
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  shader->use("Phong");
  // we need to load the inverse view matrix to the light transform
  // this will give us the correct light positions in eye space
  ngl::Mat4 iv=m_cam.getViewMatrix();
  iv.transpose();
  // loop for the NumLights lights and set the position and colour
  int i=0;
  for(auto &light : m_lightArray)
  {
    light.enable();
    // get a random light position
    pos=rand->getRandomPoint(20,20,20);
    // create random colour
    col=rand->getRandomColour();
    col.clamp(0.05f,0.3f);
    speccol=rand->getRandomColour();
    speccol.clamp(0.1f,0.2f);
    // create an instance of the light and put it in the array
    light.setPosition(pos);
    light.setColour(col);
    light.setSpecColour(speccol);
    QString lightName=QString("light[%1]").arg(i++);
    std::cout<<lightName.toStdString()<<"\n";
    light.setTransform(iv);
    light.loadToShader(lightName.toStdString());
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

