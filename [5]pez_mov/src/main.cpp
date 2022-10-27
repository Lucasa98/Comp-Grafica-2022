#include <algorithm>
#include <stdexcept>
#include <vector>
#include <limits>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Model.hpp"
#include "Window.hpp"
#include "Callbacks.hpp"
#include "Debug.hpp"
#include "Shaders.hpp"
#include "BezierRenderer.hpp"
#include "Spline.hpp"

#define VERSION 20221004

// settings
bool show_axis = false, show_fish = false, show_spline = true, show_poly = true, animate = true;

// curva
static const int degree = 3;
int ctrl_pt = -1, cant_pts = 6; // pto de control seleccionado (para el evento de arrastre)
Spline spline( { {-1.f,0.f,0.f}, {0.f,0.f,-1.f}, {1.f,0.f,0.f}, {0.f,0.f,1.f} } );

void updateControlPointsAround(Spline &spline, int ctrl_pt) {
	/// @todo: actualizar los puntos anterior y posterior a ctrl_pt
	
	///OVERHAUSSER
	//Puntos
	glm::vec3 p = spline.getControlPoint(ctrl_pt);
	glm::vec3 ant = spline.getControlPoint(ctrl_pt-3);
	glm::vec3 sig = spline.getControlPoint(ctrl_pt+3);
	
	//Segmentos
	glm::vec3 segAnt = p-ant;
	glm::vec3 segSig = sig-p;
	//Terminos de reparametrización
	float distAnt = length(segAnt);
	float distSig = length(segSig);
	
	//Calcular ptos de control no interpolantes
	glm::vec3 v = ((distSig/distAnt)*segAnt + (distAnt/distSig)*segSig) / (distSig+distAnt);
	
	///Forma gauchesca de tener continuidad parametrica
	///float distMin = min(distAnt,distSig);
	
	spline.setControlPoint(ctrl_pt-1, p - (distAnt/3.f) * v);
	spline.setControlPoint(ctrl_pt+1, p + (distSig/3.f) * v);
}

// callbacks
void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods);
void characterCallback(GLFWwindow* glfw_win, unsigned int code);

glm::mat4 getTransform(const Spline &spline, double t) {
	/// @todo: obtener los ejes y la nueva posicion del origen en funcion
	///        de la curva y el valor del parametro t
	glm::vec3 dir;						//Derivada
	const glm::vec3& pos = spline.at(t, dir);	//La posicion del pez es la posicion indicada por la curva en t
	dir = normalize(dir);
	float sin = .5f*dir.z;
	float cos = .5f*dir.x;
	
	glm::vec3 e_x(cos,0.f,sin);
	glm::vec3 e_y(0.f,.5f,0.f);
	glm::vec3 e_z(-sin,0.f,cos);
	
	// armar la matriz
	glm::mat4 m(1.f);
	for(int k=0;k<3;++k) {		//Recorre las 3 primeras filas (la cuarta es (0,0,0,1))
		m[0][k] = e_x[k];
		m[1][k] = e_y[k];
		m[2][k] = e_z[k];
		m[3][k] = pos[k];
	}
	return m;
}

// cuando cambia la cant de tramos, regenerar la spline
void remapSpline(Spline &spline, int cant_pts) {
	if (cant_pts<3) return;
	if (static_cast<int>(spline.getPieces().size()) == cant_pts) return;
	std::vector<glm::vec3> vp;
	double dt = 1.0/cant_pts;
	for(int i=0;i<cant_pts;++i)
		vp.push_back(spline.at(i*dt));
	spline = Spline(vp);
	for(int i=0;i<spline.getControlPointsCount();i+=degree) 
		updateControlPointsAround(spline,i);
}

