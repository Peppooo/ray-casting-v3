#define SDL_MAIN_HANDLED
#include <iostream>
#include <SDL2/SDL.h>
#include <math.h>
#include <vector>
#include <thread>
#include <imgui.h>
#include "imgui_sdl.h"

using namespace std;

float W,H;

bool fast_square_root = true;

SDL_Window* window;
SDL_Renderer* renderer;

float map(float v,float m,float r) {
	return (v / m) * r;
}

float rad(float deg) {
	return deg * (M_PI / 180);
}

float deg(float rad) {
	return rad / (M_PI / 180);
}

int rrand(int max) {
	return rand() % max;
}

float approx(float v,float k) {
	return (ceilf(v * k) / k);
}

float fsqrt(float v,int steps) {
	double a = (v + 1) / 3;
	for(int i = 0; i < steps; i++) {
		a = (a + (v / a)) / 2;
	}
	return a;
}

float steps_sqrt = 10;

float dsqrt(float k) {
	if(fast_square_root) {
		return fsqrt(k,steps_sqrt);
	}
	else {
		return sqrtf(k);
	}
}
int random(int max,int min) {
	int range = max - min + 1;
	return rand() % range + min;
}

struct vec2 {
	float x,y;
	vec2 operator+(vec2 obj) {
		return {x + obj.x,y + obj.y};
	}
	vec2 operator-(vec2 obj) {
		return {x - obj.x,y - obj.y};
	}
	float distance(vec2 p) {
		return dsqrt(pow(p.x - x,2) + pow(p.y - y,2));
	};
	float dot(vec2 p) {
		return p.x * x + p.y * y;
	}
};

struct vec3 {
	float x,y,z;
};

#define OBJ_SPHERE 0
#define OBJ_QUAD 1

class obj {
public:
	vec2 center;
	vec2 size;
	SDL_Color c;
	int type = 0;
	obj(vec2 center,vec2 size,SDL_Color c,int type):center(center),size(size),c(c),type(type) {}
	bool cast(vec2 p) {
		float w = size.x / 2;
		float h = size.y / 2;
		return ((p.x <= center.x + w) && (p.x >= center.x - w) && (p.y <= center.y + h) && (p.y >= center.y - h));
	}
	bool hit(vec2 origin,vec2 direction) {
		return (abs(vec2{center - origin}.dot({-direction.y,direction.x})) <= size.x);
	}
	void draw() {
		SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
		SDL_Rect r = {center.x - size.x / 2,center.y - size.y / 2,size.x,size.y};
		SDL_RenderFillRect(renderer,&r);
	}
};

vector<obj*> scene;

int frames = 0;

class light {
public:
	vec2 position;
	float max_length;
	light(vec2 position,float max_length):position(position),max_length(max_length) {}
	float visible(vec2 from,float step) {
		if(from.x == position.x && from.y == position.y) {
			return 1;
		}
		float d1 = from.distance(position);
		vec2 step_f = {((from.x - position.x) / d1)*step,((from.y - position.y) / d1)*step};
		vec2 step_pos = position;
		float d_f = 0;
		if(d1 >= max_length) {
			return 0;
		}
		while(1) {
			d_f = step_pos.distance(position) - step;
			step_pos = step_pos + step_f;
			if(d_f >= d1 || d_f >= max_length) {
				// out of range
				float normalized = map(d_f,max_length,1);
				if(normalized >= 1) {
					normalized = 1;
				}
				return abs(normalized-1 );
			}
			for(int i = 0; i < scene.size(); i++) {
				if(scene[i]->cast(step_pos)) {
					// intersect with an object
					return 0;
				}
			}
		}
	}
};

double e = M_E;

float focal_step(float x,float m,float t,float z,float s) {
	return ((m)-((m - t) * powf(e,-z * powf((e * x),2 * (10 * (1 - s + 0.1))))));
}

