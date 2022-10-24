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
#include "SubDivMesh.hpp"
#include "SubDivMeshRenderer.hpp"

#define VERSION 20221013

// models and settings
std::vector<std::string> models_names = { "cubo", "icosahedron", "plano", "suzanne", "star" };
int current_model = 0;
bool fill = true, nodes = true, wireframe = true, smooth = false, 
	 reload_mesh = true, mesh_modified = false;

// extraa callbacks
void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods);

SubDivMesh mesh;
void subdivide(SubDivMesh &mesh);

int main() {
	
	// initialize window and setup callbacks
	Window window(win_width,win_height,"CG Demo",true);
	setCommonCallbacks(window);
	glfwSetKeyCallback(window, keyboardCallback);
	view_fov = 60.f;
	
	// setup OpenGL state and load shaders
	glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); 
	glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glClearColor(0.8f,0.8f,0.9f,1.f);
	Shader shader_flat("shaders/flat"),
	       shader_smooth("shaders/smooth"),
		   shader_wireframe("shaders/wireframe");
	SubDivMeshRenderer renderer;
	
	// main loop
	Material material;
	material.ka = material.kd = glm::vec3{.8f,.4f,.4f};
	material.ks = glm::vec3{.5f,.5f,.5f};
	material.shininess = 50.f;
	
	FrameTimer timer;
	do {
		
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		
		if (reload_mesh) {
			mesh = SubDivMesh("models/"+models_names[current_model]+".dat");
			reload_mesh = false; mesh_modified = true;
		}
		if (mesh_modified) {
			renderer = makeRenderer(mesh);
			mesh_modified = false;
		}
		
		if (nodes) {
			shader_wireframe.use();
			setMatrixes(shader_wireframe);
			renderer.drawPoints(shader_wireframe);
		}
		
		if (wireframe) {
			shader_wireframe.use();
			setMatrixes(shader_wireframe);
			renderer.drawLines(shader_wireframe);
		}
		
		if (fill) {
			Shader &shader = smooth ? shader_smooth : shader_flat;
			shader.use();
			setMatrixes(shader);
			shader.setLight(glm::vec4{2.f,1.f,5.f,0.f}, glm::vec3{1.f,1.f,1.f}, 0.25f);
			shader.setMaterial(material);
			renderer.drawTriangles(shader);
		}
		
		// settings sub-window
		window.ImGuiDialog("CG Example",[&](){
			if (ImGui::Combo(".dat (O)", &current_model,models_names)) reload_mesh = true;
			ImGui::Checkbox("Fill (F)",&fill);
			ImGui::Checkbox("Wireframe (W)",&wireframe);
			ImGui::Checkbox("Nodes (N)",&nodes);
			ImGui::Checkbox("Smooth Shading (S)",&smooth);
			if (ImGui::Button("Subdivide (D)")) { subdivide(mesh); mesh_modified = true; }
			if (ImGui::Button("Reset (R)")) reload_mesh = true;
			ImGui::Text("Nodes: %i, Elements: %i",mesh.n.size(),mesh.e.size());
		});
		
		// finish frame
		glfwSwapBuffers(window);
		glfwPollEvents();
		
	} while( glfwGetKey(window,GLFW_KEY_ESCAPE)!=GLFW_PRESS && !glfwWindowShouldClose(window) );
}

void keyboardCallback(GLFWwindow* glfw_win, int key, int scancode, int action, int mods) {
	if (action==GLFW_PRESS) {
		switch (key) {
		case 'D': subdivide(mesh); mesh_modified = true; break;
		case 'F': fill = !fill; break;
		case 'N': nodes = !nodes; break;
		case 'W': wireframe = !wireframe; break;
		case 'S': smooth = !smooth; break;
		case 'R': reload_mesh=true; break;
		case 'O': case 'M': current_model = (current_model+1)%models_names.size(); reload_mesh = true; break;
		}
	}
}