int main() {
	
	// initialize window and setup callbacks
	Window window(win_width,win_height,"CG Demo",true);
	glfwSetFramebufferSizeCallback(window, common_callbacks::viewResizeCallback);
	glfwSetCursorPosCallback(window, mouseMoveCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetCharCallback(window, characterCallback);
	
	// setup OpenGL state and load the model
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); 
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glClearColor(0.4f,0.4f,0.7f,1.f);
	Shader shader_fish("shaders/fish");
	Shader shader_phong("shaders/phong");
	auto fish = Model::load("fish",Model::fKeepGeometry|Model::fDynamic);
	auto axis = Model::load("axis",Model::fDontFit);
	BezierRenderer bezier_renderer(500);
	model_angle = .33; view_angle = .85;
	
	// main loop
	FrameTimer ftime;
	float t = 0.f, speed = .05f;
	do {
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		remapSpline(spline,cant_pts);
		
		// draw models and curve
		float dt = ftime.newFrame();
		if (animate) {
			t += dt*speed; while (t>1.f) t-=1.f; 
		}
		if (show_fish) {
			shader_fish.use();
			shader_fish.setLight(glm::vec4{-2.f,-2.f,-4.f,0.f}, glm::vec3{1.f,1.f,1.f}, 0.15f);
			shader_fish.setUniform("t",t*20);
			glm::mat4 m = getTransform(spline, t);
			auto mats = common_callbacks::getMatrixes();
			for(Model &model : fish) {
				shader_fish.setMatrixes(mats[0]*m,mats[1],mats[2]);
				shader_fish.setBuffers(model.buffers);
				shader_fish.setMaterial(model.material);
				model.buffers.draw();
			}
		}
		
		if (show_spline or show_poly) {
			setMatrixes(bezier_renderer.getShader());
			for(const auto & curve : spline.getPieces()) {
				bezier_renderer.update(curve);
				glPointSize(1);
				if (show_spline) bezier_renderer.drawCurve();
				glPointSize(5);
				if (show_poly) bezier_renderer.drawPoly();
			}
		}
		
		if (show_axis) {
			shader_phong.use();
			shader_phong.setLight(glm::vec4{-2.f,-2.f,-4.f,0.f}, glm::vec3{1.f,1.f,1.f}, 0.15f);
			setMatrixes(shader_phong);
			glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
			for(const Model &model : axis) {
				shader_phong.setBuffers(model.buffers);
				shader_phong.setMaterial(model.material);
				model.buffers.draw();
			}
		}
		
		// settings sub-window
		window.ImGuiDialog("CG Example",[&](){
			ImGui::Checkbox("Pez (P)",&show_fish);
			ImGui::Checkbox("Spline (S)",&show_spline);
			ImGui::Checkbox("Pol. Ctrl. (C)",&show_poly);
			ImGui::Checkbox("Ejes (J)",&show_axis);
			ImGui::Checkbox("Animar (A)",&animate);
			ImGui::SliderFloat("Velocidad",&speed,0.005f,0.5f);
			ImGui::SliderFloat("T",&t,0.f,1.f);
			if (ImGui::InputInt("Cant. Pts.",&cant_pts,1,1))
				if (cant_pts<3) cant_pts=3;
		});
		
		// finish frame
		glfwSwapBuffers(window);
		glfwPollEvents();
		
	} while( glfwGetKey(window,GLFW_KEY_ESCAPE)!=GLFW_PRESS && !glfwWindowShouldClose(window) );
}

void characterCallback(GLFWwindow* glfw_win, unsigned int code) {
	switch (code) {
		case 'a': case 'A':animate = !animate; break;
		case 's': case 'S':show_spline = !show_spline; break;
		case 'p': case 'P':show_fish = !show_fish; break;
		case 'j': case 'J':show_axis = !show_axis; break;
		case 'c': case 'C':show_poly = !show_poly; break;
		case '+': ++cant_pts; break;
		case '-': --cant_pts; break;
	}
}

glm::vec3 viewportToPlane(double xpos, double ypos) {
	auto ms = common_callbacks::getMatrixes(); // { model, view, projection }
	auto inv_matrix = glm::inverse(ms[2]*ms[1]*ms[0]); // ndc->world
	auto pa = inv_matrix * glm::vec4{ float(xpos)/win_width*2.f-1.f,
		(1.f-float(ypos)/win_height)*2.f-1.f,
		0.f, 1.f }; // point on near
	auto pb = inv_matrix * glm::vec4{ float(xpos)/win_width*2.f-1.f,
		(1.f-float(ypos)/win_height)*2.f-1.f,
		1.f, 1.f }; // point on far
	float alpha = pa[1]/(pa[1]-pb[1]);
	auto p = pa*(1-alpha) + pb*alpha; // point on plane
	return {p[0]/p[3],0.f,p[2]/p[3]};
}

void mouseMoveCallback(GLFWwindow* window, double xpos, double ypos) {
	if (ctrl_pt==-1) common_callbacks::mouseMoveCallback(window,xpos,ypos);
	else {
		spline.setControlPoint(ctrl_pt,viewportToPlane(xpos,ypos));
		if (ctrl_pt%degree==0) {
			updateControlPointsAround(spline,ctrl_pt);
			updateControlPointsAround(spline,ctrl_pt+degree);
			updateControlPointsAround(spline,ctrl_pt-degree);
		}
	}
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) return;
	ctrl_pt = -1;
	if (action == GLFW_PRESS) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		auto p = viewportToPlane(xpos,ypos);
		float dmin = .1f;
		for(int i=0;i<spline.getControlPointsCount(); ++i) {
			double aux = glm::distance(p,spline.getControlPoint(i));
			if (aux < dmin) { dmin = aux; ctrl_pt = i; }
		}
	}
	if (ctrl_pt==-1) common_callbacks::mouseButtonCallback(window,button,action,mods);
}

