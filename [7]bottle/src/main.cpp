#include <algorithm>
#include <stdexcept>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "ObjMesh.hpp"
#include "Shaders.hpp"
#include "Texture.hpp"
#include "Window.hpp"
#include "Callbacks.hpp"
#include "Model.hpp"

#define VERSION 20221019
#include <iostream>

std::vector<glm::vec2> generateTextureCoordinatesForBottle(const std::vector<glm::vec3> &v) {
	/// @todo: generar el vector de coordenadas de texturas para los vertices de la botella
	
	///Mapeo en dos partes (cilindro)
	std::vector<glm::vec2> vT;
	///Mapear los vertices sobre el cilindro (Presuponemos eje de revolucion en (x=0, z=0))
	///		Recorremos todos los vertices
	for(int i=0; i<v.size(); ++i){
		//Dado X y Z del vertice determinamos un angulo theta del cilindro
		float s = (glm::atan(v[i].x,v[i].z) / glm::pi<float>());
		
		//t corresponde con la coordenada Y, escalada y desplaza.
		float t = v[i].y*2.5f + 0.5f;
		
		vT.push_back(glm::vec2(s, t));
	}
	
	return vT;
}

std::vector<glm::vec2> generateTextureCoordinatesForLid(const std::vector<glm::vec3> &v) {
	/// @todo: generar el vector de coordenadas de texturas para los vertices de la tapa
	
	///Mapeo plano
	std::vector<glm::vec2> vT;
	
	///Determinamos un plano para s (-7x + 0y + 0z + 0.5 = s) y para t (0x + 0y + 7z + 0.5 = t)  (a ojo)
	for(int i=0; i<v.size(); ++i){
		float s, t;
		
		s = -v[i].x*7.f + 0.5f;
		t = v[i].z*7.f + 0.5f;
		
		vT.push_back(glm::vec2(s,t));
	}
	
	return vT;
}

int main() {
	
	// initialize window and setup callbacks
	Window window(win_width,win_height,"CG Texturas");
	setCommonCallbacks(window);
	
	// setup OpenGL state and load shaders
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
	glEnable(GL_BLEND); glad_glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.6f,0.6f,0.8f,1.f);
	Shader shader("shaders/texture");
	
	// load model and assign texture
	auto models = Model::load("bottle",Model::fKeepGeometry);
	Model &bottle = models[0], &lid = models[1];
	bottle.buffers.updateTexCoords(generateTextureCoordinatesForBottle(bottle.geometry.positions),true);
	bottle.texture = Texture("models/label.png",true,false);
	lid.buffers.updateTexCoords(generateTextureCoordinatesForLid(lid.geometry.positions),true);
	lid.texture = Texture("models/lid.png",false,false);
	
	do {
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		shader.use();
		setMatrixes(shader);
		shader.setLight(glm::vec4{2.f,-2.f,-4.f,0.f}, glm::vec3{1.f,1.f,1.f}, 0.15f);
		for(Model &mod : models) {
			mod.texture.bind();
			shader.setMaterial(mod.material);
			shader.setBuffers(mod.buffers);
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
			mod.buffers.draw();
		}
		
		// finish frame
		glfwSwapBuffers(window);
		glfwPollEvents();
		
	} while( glfwGetKey(window,GLFW_KEY_ESCAPE)!=GLFW_PRESS && !glfwWindowShouldClose(window) );
}

