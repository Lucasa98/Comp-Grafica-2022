#include <algorithm>
#include <stdexcept>
#include <vector>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Model.hpp"
#include "Window.hpp"
#include "Callbacks.hpp"
#include "Debug.hpp"
#include "Shaders.hpp"
#include "Car.hpp"

#define VERSION 20220901.2

// models and settings
bool wireframe = false, play = false, top_view = true;

// extra callbacks (atajos de teclado para cambiar de modo y camara)
void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods);

// función para sensar joystick o teclado (si no hay joystick) y definir las 
// entradas para el control del auto
std::tuple<float,float,bool> getInput(GLFWwindow *window);

// matrices que definen la camara
glm::mat4 projection_matrix, view_matrix;

// struct para guardar cada "parte" del auto
struct Part {
	std::string name;
	bool show;
	std::vector<Model> models;
};

// función para renderizar cada "parte" del auto
void renderPart(const Car &car, const std::vector<Model> &v_models, const glm::mat4 &matrix) {
	static Shader shader("shaders/phong");
	
	// select a shader
	for(const Model &model : v_models) {
		shader.use();
		
		// matrixes
		if (play) {
			/// @todo: modificar una de estas matrices para mover todo el auto (todas
			///        las partes) a la posición (y orientación) que le corresponde en la pista
			glm::mat4 M(std::cos(car.ang), 0.f, std::sin(car.ang), 0.f,
						0.f, 1.f, 0.f, 0.f,
						-std::sin(car.ang), 0.f, std::cos(car.ang), 0.f,
						car.x, 0.f, car.y, 1.f);
			shader.setMatrixes(M*matrix,view_matrix,projection_matrix);
		} else {
			glm::mat4 model_matrix = glm::rotate(glm::mat4(1.f),view_angle,glm::vec3{1.f,0.f,0.f}) *
						             glm::rotate(glm::mat4(1.f),model_angle,glm::vec3{0.f,1.f,0.f}) *
			                         matrix;
			shader.setMatrixes(model_matrix,view_matrix,projection_matrix);
		}
		
		// setup light and material
		shader.setLight(glm::vec4{20.f,-20.f,-40.f,0.f}, glm::vec3{1.f,1.f,1.f}, 0.35f);
		shader.setMaterial(model.material);
		
		// send geometry
		shader.setBuffers(model.buffers);
		glPolygonMode(GL_FRONT_AND_BACK,(wireframe and (not play))?GL_LINE:GL_FILL);
		model.buffers.draw();
	}
}

// función que renderiza la pista
void RenderTrack() {
	static Model track = Model::loadSingle("track",Model::fDontFit);
	static Shader shader("shaders/texture");
	shader.use();
	shader.setMatrixes(glm::mat4(1.f),view_matrix,projection_matrix);
	shader.setMaterial(track.material);
	shader.setBuffers(track.buffers);
	track.texture.bind();
	static float aniso = -1.0f;
	if (aniso<0) glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &aniso);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, aniso); 
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	track.buffers.draw();
}

// función que actualiza las matrices que definen la cámara
void setViewAndProjectionMatrixes( Car &car ) {
	projection_matrix = glm::perspective( glm::radians(view_fov), float(win_width)/float(win_height), 0.1f, 100.f );
	if (play) {
		if (top_view) {
			/// @todo: modificar el look at para que en esta vista el auto siempre apunte hacia arriba
			glm::vec3 pos_auto = {car.x, 0.f, car.y};
			view_matrix = glm::lookAt( pos_auto+glm::vec3{0.f,30.f,0.f}, pos_auto, glm::vec3(std::cos(car.ang),0.f,std::sin(car.ang)) );
		} else {
			/// @todo: definir view_matrix de modo que la camara persiga al auto desde "atras"
			glm::vec3 pos_auto = {car.x, 0.f, car.y};
			float ang_dif = car.ang - car.camAng;			//Diferencia entre el angulo del auto y el angulo de la camara
			float dAng = 0.1*ang_dif;						//Cociente entre el giro del auto y el giro de la camara
			
			float fAng = car.camAng+(car.vel/5.f)*dAng;		//Angulo asi delante
			float rAng = car.camAng+dAng;					//Angulo hacia atras
			
			float dist = 5 + car.vel*0.03;
			
			glm::vec3 front(5.f*std::cos(fAng),0.f,5.f*std::sin(fAng));  						//El centro esta un poco mas adelante del auto
			glm::vec3 rear(-dist*std::cos(car.camAng+dAng),2.5f,-dist*std::sin(car.camAng+dAng));
			view_matrix = glm::lookAt(pos_auto+rear, pos_auto + front, glm::vec3(0.f,1.f,0.f));
			car.camAng = rAng;	//Nuevo angulo de la camara (el rAng)
		}
	} else {
		view_matrix = glm::lookAt( glm::vec3{0.f,0.f,3.f}, view_target, glm::vec3{0.f,1.f,0.f} );
	}
}

