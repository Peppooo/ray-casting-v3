#define SDL_MAIN_HANDLED
#include <iostream>
#include <SDL2/SDL.h>
#include <math.h>
#include <vector>
#include <thread>
#include <imgui.h>
#include "imgui_sdl.h"

using namespace std;

double W,H;

bool fast_square_root = true;

SDL_Window* window;
SDL_Renderer* renderer;

double map(double v,double m,double r) {
	return (v / m) * r;
}

double rad(double deg) {
	return deg * (M_PI / 180);
}

double deg(double rad) {
	return rad / (M_PI / 180);
}

int rrand(int max) {
	return rand() % max;
}

float fsqrt(float n) {

	const float threehalfs = 1.5F;
	float y = n;

	long i = *(long*)&y;

	i = 0x5f3759df - (i >> 1);
	y = *(float*)&i;

	y = y * (threehalfs - ((n * 0.5F) * y * y));

	return 1 / y;
}

double dsqrt(double k) {
	if(fast_square_root) {
		return fsqrt(k);
	}
	else {
		return sqrt(k);
	}
}
int random(int max,int min) {
	int range = max - min + 1;
	return rand() % range + min;
}

struct vec2 {
	double x,y;
	vec2 operator+(vec2 obj) {
		return {x + obj.x,y + obj.y};
	}
	double distance(vec2 p) {
		return dsqrt(pow(p.x - x,2) + pow(p.y - y,2));
	};
};

struct vec3 {
	double x,y,z;
};

#define OBJ_SPHERE 0
#define OBJ_QUAD 1

class obj {
public:
	vec2 center;
	vec2 size;
	SDL_Color c;
	int type = 0;
	obj(vec2 center,vec2 size,SDL_Color c,int type):center(center),size(size),type(type) {}
	bool cast(vec2 p) {
		if(type == OBJ_QUAD)
		{
			double w = size.x / 2;
			double h = size.y / 2;
			return ((p.x <= center.x + w) && (p.x >= center.x - w) && (p.y <= center.y + h) && (p.y >= center.y - h));
		}
		else if(type == OBJ_SPHERE) {
			return (p.distance(center) < size.x / 2);
		}
		return false;
	}
	void draw() {
		SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
		SDL_Rect r = {center.x - size.x / 2,center.y - size.y / 2,size.x,size.y};
		SDL_RenderFillRect(renderer,&r);
	}
};

vector<obj*> scene;

int frames = 0;

