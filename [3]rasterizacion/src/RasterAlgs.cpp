#include <forward_list>
#include <iostream>
#include <GL/gl.h>
#include <cmath>
#include "RasterAlgs.hpp"

/*
	paintPixel dibuja el pixel en la coordenada que le digas.
	p0 es el punto inicial
	p1 es el punto final
*/
void drawSegment(paintPixelFunction paintPixel, glm::vec2 p0, glm::vec2 p1) {
	/// @todo: implementar algun algoritmo de rasterizacion de segmentos 
	
	//Vector de p0 a p1
	glm::vec2 vDirec = p1-p0;
	
	//Pixeles exactos para dibujar la recta (dx si es mas horizontal, dy si es mas vertical)
	int cantPixel = std::max(fabs(vDirec.x),fabs(vDirec.y));
	
	//Incremento en la recta de un pixel al siguiente
	float dt = 1.f/cantPixel;
	
	//vector incremento de un punto al siguiente
	glm::vec2 dV = dt*vDirec;
	
	//Inicio en p0
	glm::vec2 p = p0;
	
	for(int i=0; i<cantPixel; ++i){
		//Redondeamos las coordenadas 
		paintPixel(glm::vec2(round(p.x),round(p.y)));
		
		p += dV;
	}
}

#if 1

/*
paintPixel dibuja el pixel en la coordenada
evalCurve es una funcion paramétrica que devuelve una coordenada 2d
*/
void drawCurve(paintPixelFunction paintPixel, curveEvalFunction evalCurve) {
	/// @todo: implementar algun algoritmo de rasterizacion de curvas
	
	//DDA de curvas
	float t=0.f;
	
	//Bandera para evitar repintar
	bool pintar = true;
	
	/*
	t: parametro que recibe la funcion parametrica de la curva, devuelve un par <punto, derivada>
	t = [0.0, 1.0]
	t=0.0 punto inicial
	t=1.0 punto final
	*/
	while(t <= 1.f){
		
		//p = par (punto, derivada) del punto evaluado
		auto p = evalCurve(t);
		
		//Pintar pixel donde cae p (recondeado)
		if(pintar)
			paintPixel(glm::vec2(round(p.p.x),round(p.p.y)));
		
		pintar = true;
		
		//t2: proximo t
		float t2 = t;
		//dt: incremento en t (t2 - t)
		float dt = 0.f;
		
		//Evaluamos hacia donde crece mas la curva (p.d es la derividada p.p de la curva)
		if(fabs(p.d.x)>fabs(p.d.y)){
			//Candidato a incremento
			dt = 1.f/fabs(p.d.x);
		}
		else{
			dt = 1.f/fabs(p.d.y);
		}
		
		///Correccion para no saltear pixeles
		//Se ejecuta hasta que el incremento es lo suficientemente chico
		//Suficientemente chico:
		//- No crece 2 o mas pixeles en x
		//- No crece 2 o mas pixeles en y
		do{
			//Definimos t2
			t2 = t + dt;
			
			//Refinamos dt (se aplica si se ejecuta una iteracion mas)
			dt *= 0.5f;
			
			//Se vuelve a ejecutar si la diferencia entre el redondeo de p.p en t y en t2 es mayor a 1 (se saltea un pixel en x y/o y)
		}while(abs(round(evalCurve(t2).p.x) - round(p.p.x)) > 1 || abs(round(evalCurve(t2).p.y) - round(p.p.y)) > 1);
		
		///Correccion de repintado (si el redonde de p.p en t y en t2 son iguales)
		if(glm::vec2(round(evalCurve(t2).p.x),round(evalCurve(t2).p.y)) == glm::vec2(round(p.p.x),round(p.p.y))){
			pintar = false;
		}
		
		t = t2;
	}
}

#else

///Subdivision (corregir)
void drawCurve(paintPixelFunction paintPixel, curveEvalFunction evalCurve, float t0, float t1){
	//Punto medio de la secante
	glm::vec2 secMed = (evalCurve(t0).p + evalCurve(t1).p) * 0.5f;
	
	//Punto medio de la curva
	float tmed = (t0+t1)/2.f;
	glm::vec2 curveMed = evalCurve(tmed).p;
	
	glm::vec2 secm= normalize(evalCurve(t1).p - evalCurve(t0).p);
	glm::vec2 tmedm= normalize(evalCurve(tmed).d);
	//Ver si es lo suficientemente copada(la distancia entre los puntos medio es menor a 1 y la pendiente en tmed es parecida a la pendiente de la secante)
	if(dot(secm,tmedm) < 0.9f){
		drawCurve(paintPixel, evalCurve, t0, tmed);
		drawCurve(paintPixel, evalCurve, tmed, t1);
	}else if(distance(secMed, curveMed) >= 1){
		drawCurve(paintPixel, evalCurve, t0, tmed);
		drawCurve(paintPixel, evalCurve, tmed, t1);
	}else{
		drawSegment(paintPixel, evalCurve(t0).p, evalCurve(t1).p);
	}
}
	
	//Wrapper para subdivision
	void drawCurve(paintPixelFunction paintPixel, curveEvalFunction evalCurve) {
		drawCurve(paintPixel, evalCurve, 0.f, 1.f);
	}
	
#endif
	