// función que rendiriza todo el auto, parte por parte
void renderCar(const Car &car, const std::vector<Part> &parts) {
	const Part &axis = parts[0], &body = parts[1], &wheel = parts[2],
	           &fwing = parts[3], &rwing = parts[4], &helmet = parts[5];
	
	/// @todo: armar la matriz de transformación de cada parte para construir el auto
	/*Matriz:
			(Ax, Ay, Az, Aw,
			 Bx, By, Bz, Bw,
			 Cx, Cy, Cz, Cw,
			 Tx, Ty, Tz, Tw
			)
	*/
	//Definimos una matriz que vamos a modificar para cada parte (no hace falta inicializarla, pero bue)
	glm::mat4 M(1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f);
	
	if (body.show or play) {
		M = glm::mat4(1.f, 0.f, 0.f, 0.f,
					  0.f, 1.f, 0.f, 0.f,
					  0.f, 0.f, 1.f, 0.f,
					  0.f, 0.2f, 0.f, 1.f);
		renderPart(car,body.models, M);
	}
	
	if (wheel.show or play) {
		float wscl = 0.2;
		glm::mat4 MdirDer(std::cos(car.rang1), 0.f, -std::sin(car.rang1), 0.f,
						  0.f, 1.f, 0.f, 0.f,
						  std::sin(car.rang1), 0.f, std::cos(car.rang1), 0.f,
						  0.f, 0.0f, 0.f, 1.f);
		glm::mat4 MdirIzq(std::cos(car.rang1), 0.f, std::sin(car.rang1), 0.f,
						  0.f, 1.f, 0.f, 0.f,
						  -std::sin(car.rang1), 0.f, std::cos(car.rang1), 0.f,
						  0.f, 0.0f, 0.f, 1.f);
		glm::mat4 MtraccionDer(std::cos(car.rang2), -std::sin(car.rang2), 0.f, 0.f,
							   std::sin(car.rang2), std::cos(car.rang2), 0.f, 0.f,
							   0.f, 0.f, 1.f, 0.f,
							   0.f, 0.0f, 0.f, 1.f);
		glm::mat4 MtraccionIzq(std::cos(car.rang2), -std::sin(car.rang2), 0.f, 0.f,
							   std::sin(car.rang2), std::cos(car.rang2), 0.f, 0.f,
							   0.f, 0.f, 1.f, 0.f,
							   0.f, 0.0f, 0.f, 1.f);
		
		///Adelante izq
		M = glm::mat4(wscl, 0.f, 0.f, 0.f,
					  0.f, wscl, 0.f, 0.f,
					  0.f, 0.f, wscl, 0.f,
					  0.5f, 0.2f, -0.4f, 1.f);
		renderPart(car,wheel.models, M*MdirIzq*MtraccionIzq);
		
		///Adelante der
		M = glm::mat4(wscl, 0.f, 0.f, 0.f,
					  0.f, wscl, 0.f, 0.f,
					  0.f, 0.f, -wscl, 0.f,
					  0.5f, 0.2f, 0.4f, 1.f);
		renderPart(car,wheel.models, M*MdirDer*MtraccionDer);
		
		///Atras izq
		M = glm::mat4(wscl, 0.f, 0.f, 0.f,
					  0.f, wscl, 0.f, 0.f,
					  0.f, 0.f, wscl, 0.f,
					  -0.9f, 0.2f, -0.4f, 1.f);
		renderPart(car,wheel.models, M*MtraccionIzq);
		
		///Atras der
		M = glm::mat4(wscl, 0.f, 0.f, 0.f,
					  0.f, wscl, 0.f, 0.f,
					  0.f, 0.f, -wscl, 0.f,
					  -0.9f, 0.2f, 0.4f, 1.f);
		renderPart(car,wheel.models, M*MtraccionDer);
	}
	
	if (fwing.show or play) {
		M = glm::mat4(0.f, 0.f, 0.3f, 0.f,
					  0.f, 0.5f, 0.f, 0.f,
					  -0.3f, 0.f, 0.f, 0.f,
					  0.9f, 0.2f, 0.f, 1.f);
		renderPart(car,fwing.models, M);
	}
	
	if (rwing.show or play) {
		float scl = 0.30f;
		M = glm::mat4(0.f, 0.f, scl, 0.f,
					  0.f, -scl, 0.f, 0.f,
					  scl, 0.f, 0.f, 0.f,
					  -1.f, 0.5f, 0.f, 1.f);
		renderPart(car,rwing.models, M);
	}
	
	if (helmet.show or play) {
		M = glm::mat4(0.f, 0.f, 0.1f, 0.f,
					  0.f, 0.1, 0.f, 0.f,
					  -0.1f, 0.f, 0.f, 0.f,
					  0.f, 0.3f, 0.f, 1.f);
		renderPart(car,helmet.models, M);
	}
	
	if (axis.show and (not play)) renderPart(car,axis.models,glm::mat4(1.f));
}