class camera {
private:
	int steps_max;
	int steps_max_fixed;
public:
	vec2 center;
	double angle;
	double fov;
	double rays_max;
	double renderdistance;
	double step;
	bool map_dist = false;
	bool rendering = false;
	double percentage = 0;
	camera(vec2 ncenter,double angle,double fov,double rays_max,double renderdistance,double nstep,bool flat_surf = true):center(ncenter),angle(angle),fov(fov),rays_max(rays_max),renderdistance(renderdistance),step(nstep) {
		steps_max = (int)ceil(renderdistance/step);
	}
	void render(bool& round_rays) {
		rendering = true;
		if(fov > 135) {
			round_rays = true;
		}
		double max_dist = 0;
		for(double x = angle - fov / 2; x < angle + fov / 2; x += fov / rays_max) {
			bool intersected = false;
			vec2 step_pos = center;
			if(!round_rays) {
				//max_dist = dsqrt(pow(renderdistance,2) + pow(renderdistance * tan(rad(x - angle)),2));
				steps_max_fixed = (int)ceilf(dsqrt(pow(renderdistance,2) + pow(renderdistance * tan(rad(x - angle)),2))/step);
			}
			while(!intersected) {
				step_pos = step_pos + vec2{sin(rad(x))*step,cos(rad(x))*step};
				double dist = step_pos.distance(center);
				if(dist >= (round_rays?renderdistance:max_dist)) {
					intersected = true;
					break;
				}
				for(int i = 0; i < scene.size(); i++) {
					if(scene[i]->cast(step_pos))
					{
						intersected = true;

						double norm = abs(map(dist,map_dist?max_dist:renderdistance,1)-1);
						double h = H*norm;
						Uint8 alpha = 255*norm;
						double X = map(x - angle + fov / 2,fov,W);

						SDL_Color* c = &scene[i]->c;
						SDL_SetRenderDrawColor(renderer,c->r,c->g,c->b,alpha);
						SDL_RenderDrawLineF(renderer,X,H / 2 + h / 2,X,H / 2 - h / 2);
						break;
					}
				}
			}
			percentage = map(x - angle + fov / 2,fov,100);
		}
		percentage = 0.0;
		rendering = false;
	}
	void debug(bool round_rays = false) {
		for(int j = 0; j < scene.size(); j++) {
			scene[j]->draw();
		}
		SDL_SetRenderDrawColor(renderer,255,255,255,255);
		double max_dist = 0;
		for(double x = angle - fov / 2; x < angle + fov / 2; x += fov / rays_max) {
			bool intersected = false;
			vec2 step_pos = center;
			max_dist = dsqrt(pow(renderdistance,2) + pow(renderdistance * tan(rad(x - angle)),2));
			while(!intersected) {
				step_pos = step_pos + vec2{sin(rad(x)) * step,cos(rad(x)) * step};
				if(step_pos.distance(center) >= (round_rays ? renderdistance : max_dist)) {
					intersected = true;
					break;
				}
				for(int i = 0; i < scene.size(); i++) {
					if(scene[i]->cast(step_pos))
					{
						intersected = true;
						break;
					}
				}
				SDL_RenderDrawPoint(renderer,step_pos.x,step_pos.y);
			}
		}
	}
};

void convert_scene() {
	for(int i = 0; i < scene.size(); i++)
	{
		scene[i]->type = scene[i]->type == OBJ_SPHERE ? OBJ_QUAD : OBJ_SPHERE;
	}
}

void stats(camera* c) {
	while(1) {
		if(c->rendering) {
			cout << "rendering: " << c->percentage << "\r";
		}
	}
}