class camera {
private:
public:
	vec2 center;
	float angle;
	float fov;
	int rays_max;
	float far;
	float step;
	bool focus = false;
	float focal_min_step;       // t
	float focal_max_step;       // m
	float focal_max_area;       // z
	float focal_area_smoothing; // s
	float height = 0;
	bool map_dist = true;
	bool rendering = false;
	bool cast_lights = true;
	float light_casting_step = 1;
	float percentage = 0;
	light* light_1;
	camera(vec2 ncenter,float angle,float fov,float rays_max,float far,float focal_min_step,float focal_max_step,float focal_max_area,float focal_area_smoothing,float nstep,float nstep_light,light* light,bool flat_surf = true):center(ncenter),angle(angle),fov(fov),rays_max(rays_max),far(far),focal_min_step(focal_min_step),focal_max_step(focal_max_step),focal_max_area(focal_max_area),focal_area_smoothing(focal_area_smoothing),step(nstep),light_casting_step(nstep_light),light_1(light) {
		//steps_max = (int)ceil(far/step);
	}
	void render(bool& round_rays,bool debug = false) {
		if(debug) {
			for(int i = 0; i < scene.size(); i++) {
				scene[i]->draw();
			}
		}
		rendering = true;
		if(fov > 135) {
			round_rays = true;
		}
		float max_dist = 0;
		for(float x = angle - fov / 2; x < angle + fov / 2; x += fov / rays_max) {
			//int steps = 0;
			bool intersected = false;
			vec2 step_pos = center;
			max_dist = dsqrt(powf(far,2) + powf(far * tanf(rad(x - angle)),2));
			//steps_max_fixed = (int)ceilf(dsqrt(pow(far,2) + pow(far * tan(rad(x - angle)),2))/step;
			float dx = sinf(rad(x));
			float dy = cosf(rad(x));
			float c_step = step;
			if(focus) {
				c_step = focal_step((2 * (x - angle)) / fov,focal_max_step,focal_min_step,focal_max_area,focal_area_smoothing);
			}
			for(auto c : scene) {
				if(c->hit(this->center,{dx,dy})) {
					break;
				} else {
					goto skip_ray;
				}
			}
			while(!intersected) {
				step_pos = step_pos + vec2{dx*c_step,dy*c_step};
				float dist = step_pos.distance(center);
				if(dist >= (round_rays?far:max_dist)) {
					intersected = true;
					break;
				}
				for(int i = 0; i < scene.size(); i++) {
					if(scene[i]->cast(step_pos))
					{
						intersected = true;

						float norm = abs(map(dist,map_dist?max_dist:far,1)-1);
						float norm_light = 1;
						if(cast_lights) {
							norm_light = light_1->visible(step_pos - vec2{(dx*c_step)*10,(dy*c_step)*10},light_casting_step);
						}
						float h = H*norm;
						Uint8 alpha = (255*(cast_lights?norm_light:norm));
						float X = map(x - angle + fov / 2,fov,W);

						SDL_Color* c = &scene[i]->c;
						float mid = H / 2 + height;
						if(!debug) {
							SDL_SetRenderDrawColor(renderer,c->r,c->g,c->b,alpha);
							SDL_RenderDrawLine(renderer,X,mid + h / 2,X,mid - h / 2);
						}
						break;
					}
				}
				if(debug) {
					SDL_SetRenderDrawColor(renderer,255,255,255,255);
					SDL_RenderDrawPoint(renderer,step_pos.x,step_pos.y);
				}
			}
		skip_ray:continue;
		}
		rendering = false;
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
	srand(time(0));
	W = 1920;
	H = 1080;
	SDL_Init(SDL_INIT_EVERYTHING);
	SDL_CreateWindowAndRenderer(W,H,SDL_WINDOW_RESIZABLE,&window,&renderer);

	light l1({1920 / 2,1080 / 2},100);


	camera cam({W/2-100,H/2},90,110,W,300,0.4,2,.5,1,.5,.5,&l1,true);

	//thread stats_t = thread(stats,&cam);

	fast_square_root = false;

	//thread t1(&stats,&cam);

	scene.push_back(new obj({W / 2 + 60,H / 2 - 60},{30,30},{0,0,0,255},OBJ_QUAD));
	scene.push_back(new obj({W / 2 + 60,H / 2},{30,30},{0,0,0,255},OBJ_QUAD));
	scene.push_back(new obj({W / 2 + 60,H / 2 + 60},{30,30},{0,0,0,255},OBJ_QUAD));

	scene.push_back(new obj({W / 2 + 30,H / 2 - 30},{30,30},{0,0,0,255},OBJ_QUAD));
	scene.push_back(new obj({W / 2 + 30,H / 2 + 30},{30,30},{0,0,0,255},OBJ_QUAD));
	
	for(int i = 0; i < scene.size(); i++) {
		scene[i]->c = {(Uint8)random(255,0),(Uint8)random(255,0),(Uint8)random(255,0),255};
	}

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
			else if(e.key.keysym.scancode == SDL_SCANCODE_UP) {
				l1.position.x++;
			}
			else if(e.key.keysym.scancode == SDL_SCANCODE_DOWN) {
				l1.position.x--;
			}
			else if(e.key.keysym.scancode == SDL_SCANCODE_LEFT) {
				l1.position.y++;
			}
			else if(e.key.keysym.scancode == SDL_SCANCODE_RIGHT) {
				l1.position.y--;
			}
			if(e.type == SDL_KEYDOWN) {
				if(e.key.keysym.scancode == SDL_SCANCODE_E) {
					show_mode = !show_mode;
				}
			}
			if(e.type == SDL_MOUSEWHEEL) {
				cam.step += e.wheel.preciseY / 50;
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

		ImGui::Begin("values");
		ImGui::InputFloat("fov",&cam.fov,1,2);
		ImGui::SliderInt("rays",&cam.rays_max,0,2*W);
		ImGui::InputFloat("step",&cam.step,0.01,0.1);
		ImGui::InputFloat("render distance",&cam.far,1,2);
		ImGui::Checkbox("blend shadow",&shadowing_alpha);
		ImGui::Checkbox("cast light",&cam.cast_lights);
		if(cam.cast_lights) {
			ImGui::InputFloat("light step",&cam.light_casting_step,0.01,0.1);
			ImGui::SliderFloat("light brightness",&l1.max_length,1,500);
		}
		ImGui::Checkbox("use fast square root",&fast_square_root);
		ImGui::Checkbox("round rays",&round_rays);
		ImGui::Checkbox("map method (distance)",&cam.map_dist);
		ImGui::Checkbox("focus",&cam.focus);
		if(cam.focus) {
			
		}
		r_convert_scene = ImGui::Button("casting method (spheres/quadrilaterals)");
		ImGui::End();

		if(show_mode) {
			cam.render(round_rays);
		}
		else {
			//cout << l1.visible({(float)x,(float)y},0.1) << endl;
			//SDL_RenderDrawLineF(renderer,l1.position.x,l1.position.y,x,y);
			cam.render(round_rays,true);
			SDL_SetRenderDrawColor(renderer,0,255,0,255);
			SDL_RenderDrawPoint(renderer,l1.position.x,l1.position.y);
		}

		ImGui::Render();
		ImGuiSDL::Render(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}
	//t1.join();
}