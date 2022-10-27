#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "utils.hpp"
#include "Debug.hpp"

BoundingBox::BoundingBox(glm::vec3 &p1, glm::vec3 &p2) 
	: pmin({std::min(p1.x,p2.x),std::min(p1.y,p2.y),std::min(p1.z,p2.z)}),
      pmax({std::max(p1.x,p2.x),std::max(p1.y,p2.y),std::max(p1.z,p2.z)}) 
{
	
}
	
bool BoundingBox::contiene(glm::vec3 &p) const {
	return p.x>=pmin.x && p.x<=pmax.x &&
		p.y>=pmin.y && p.y<=pmax.y &&
		p.z>=pmin.z && p.z<=pmax.z;
}

Pesos calcularPesos(glm::vec3 x0, glm::vec3 x1, glm::vec3 x2, glm::vec3 &x) {
	/// @todo: implementar
	
	float p0,p1,p2;
	
	///Productos cruz
	glm::vec3 a = cross(x1-x0,x2-x0);
	glm::vec3 a0 = cross(x1-x,x2-x);
	glm::vec3 a1 = cross(x2-x,x0-x);
	glm::vec3 a2 = cross(x0-x,x1-x);
	
	///Calcular pesos (mirar la ecuacion del apunte)
	p0 = dot(a0,a)/dot(a,a);
	p1 = dot(a1,a)/dot(a,a);
	p2 = dot(a2,a)/dot(a,a);
	
	return {p0,p1,p2};
}