int main() {
	W = 512;
	H = 512;
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_CreateWindowAndRenderer(W,H,SDL_WINDOW_RESIZABLE,&window,&renderer);

	camera cam({W/2,H/2},90,60,W,100,.5,true);
	
	thread t1(&stats,&cam);

	scene.push_back(new obj({W / 2 + 60,H / 2 - 60},{30,30},{0,0,0,255},OBJ_QUAD));
	scene.push_back(new obj({W / 2 + 60,H / 2},{30,30},{0,0,0,255},OBJ_QUAD));
	scene.push_back(new obj({W / 2 + 60,H / 2 + 60},{30,30},{0,0,0,255},OBJ_QUAD));

	scene.push_back(new obj({W / 2 + 30,H / 2 - 30},{30,30},{0,0,0,255},OBJ_QUAD));
	scene.push_back(new obj({W / 2 + 30,H / 2 + 30},{30,30},{0,0,0,255},OBJ_QUAD));

	scene[0]->c = {255,0,0,255};
	scene[1]->c = {0,255,0,255};
	scene[2]->c = {0,0,255,255};

	scene[3]->c = {0,0,255,255};
	scene[4]->c = {255,0,0,255};
	

	/*for(int i = 0; i < 100; i++) {
		scene.push_back(new obj{{}});
	}*/

	SDL_Event e;

	bool show_mode = true;
	bool round_rays = false;

	Uint32 buttons;

	int x,y;

	ImGui::CreateContext();
	ImGuiSDL::Initialize(renderer,W,H);

	int wheel = 0;

	bool shadowing_alpha = true;

	bool r_convert_scene = false;


	while(1) {
		SDL_SetRenderDrawBlendMode(renderer,shadowing_alpha ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
		ImGuiIO& io = ImGui::GetIO();
		SDL_SetRenderDrawColor(renderer,0,0,0,255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer,255,255,255,255);

		buttons = SDL_GetMouseState(&x,&y);

		int WI,HI;

		SDL_GetWindowSize(window,&WI,&HI);

		W = WI,H=HI;

		while(SDL_PollEvent(&e))
		{
			if(e.type == SDL_QUIT) return 0;
			else if(e.type == SDL_WINDOWEVENT)
			{
				if(e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
				{
					io.DisplaySize.x = static_cast<float>(e.window.data1);
					io.DisplaySize.y = static_cast<float>(e.window.data2);
					W = e.window.data1;
					cam.rays_max = W;
				}
			}
			else if(e.type == SDL_MOUSEWHEEL)
			{
				wheel = e.wheel.y;
			}
			if(e.key.keysym.scancode == SDL_SCANCODE_W) {
				cam.center.x += sin(rad(cam.angle));
				cam.center.y += cos(rad(cam.angle));
			}
			else if(e.key.keysym.scancode == SDL_SCANCODE_S) {
				cam.center.x += -sin(rad(cam.angle));
				cam.center.y += -cos(rad(cam.angle));
			}
			else if(e.key.keysym.scancode == SDL_SCANCODE_D) {
				cam.angle++;
			}
			else if(e.key.keysym.scancode == SDL_SCANCODE_A) {
				cam.angle--;
			}
			if(e.type == SDL_KEYDOWN) {
				if(e.key.keysym.scancode == SDL_SCANCODE_E) {
					show_mode = !show_mode;
				}
				if(e.key.keysym.scancode == SDL_SCANCODE_Q) {
					r_convert_scene = true;
				}
				if(e.key.keysym.scancode == SDL_SCANCODE_R) {
					round_rays = !round_rays;
				}
				if(e.key.keysym.scancode == SDL_SCANCODE_G) {
					fast_square_root = !fast_square_root;
				}
			}
			if(e.type == SDL_MOUSEWHEEL) {
				cam.step += e.wheel.preciseY / 50;
			}
			if(!show_mode) {
				if(e.type == SDL_MOUSEBUTTONDOWN) {
					if(buttons == SDL_BUTTON_LEFT) {
						int i = scene.size();
						scene.push_back(new obj({(double)x,(double)y},{30,30},{},OBJ_QUAD));
						scene[i]->c = {(Uint8)rrand(255),(Uint8)rrand(255),(Uint8)rrand(255),255};
					}
				}
			}
		}
	
		if(r_convert_scene) {
			convert_scene();
			r_convert_scene = false;
		}
		

		io.DeltaTime = 1.0f / 60.0f;
		io.MousePos = ImVec2(static_cast<float>(x),static_cast<float>(y));
		io.MouseDown[0] = buttons & SDL_BUTTON(SDL_BUTTON_LEFT);
		io.MouseDown[1] = buttons & SDL_BUTTON(SDL_BUTTON_RIGHT);
		io.MouseWheel = static_cast<float>(wheel);
		ImGui::NewFrame();

		ImGui::Begin("Values");
		ImGui::InputDouble("fov",&cam.fov,1,2);
		ImGui::InputDouble("rays",&cam.rays_max,1,2);
		ImGui::InputDouble("step",&cam.step,0.01,0.1);
		ImGui::InputDouble("render distance",&cam.renderdistance,1,2);
		ImGui::Checkbox("blend shadow",&shadowing_alpha);
		ImGui::Checkbox("use fast square root",&fast_square_root);
		ImGui::Checkbox("round rays",&round_rays);
		ImGui::Checkbox("map method (distance)",&cam.map_dist);
		r_convert_scene = ImGui::Button("casting method (spheres/quadrilaterals)");
		ImGui::End();

		if(show_mode) {
			cam.render(round_rays);
		}
		else {
			cam.debug(round_rays);
		}

		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}
	t1.join();
}