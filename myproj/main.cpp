#include <fstream>
#include <string>
#include <vector>

#include <GL/glew.h>

#include <SDL2/SDL_main.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#undef main


#include "helperFunctions.h"

#include "myShader.h"
#include "myCamera.h"
#include "mySubObject.h"

#include <glm/glm.hpp>
#include <iostream>
#include "myObject.h"
#include "myLights.h"
#include "myFBO.h"
#include "default_constants.h"
#include "myScene.h"
#include "myShaders.h"

using namespace std;

// SDL variables
SDL_Window* window;
SDL_GLContext glContext;

int mouse_position[2];
bool mouse_button_pressed = false;
bool quit = false;
bool windowsize_changed = true;
bool crystalballorfirstperson_view = false;
float movement_stepsize = DEFAULT_KEY_MOVEMENT_STEPSIZE;

// Camera parameters.
myCamera *cam1;

// All the meshes 
myScene scene;

//Triangle to draw to illustrate picking
size_t picked_triangle_index = 0;
myObject *picked_object = nullptr;

// Process the event.  
void processEvents(SDL_Event current_event)
{
	switch (current_event.type)
	{
	case SDL_QUIT:
	{
		quit = true;
		break;
	}
	case SDL_KEYDOWN:
	{
		if (current_event.key.keysym.sym == SDLK_ESCAPE)
			quit = true;
		if (current_event.key.keysym.sym == SDLK_r)
			cam1->reset();
		if (current_event.key.keysym.sym == SDLK_UP || current_event.key.keysym.sym == SDLK_w)
			cam1->moveForward(movement_stepsize);
		if (current_event.key.keysym.sym == SDLK_DOWN || current_event.key.keysym.sym == SDLK_s)
			cam1->moveBack(movement_stepsize);
		if (current_event.key.keysym.sym == SDLK_LEFT || current_event.key.keysym.sym == SDLK_a)
			cam1->turnLeft(DEFAULT_LEFTRIGHTTURN_MOVEMENT_STEPSIZE);
		if (current_event.key.keysym.sym == SDLK_RIGHT || current_event.key.keysym.sym == SDLK_d)
			cam1->turnRight(DEFAULT_LEFTRIGHTTURN_MOVEMENT_STEPSIZE);
		if (current_event.key.keysym.sym == SDLK_v)
			crystalballorfirstperson_view = !crystalballorfirstperson_view;
		break;
	}
	case SDL_MOUSEBUTTONDOWN:
	{
		mouse_position[0] = current_event.button.x;
		mouse_position[1] = current_event.button.y;
		mouse_button_pressed = true;

		const Uint8 *state = SDL_GetKeyboardState(nullptr);
		if (state[SDL_SCANCODE_LCTRL])
		{
			glm::vec3 ray = cam1->constructRay(mouse_position[0], mouse_position[1]);
			scene.closestObject(ray, cam1->camera_eye, picked_object, picked_triangle_index);
		}
		break;
	}
	case SDL_MOUSEBUTTONUP:
	{
		mouse_button_pressed = false;
		break;
	}
	case SDL_MOUSEMOTION:
	{
		int x = current_event.motion.x;
		int y = current_event.motion.y;

		int dx = x - mouse_position[0];
		int dy = y - mouse_position[1];

		mouse_position[0] = x;
		mouse_position[1] = y;

		if ((dx == 0 && dy == 0) || !mouse_button_pressed) return;

		if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)) && crystalballorfirstperson_view)
			cam1->crystalball_rotateView(dx, dy);
		else if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)) && !crystalballorfirstperson_view)
			cam1->firstperson_rotateView(dx, dy);
		else if (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT))
			cam1->panView(dx, dy);

		break;
	}
	case SDL_WINDOWEVENT:
	{
		if (current_event.window.event == SDL_WINDOWEVENT_RESIZED)
			windowsize_changed = true;
		break;
	}
	case SDL_MOUSEWHEEL:
	{
		if (current_event.wheel.y < 0)
			cam1->moveBack(DEFAULT_MOUSEWHEEL_MOVEMENT_STEPSIZE);
		else if (current_event.wheel.y > 0)
			cam1->moveForward(DEFAULT_MOUSEWHEEL_MOVEMENT_STEPSIZE);
		break;
	}
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	// Initialize video subsystem
	SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO);

	// Using OpenGL 3.1 core
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

	// Create window
	window = nullptr;
	window = SDL_CreateWindow("IN4I24", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

	// Create OpenGL context
	glContext = SDL_GL_CreateContext(window);

	// Initialize glew
	glewInit();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

	glEnable(GL_MULTISAMPLE);
	glHint(GL_MULTISAMPLE_FILTER_HINT_NV, GL_NICEST);

	cam1 = new myCamera();
	SDL_GetWindowSize(window, &cam1->window_width, &cam1->window_height);


	checkOpenGLInfo(true);


	/**************************INITIALIZING LIGHTS ***************************/
	scene.lights = new myLights();
	scene.lights->lights.push_back(new myLight(glm::vec3(1, 0, 0), glm::vec3(0.5, 0.5, 0.5), myLight::POINTLIGHT));
	scene.lights->lights.push_back(new myLight(glm::vec3(0, 1, 0), glm::vec3(0.6, 0.6, 0.6), myLight::POINTLIGHT));

	/**************************INITIALIZING FBO ***************************/
	//plane will draw the color_texture of the framebufferobject fbo.
	myFBO *fbo = new myFBO();
	fbo->initFBO(cam1->window_width, cam1->window_height);

	/**************************INITIALIZING OBJECTS THAT WILL BE DRAWN ***************************/
	myObject *obj;

	//the big christmas scene.
	obj = new myObject();
	if (!obj->readObjects("models/ChristmasChallenge3.obj", true, false))
		cout << "obj3 readScene failed.\n";
	obj->createmyVAO();
	scene.addObjects(obj, "ChristmasChallenge3");

	//plane1
	obj = new myObject();
	obj->readObjects("models/plane.obj", false, false);
	obj->computeTexturecoordinates_plane();
	obj->createmyVAO();
	obj->setTexture(fbo->colortexture, mySubObject::COLORMAP);
	obj->translate(0, 0, -2);
	scene.addObjects(obj, "plane");
	
	//plane2
	obj = new myObject();
	obj->readObjects("models/plane.obj", false, false);
	obj->computeTexturecoordinates_plane();
	obj->createmyVAO();
	obj->setTexture(fbo->colortexture, mySubObject::COLORMAP);
	obj->scale(0.1f, 0.1f, 0.1f);
	obj->translate(0.57f, -0.17f, -2.0f);
	scene.addObjects(obj, "plane2");

	//objectwithtexture has spheretexture of scenary.ppm
	obj = new myObject();
	obj->readObjects("models/sphere.obj", false, true);
	obj->computeTexturecoordinates_sphere();
	obj->createmyVAO();
	obj->setTexture(new myTexture("models/scenary.jpg"), mySubObject::COLORMAP);
	obj->translate(6, 4, 8);
	scene.addObjects(obj, "objectwithtexture");

	//apple has bump texture
	obj = new myObject();
	obj->readObjects("models/apple.obj", false, true);
	obj->computeTexturecoordinates_cylinder();
	obj->computeTangents();
	obj->createmyVAO();
	obj->setTexture(new myTexture("models/br-diffuse.ppm"), mySubObject::COLORMAP);
	obj->setTexture(new myTexture("models/br-normal.ppm"), mySubObject::BUMPMAP);
	obj->translate(16, 14, 18);
	scene.addObjects(obj, "apple");

	//mario 
	obj = new myObject();
	obj->readObjects("models/MarioandLuigi/mario_obj.obj", true, false);
	obj->createmyVAO();
	obj->scale(0.4f, 0.4f, 0.4f);
	scene.addObjects(obj, "mario");

	//enviornment mapped object
	obj = new myObject();
	obj->readObjects("models/apple.obj", false, false);
	obj->computeTexturecoordinates_sphere();
	obj->createmyVAO();
	//vector <string> cubemaps = { "models/yokohamapark/posx.jpg", "models/yokohamapark/negx.jpg", "models/yokohamapark/posy.jpg", "models/yokohamapark/negy.jpg", "models/yokohamapark/posz.jpg", "models/yokohamapark/negz.jpg" };
	vector <string> cubemaps = { "models/building/posx.jpg", "models/building/negx.jpg", "models/building/posy.jpg", "models/building/negy.jpg", "models/building/posz.jpg", "models/building/negz.jpg" };
	obj->setTexture(new myTexture("models/scenary.jpg"), mySubObject::COLORMAP);
	obj->setTexture(new myTexture(cubemaps), mySubObject::CUBEMAP);
	obj->scale(10.0f, 10.0f, 10.0f);
	obj->translate(10, 6, 10);
	scene.addObjects(obj, "applewithenvironmentmap");



	/**************************SETTING UP OPENGL SHADERS ***************************/
	myShaders shaders;
	shaders.addShader(new myShader("shaders/basic-vertexshader.glsl", "shaders/basic-fragmentshader.glsl"), "shader_basic");
	shaders.addShader(new myShader("shaders/phong-vertexshader.glsl", "shaders/phong-fragmentshader.glsl"), "shader_phong");
	shaders.addShader(new myShader("shaders/texture-vertexshader.glsl", "shaders/texture-fragmentshader.glsl"), "shader_texture");
	shaders.addShader(new myShader("shaders/texture+phong-vertexshader.glsl", "shaders/texture+phong-fragmentshader.glsl"), "shader_texturephong");
	shaders.addShader(new myShader("shaders/bump-vertexshader.glsl", "shaders/bump-fragmentshader.glsl"), "shader_bump");
	shaders.addShader(new myShader("shaders/imageprocessing-vertexshader.glsl", "shaders/imageprocessing-fragmentshader.glsl"), "shader_imageprocessing");
	shaders.addShader(new myShader("shaders/environmentmap-vertexshader.glsl", "shaders/environmentmap-fragmentshader.glsl"), "shader_environmentmap");



	myShader *curr_shader;
	// display loop
	while (!quit)
	{
		if (windowsize_changed)
		{
			SDL_GetWindowSize(window, &cam1->window_width, &cam1->window_height);
			windowsize_changed = false;

			if (fbo) delete fbo;
			fbo = new myFBO();
			fbo->initFBO(cam1->window_width, cam1->window_height);
			scene["plane"]->setTexture(fbo->colortexture, mySubObject::COLORMAP);
			scene["plane2"]->setTexture(fbo->colortexture, mySubObject::COLORMAP);
		}

		//Computing transformation matrices. Note that model_matrix will be computed and set in the displayScene function for each object separately
		glViewport(0, 0, cam1->window_width, cam1->window_height);
		glm::mat4 projection_matrix = cam1->projectionMatrix();
		glm::mat4 view_matrix = cam1->viewMatrix();
		glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(view_matrix)));

		//Setting uniform variables for each shader
		for (unsigned int i=0;i<shaders.size();i++)
		{
			curr_shader = shaders[i];
			curr_shader->start();
			curr_shader->setUniform("myprojection_matrix", projection_matrix);
			curr_shader->setUniform("myview_matrix", view_matrix);
			scene.lights->setUniform(curr_shader, "Lights");
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		/*-----------------------*/
		curr_shader = shaders["shader_phong"];
		curr_shader->start();
		fbo->bind();
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			scene["ChristmasChallenge3"]->displayObjects(curr_shader, view_matrix);
			scene["objectwithtexture"]->displayObjects(curr_shader, view_matrix);
		}
		fbo->unbind();
		scene["ChristmasChallenge3"]->displayObjects(curr_shader, view_matrix);
		/*-----------------------*/
		curr_shader = shaders["shader_texturephong"];
		curr_shader->start();
		scene["objectwithtexture"]->displayObjects(curr_shader, view_matrix);
		scene["plane"]->displayObjects(curr_shader, view_matrix);
		scene["apple"]->translate(2, 0, -2);
		scene["apple"]->displayObjects(curr_shader, view_matrix);
		scene["apple"]->translate(-2, 0, 2);
		/*-----------------------*/
		curr_shader = shaders["shader_bump"];
		curr_shader->start();
		scene["apple"]->displayObjects(curr_shader, view_matrix);
		/*-----------------------*/
		curr_shader = shaders["shader_environmentmap"];
		curr_shader->start();
		scene["applewithenvironmentmap"]->displayObjects(curr_shader, view_matrix);
		/*-----------------------*/
		curr_shader = shaders["shader_texturephong"];
		curr_shader->start();
		scene["mario"]->displayObjects(curr_shader, view_matrix);

		fbo->clear();
		fbo->bind();
		{
			scene["mario"]->displayObjects(curr_shader, view_matrix);
			scene["ChristmasChallenge3"]->displayObjects(curr_shader, view_matrix);
		}
		fbo->unbind();
		/*-----------------------*/
		curr_shader = shaders["shader_imageprocessing"];
		curr_shader->start();
		curr_shader->setUniform("input_color", glm::vec4(0, 1, 0, 1));

		curr_shader->setUniform("myview_matrix", glm::mat4(1.0f));

		scene["plane2"]->displayObjects(curr_shader, glm::mat4(1.0f));

		curr_shader->setUniform("myview_matrix", view_matrix);
		/*-----------------------*/
		curr_shader = shaders["shader_basic"];
		curr_shader->start();

		if (picked_object != nullptr)
		{
			curr_shader->setUniform("mymodel_matrix", picked_object->model_matrix);

			curr_shader->setUniform("input_color", glm::vec4(0, 1, 0, 1));
			picked_object->displayObjects(curr_shader, view_matrix);

			curr_shader->setUniform("input_color", glm::vec4(1, 0, 0, 1));
			glBegin(GL_TRIANGLES);
			{
				glVertex3fv(&(picked_object->vertices[picked_object->indices[picked_triangle_index][0]][0]));
				glVertex3fv(&(picked_object->vertices[picked_object->indices[picked_triangle_index][1]][0]));
				glVertex3fv(&(picked_object->vertices[picked_object->indices[picked_triangle_index][2]][0]));
			}
			glEnd();
		}
		/*-----------------------*/


		SDL_GL_SwapWindow(window);

		SDL_Event current_event;
		while (SDL_PollEvent(&current_event) != 0)
			processEvents(current_event);
	}

	// Destroy window
	if (glContext) SDL_GL_DeleteContext(glContext);
	if (window) SDL_DestroyWindow(window);

	// Quit SDL subsystems
	SDL_Quit();

	return 0;
}