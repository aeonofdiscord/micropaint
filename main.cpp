#include <algorithm>
#include <SDL2/SDL.h>

typedef unsigned char u8;
typedef unsigned short u16;

static bool running = true;
static bool dirty = true;
static SDL_Texture* texture;
static u16* internalSurface;
static u8* internal32;
static bool* stroke;

static bool lmb = false;

struct Colour
{
	u16 r, g, b, a;
};
static Colour WHITE = { 0xffff, 0xffff, 0xffff, 0x4000 };

static int surfaceW = 1680;
static int surfaceH = 1050;
enum BrushMode{BRUSH_DRAW, BRUSH_ERASE, BRUSH_MAX};
static int brushMode = BRUSH_DRAW;

Colour readPixel(int x, int y)
{
	u16* i = internalSurface + ((y*surfaceW) + x) * 4;
	Colour c = { i[0], i[1], i[2], i[3] };
	return c;
}

void setPixel(int x, int y, const Colour& c)
{
	if(x < 0 || x >= surfaceW || y < 0 || y >= surfaceH)
		return;
	u16* i = internalSurface + ((y*surfaceW) + x) * 4;
	i[0] = c.r;
	i[1] = c.g;
	i[2] = c.b;
	i[3] = c.a;
	dirty = true;
	stroke[(y*surfaceW) + x] = true;
}

u16 clamp(int lhs, u16 rhs)
{
	if(lhs > rhs)
		return rhs;
	return u16(lhs);
}

Colour blend(const Colour& d, const Colour& s)
{
	using std::min;
	using std::max;
	Colour result;
	float a = s.a / 65535.0f;
	if(brushMode == BRUSH_DRAW)
	{
		result.r = min(d.r + s.r * a, float(0xffff));
		result.g = min(d.g + s.g * a, float(0xffff));
		result.b = min(d.b + s.b * a, float(0xffff));
		result.a = min(d.a + s.a * 1.0f, float(0xffff));
	}
	else if(brushMode == BRUSH_ERASE)
	{
		result.r = max(d.r - s.r * a, 0.0f);
		result.g = max(d.g - s.g * a, 0.0f);
		result.b = max(d.b - s.b * a, 0.0f);
		result.a = max(d.a - s.a * 1.0f, 0.0f);
	}
	return result;
}

void drawPixel(int x, int y, const Colour& c)
{
	Colour dest = readPixel(x, y);
	setPixel(x, y, blend(dest, c));
}

void drawBrush(int x, int y, const Colour& c)
{
	int r = 2;
	for(int py = -r; py <= r; ++py)
	{
		for(int px = -r; px <= r; ++px)
		{
			float d = px*px+py*py;
			if(d<=r*r && !stroke[(y+py)*surfaceW + x+px])
				drawPixel(x+px, y+py, c);
		}
	}
}

void drawLine(int x0, int y0, int x1, int y1)
{
	using std::swap;
	int dx = abs(x1-x0);
	int dy = abs(y1-y0);

	float error = 0;
	if(dx > dy)
	{
		if(x0 > x1)
		{
			swap(x0, x1);
			swap(y0, y1);
		}
		int px = x0;
		int py = y0;
		int yd = 0;
		if(y1 > y0)
			yd = 1;
		else if(y1 < y0)
			yd = -1;

		while(px <= x1)
		{
			drawBrush(px, py, WHITE);
			++px;
			error += dy/float(dx);
			if(error > 1.0f)
			{
				py += yd;
				error -= 1.0f;
			}
		}
	}
	else
	{
		if(y0 > y1)
		{
			swap(y0, y1);
			swap(x0, x1);
		}
		int px = x0;
		int py = y0;
		int xd = 0;
		if(x1 > x0)
			xd = 1;
		else if(x1 < x0)
			xd = -1;
		while(py <= y1)
		{
			drawBrush(px, py, WHITE);
			++py;
			error += dx/float(dy);
			if(error > 1.0f)
			{
				px += xd;
				error -= 1.0f;
			}
		}
	}
}

void clearStroke()
{
	for(int i = 0; i < surfaceW*surfaceH; ++i)
		stroke[i] = false;
}

void handleEvents(SDL_Window* window)
{
	SDL_Event e;
	static int mx, my;
	while(SDL_PollEvent(&e))
	{
		switch(e.type)
		{
		case SDL_QUIT:
			running = false;
			break;
		case SDL_MOUSEBUTTONDOWN:
			if(e.button.button == SDL_BUTTON_LEFT)
			{
				mx = e.button.x;
				my = e.button.y;
				drawBrush(e.button.x, e.button.y, WHITE);
				lmb = true;
			}
			else if(e.button.button == SDL_BUTTON_RIGHT)
			{
				brushMode = (brushMode+1) % BRUSH_MAX;
			}
			break;
		case SDL_MOUSEBUTTONUP:
			if(e.button.button == SDL_BUTTON_LEFT)
			{
				lmb = false;
				clearStroke();
			}
			break;
		case SDL_MOUSEMOTION:
			if(lmb)
			{
				drawLine(mx, my, e.button.x, e.button.y);
				mx = e.button.x;
				my = e.button.y;
			}
			break;
		}
	}
}

void updateTexture()
{
	for(int i = 0; i < surfaceW*surfaceH*4; ++i)
	{
		internal32[i] = (internalSurface[i] >> 8);
	}
	SDL_UpdateTexture(texture, 0, internal32, surfaceW*4);
}

void draw(SDL_Renderer* r)
{
	if(dirty)
	{
		updateTexture();
		dirty = false;
	}
	SDL_RenderCopy(r, texture, 0, 0);
	SDL_RenderPresent(r);
}

int main(int argc, char** argv)
{
	SDL_Init(SDL_INIT_VIDEO);
	SDL_Window* window = SDL_CreateWindow("microPaint", 0, 0, surfaceW, surfaceH, 0);
	SDL_Renderer* renderer = SDL_CreateRenderer(window, 0, 0);
	texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, surfaceW, surfaceH);
	internalSurface = new u16[surfaceW*surfaceH*4];
	internal32 = new u8[surfaceW*surfaceH*4];
	stroke = new bool[surfaceW*surfaceH];
	while(running)
	{
		handleEvents(window);
		draw(renderer);
		SDL_Delay(10);
	}

	delete [] stroke;
	delete [] internal32;
	delete [] internalSurface;

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