// main: crea la ventana, carga los modelos e implementa el bucle principal
int main() {
	
	// initialize window and setup callbacks
	Window window(win_width,win_height,"CG Demo",true);
	setCommonCallbacks(window);
	glfwSetKeyCallback(window, keyboardCallback);
	
	// setup OpenGL state and load shaders
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.4f,0.4f,0.8f,1.f);
	
	// main loop
	std::vector<Part> parts; parts.reserve(8);
	parts.push_back({"axis",      true,Model::load("axis",      Model::fDontFit)});
	parts.push_back({"body",      true,Model::load("body",      Model::fDontFit)});
	parts.push_back({"wheels",    true,Model::load("wheel",     Model::fDontFit)});
	parts.push_back({"front wing",true,Model::load("front_wing",Model::fDontFit)});
	parts.push_back({"rear wing", true,Model::load("rear_wing", Model::fDontFit)});
	parts.push_back({"driver",    true,Model::load("driver",    Model::fDontFit)});
	
	Car car(+66,-35,1.38);
	
	Track track("mapa.png",100,100);
	
	FrameTimer ftime;
	double accum_dt = 0.0;
	double lap_time = 0.0;
	double last_lap = 0.0;
	do {
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		// actualizar las pos del auto y de la camara
		double elapsed_time = ftime.newFrame();
		accum_dt += elapsed_time;
		if (play) lap_time += elapsed_time;
		auto in = getInput(window);
		while (accum_dt>1.0/60.0) { 
			car.Move(track,std::get<0>(in),std::get<1>(in),std::get<2>(in));
			if (track.isFinishLine(car.x,car.y) and lap_time>5) {
				last_lap = lap_time; lap_time = 0.0;
			}
			accum_dt-=1.0/60.0;
			
			setViewAndProjectionMatrixes(car);
		}
		
		// setear matrices y renderizar
		if (play) RenderTrack();
		renderCar(car,parts);
		
		// settings sub-window
		window.ImGuiDialog("CG Example",[&](){
			ImGui::Checkbox("Play (P)",&play);
			if (play) {
				ImGui::LabelText("","Lap Time: %f s",lap_time<5 ? last_lap : lap_time);
				ImGui::Checkbox("Top View (T)",&top_view);
			} else {
				ImGui::Checkbox("Wireframe (W)",&wireframe);
				if (ImGui::TreeNode("Parts")) {
					for(Part &p : parts)
						ImGui::Checkbox(p.name.c_str(),&p.show);
					ImGui::TreePop();
				}
			}
			if (ImGui::TreeNode("car")) {
				ImGui::LabelText("","x: %f",car.x);
				ImGui::LabelText("","y: %f",car.y);
				ImGui::LabelText("","vel: %f",car.vel);
				ImGui::LabelText("","ang: %f",car.ang);
				ImGui::LabelText("","camAng: %f",car.camAng);
				ImGui::LabelText("","rang1: %f",car.rang1);
				ImGui::LabelText("","rang2: %f",car.rang2);
				ImGui::TreePop();
			}
		});
		
		// finish frame
		glfwSwapBuffers(window);
		glfwPollEvents();
		
	} while( glfwGetKey(window,GLFW_KEY_ESCAPE)!=GLFW_PRESS && !glfwWindowShouldClose(window) );
}

void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods) {
	if (action==GLFW_PRESS) {
		switch (key) {
		case 'W': wireframe = !wireframe; break;
		case 'T': if (!play) play = true; else top_view = !top_view; break;
		case 'P': play = !play; break;
		}
	}
}

std::tuple<float,float,bool> getInput(GLFWwindow *window) {
	float acel = 0.f, dir = 0.f; bool analog = false;
	if (glfwGetKey(window,GLFW_KEY_UP)   ==GLFW_PRESS) acel += 1.f;
	if (glfwGetKey(window,GLFW_KEY_DOWN) ==GLFW_PRESS) acel -= 1.f;
	if (glfwGetKey(window,GLFW_KEY_RIGHT)==GLFW_PRESS) dir += 1.f;
	if (glfwGetKey(window,GLFW_KEY_LEFT) ==GLFW_PRESS) dir -= 1.f;
	
	int count;
	const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
	if (count) { dir = axes[0]; acel = -axes[1]; analog = true; }
	return std::make_tuple(acel,dir,analog);
}