// La struct Arista guarda los dos indices de nodos de una arista
// Siempre pone primero el menor indice, para facilitar la búsqueda en lista ordenada;
//    es para usar con el Mapa de más abajo, para asociar un nodo nuevo a una arista vieja
struct Arista {
	int n[2];
	Arista(int n1, int n2) {
		n[0]=n1; n[1]=n2;
		if (n[0]>n[1]) std::swap(n[0],n[1]);
	}
	Arista(Elemento &e, int i) { // i-esima arista de un elemento
		n[0]=e[i]; n[1]=e[i+1];
		if (n[0]>n[1]) std::swap(n[0],n[1]); // pierde el orden del elemento
	}
	const bool operator<(const Arista &a) const {
		return (n[0]<a.n[0]||(n[0]==a.n[0]&&n[1]<a.n[1]));
	}
};

void subdivide(SubDivMesh &mesh) {
	
	/// @@@@@: Implementar Catmull-Clark... lineamientos:
	
	std::vector<Elemento>& vE = mesh.e;
	std::vector<Nodo>& vN = mesh.n;
	
	///Nos guardamos la cantidad de puntos originales para un truquito despues(?)
	int nOriginal = vN.size();
	
	//  Los nodos originales estan en las posiciones 0 a #n-1 de m.n,
	//  Los elementos orignales estan en las posiciones 0 a #e-1 de m.e
	//  1) Por cada elemento, agregar el centroide (nuevos nodos: #n a #n+#e-1)
	
	///Mapeamos el indice de cada elemento con el indice de su centroide
	std::map<int, int> mC;
	
	///Recorremos todas las caras (elementos)
	for(size_t i=0; i<vE.size(); ++i){
		glm::vec3 centr(0,0,0);
		///Recorremos todos los vertices de la cara acumulando en centr
		for(size_t j=0; j<vE[i].nv; ++j)
			centr += vN[vE[i][j]].p;
		centr = centr * (1.f/vE[i].nv);
		
		mC[i] = vN.size();
		vN.push_back(Nodo(centr));
	}
	
	//  2) Por cada arista de cada cara, agregar un pto en el medio que es
	//      promedio de los vertices de la arista y los centroides de las caras 
	//      adyacentes. Aca hay que usar los elementos vecinos.
	//      En los bordes, cuando no hay vecinos, es simplemente el promedio de los 
	//      vertices de la arista
	//      Hay que evitar procesar dos veces la misma arista (como?)
	//      Mas adelante vamos a necesitar determinar cual punto agregamos en cada
	//      arista, y ya que no se pueden relacionar los indices con una formula simple
	//      se sugiere usar Mapa como estructura auxiliar
	
	///Asociamos cada arista (conjunto de dos puntos) con el indice del vertice nuevo
	std::map<Arista, int> mA;
	
	///Recorremos todas las caras (elementos)
	for(size_t i=0; i<vE.size(); ++i){
		///Recorremos todas las caras vecinas (elementos)
		for(size_t j=0; j<vE[i].nv; ++j){
			///Verificamos que no hayamos visitado antes la Arista
			if(!mA.count(Arista(vE[i][j],vE[i][j+1]))){
				///Verificamos si es una arista frontera (vecino = -1)
				if(vE[i].v[j] == -1){		//Si es frontera
					glm::vec3 pAr = 0.5f*(vN[vE[i][j]].p + vN[vE[i][j+1]].p);	//Mediana entre los vertices
					
					mA[Arista(vE[i][j],vE[i][j+1])] = vN.size();
					vN.push_back(pAr);
				}else{						//Si no es frontera
					glm::vec3 pAr = vN[mC[i]].p + vN[mC[vE[i].v[j]]].p + vN[vE[i][j]].p + vN[vE[i][j+1]].p;
					pAr = pAr * (0.25f);
					
					mA[Arista(vE[i][j], vE[i][j+1])] = vN.size();
					vN.push_back(pAr);
				}
			}
		}
	}
	
	//  3) Armar los elementos nuevos
	//      Los quads se dividen en 4, (uno reemplaza al original, los otros 3 se agregan)
	//      Los triangulos se dividen en 3, (uno reemplaza al original, los otros 2 se agregan)
	//      Para encontrar los nodos de las aristas usar el mapa que armaron en el paso 2
	//      Ordenar los nodos de todos los elementos nuevos con un mismo criterio (por ej, 
	//      siempre poner primero al centroide del elemento), para simplificar el paso 4.
	
	///Recorrer todas las caras(elementos) originales
	int eSize = vE.size();
	for(int i=0; i<eSize; ++i){
		///Crear los nuevos
		///Se sigue el orden (Centroide, pArista1, VerticeOriginal, pArista2)
		///Recorremos los vertices del elemento (desde el segundo)
		for(int j=1; j<vE[i].nv; ++j){
			mesh.agregarElemento(mC[i],
								 mA[Arista(vE[i][j], vE[i][j-1])], //Arista vecina 1
								 vE[i][j], 
								 mA[Arista(vE[i][j], vE[i][j+1])]);//Arista vecina 2
		}
		
		///El primero reemplaza al original
		mesh.reemplazarElemento(i, 
								mC[i],
								mA[Arista(vE[i][0], vE[i][-1])], 
								vE[i][0],
								mA[Arista(vE[i][0], vE[i][1])]);
	}
	mesh.makeVecinos();
	
	//  4) Calcular las nuevas posiciones de los nodos originales
	//      Para nodos interiores: (4r-f+(n-3)p)/n
	//         f=promedio de centroides de las caras (los agregados en el paso 1)
	//         r=promedio de los pts medios de las aristas (los agregados en el paso 2)
	//         p=posicion del nodo original
	//         n=cantidad de elementos para ese nodo
	//      Para nodos del borde: (r+p)/2
	//         r=promedio de los dos pts medios de las aristas
	//         p=posicion del nodo original
	//      Ojo: en el paso 3 cambio toda la SubDivMesh, analizar donde quedan en los nuevos 
	//      elementos (¿de que tipo son?) los nodos de las caras y los de las aristas 
	//      que se agregaron antes.
	// tips:
	//   no es necesario cambiar ni agregar nada fuera de este método, (con Mapa como 
	//     estructura auxiliar alcanza)
	//   sugerencia: probar primero usando el cubo (es cerrado y solo tiene quads)
	//               despues usando la piramide (tambien cerrada, y solo triangulos)
	//               despues el ejemplo plano (para ver que pasa en los bordes)
	//               finalmente el mono (tiene mezcla y elementos sin vecinos)
	//   repaso de como usar un mapa:
	//     para asociar un indice (i) de nodo a una arista (n1-n2): elmapa[Arista(n1,n2)]=i;
	//     para saber si hay un indice asociado a una arista:  ¿elmapa.find(Arista(n1,n2))!=elmapa.end()?
	//     para recuperar el indice (en j) asociado a una arista: int j=elmapa[Arista(n1,n2)];
	
	///Recorremos los nodos originales (guardamos las cantidad al principio)
	for(int i=0; i<nOriginal; ++i){
		if(!vN[i].es_frontera){
			glm::vec3 f(0,0,0);		//Promedio de los centroides de las caras originales
			glm::vec3 r(0,0,0);		//Promedio de los nodos asociados a las aristas
			
			std::vector<int>& eAs = vN[i].e;	//Elementos asociados al vertice
			int N = eAs.size();
			
			///Recorremos las caras a las que pertenece el nodo
			for(int j=0; j<N; ++j){
				///De la cara j de vN[i]
				f += vN[vE[eAs[j]][0]].p;	//Cuando creamos los nuevos elementos pusimos primero el centroide
				r += vN[vE[eAs[j]][1]].p;	//Cuando creamos los nuevos elementos pusimos segundo una arista (sumamos una para no repetir)
			}
			
			f = f*(1.f/N);
			r = r*(1.f/N);
			
			///Calcular nueva posicion (4r-f+(n-3)p)/n
			vN[i].p = (4.f*r - f +(N-3.f)*vN[i].p)*(1.f/N);
		}else{
			glm::vec3 r(0,0,0);
			
			std::vector<int>& eAs = vN[i].e;
			///Recorremos las caras a las que pertenece el nodo
			for(int j=0; j<eAs.size(); ++j){
				///Por cada cara, sumamos las aristas vecinas que sean frontera
				if(vN[vE[eAs[j]][1]].es_frontera)
					r += vN[vE[eAs[j]][1]].p;
				if(vN[vE[eAs[j]][-1]].es_frontera)
					r += vN[vE[eAs[j]][-1]].p;
			}
			
			r = r*0.5f;
			
			///Calcular nueva posicion(r+p)/2
			vN[i].p = (r+vN[i].p)*0.5f;
		}
	}
	
}
