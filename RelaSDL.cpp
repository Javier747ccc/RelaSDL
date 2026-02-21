#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>
#include <iostream>           // For cout and cerr
#include <string>
#include <cctype>
#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include "SDL.h"
#include "SDL_TTF.h"
#include "SDL_thread.h"
#include "RelaUtiles.h"
#include "PracticalSocket.h"
#include <SDL3/SDL_main.h>
#include <yaml-cpp/yaml.h>



const int kWindowCount = 3;
const int kColumnsPerWindow = 3;
const int kTotalColumns = kWindowCount * kColumnsPerWindow;
const double kVelocityLimit = 0.99;

int PanWidth = 1024;
int PanHeight = 1024;
SDL_Surface *screen;

TTF_Font* fuentes[10] = {};
int baseFontSizes[10] = {};
int currentFontSizes[10] = {};
float renderScales[kWindowCount] = { 1.0f, 1.0f, 1.0f };
float currentRenderScale = 1.0f;

SDL_Renderer *image;
SDL_Renderer *renderers[kWindowCount] = {};
SDL_Window *windows[kWindowCount] = {};
SDL_WindowID windowIds[kWindowCount] = {};

double Times[1024];
double Factors[1024];
double Velocidades[kTotalColumns];
bool Pause = true;
bool NextStep = false;
int SelectedGauge[kWindowCount] = { 0, 0, 0 };

struct AppEvent {
	std::string type;
	char column;
	double time;
	double amount;
	bool triggered;
	int triggeredMask;
};

std::vector<AppEvent> eventos;

/*
double Lorentz(double v)
{
	const double c = 1;
	double tc = sqrt(1-(v*v)/(c*c));
	return tc;

}
*/

static double FactorFromVelocity(double v)
{
	if (v < -kVelocityLimit) {
		v = -kVelocityLimit;
	} else if (v > kVelocityLimit) {
		v = kVelocityLimit;
	}
	double term = 1.0 - (v * v);
	if (term < 1.0e-6) {
		term = 1.0e-6;
	}
	if (v < 0.0) {
		return 1.0 / sqrt(term);
	}
	return sqrt(term);
}

static double VelocityFromFactor(double factor)
{
	if (factor <= 0.0) {
		return 0.0;
	}
	double inv = 1.0 / factor;
	double inv2 = inv * inv;
	if (factor >= 1.0) {
		double term = 1.0 - inv2;
		if (term <= 0.0) {
			return 0.0;
		}
		return sqrt(term);
	}
	return sqrt(1.0 + inv2);
}

static char GetLabelForWindowColumn(int windowIndex, int columnIndex)
{
	static const char labels[kWindowCount][kColumnsPerWindow] = {
		{ 'B', 'A', 'C' },
		{ 'A', 'B', 'C' },
		{ 'A', 'C', 'B' }
	};
	return labels[windowIndex][columnIndex];
}

static int GetIndexForLabelInWindow(int windowIndex, char label)
{
	char target = (char)toupper((unsigned char)label);
	if (windowIndex < 0 || windowIndex >= kWindowCount) {
		return -1;
	}
	for (int c = 0; c < kColumnsPerWindow; c++) {
		if (GetLabelForWindowColumn(windowIndex, c) == target) {
			return windowIndex * kColumnsPerWindow + c;
		}
	}
	return -1;
}

static bool LoadEventosFromYaml(const char* filePath)
{
	try {
		YAML::Node config = YAML::LoadFile(filePath);
		if (!config["eventos"] || !config["eventos"].IsSequence()) {
			return false;
		}
		eventos.clear();
		for (const auto& node : config["eventos"]) {
			if (!node.IsMap()) {
				continue;
			}
			const YAML::Node tipoNode = node["tipo"];
			const YAML::Node columnaNode = node["columna"];
			const YAML::Node tiempoNode = node["tiempo"];
			if (!tipoNode.IsDefined() || !columnaNode.IsDefined() || !tiempoNode.IsDefined()) {
				continue;
			}
			std::string type = tipoNode.as<std::string>();
			std::string col = columnaNode.as<std::string>();
			if (col.empty()) {
				continue;
			}
			char column = col[0];
			double time = tiempoNode.as<double>();
			double amount = 0.0;
			if (type == "cambio") {
				const YAML::Node cantidadNode = node["cantidad"];
				if (!cantidadNode.IsDefined()) {
					continue;
				}
				amount = cantidadNode.as<double>();
			}
			AppEvent ev { type, column, time, amount, false, 0 };
			eventos.push_back(ev);
		}
		return !eventos.empty();
	} catch (const std::exception& ex) {
		SDL_Log("Fallo al cargar config.yaml: %s", ex.what());
	}
	return false;
}

static void ResetEventos()
{
	for (auto& ev : eventos) {
		ev.triggered = false;
		ev.triggeredMask = 0;
	}
}

static bool ColumnReached(char column, double time)
{
	char target = (char)toupper((unsigned char)column);
	int w = 0;
	for (int c = 0; c < kColumnsPerWindow; c++) {
		if (GetLabelForWindowColumn(w, c) == target) {
			int idx = w * kColumnsPerWindow + c;
			if (Times[idx] >= time) {
				return true;
			}
		}
	}
	return false;
}

static bool ColumnReachedInWindow(int windowIndex, char column, double time)
{
	if (windowIndex < 0 || windowIndex >= kWindowCount) {
		return false;
	}
	char target = (char)toupper((unsigned char)column);
	for (int c = 0; c < kColumnsPerWindow; c++) {
		if (GetLabelForWindowColumn(windowIndex, c) == target) {
			int idx = windowIndex * kColumnsPerWindow + c;
			return Times[idx] >= time;
		}
	}
	return false;
}

static void LoadEventos();

static void ApplyVelocityDelta(int index, double delta)
{
	Velocidades[index] += delta;
	if (Velocidades[index] < -kVelocityLimit) {
		Velocidades[index] = -kVelocityLimit;
	}
	if (Velocidades[index] > kVelocityLimit) {
		Velocidades[index] = kVelocityLimit;
	}
	Factors[index] = FactorFromVelocity(Velocidades[index]);
}

static void ApplyDeltaToLabelInWindow(int windowIndex, char column, double delta)
{
	if (windowIndex < 0 || windowIndex >= kWindowCount) {
		return;
	}
	char target = (char)toupper((unsigned char)column);
	for (int c = 0; c < kColumnsPerWindow; c++) {
		if (GetLabelForWindowColumn(windowIndex, c) == target) {
			int idx = windowIndex * kColumnsPerWindow + c;
			ApplyVelocityDelta(idx, delta);
			break;
		}
	}
}

static void AdjustSelectedVelocity(int windowIndex, double delta)
{
	if (windowIndex < 0 || windowIndex >= kWindowCount) {
		return;
	}
	int selectedColumn = SelectedGauge[windowIndex];
	char label = GetLabelForWindowColumn(windowIndex, selectedColumn);
	int idx = windowIndex * kColumnsPerWindow + selectedColumn;
	ApplyVelocityDelta(idx, delta);
	for (int w = 0; w < kWindowCount; w++) {
		if (w == windowIndex) {
			continue;
		}
		for (int c = 0; c < kColumnsPerWindow; c++) {
			if (GetLabelForWindowColumn(w, c) == label) {
				int otherIdx = w * kColumnsPerWindow + c;
				ApplyVelocityDelta(otherIdx, -delta);
			}
		}
	}
}

static void ResetState()
{
	Pause = true;
	NextStep = false;
	for (int i = 0; i < kWindowCount; i++) {
		SelectedGauge[i] = 1;
	}
	for (int i = 0; i < 1024; i++) {
		Times[i] = 0;
		Factors[i] = 1;
	}
	for (int i = 0; i < kWindowCount; i++) {
		int base = i * kColumnsPerWindow;
		Velocidades[base + 0] = 0.0;
		Velocidades[base + 1] = 0.0;
		Velocidades[base + 2] = 0.0;
	}
	for (int i = 0; i < kTotalColumns; i++) {
		Factors[i] = FactorFromVelocity(Velocidades[i]);
	}
	ResetEventos();
}

static void quit(const char* msg)
{
	SDL_Log("ERROR: %s", msg);
	SDL_Log("SDL_GetError: %s", SDL_GetError());
	exit(1);
}

static float GetRenderScale(SDL_Renderer* renderer)
{
	int outW = 0;
	int outH = 0;
	SDL_GetCurrentRenderOutputSize(renderer, &outW, &outH);
	if (outW <= 0 || outH <= 0) {
		return 1.0f;
	}
	float scaleX = (float)outW / (float)PanWidth;
	float scaleY = (float)outH / (float)PanHeight;
	return (scaleX < scaleY) ? scaleX : scaleY;
}

static void UpdateFontsForScale(float scale)
{
	for (int i = 0; i < 10; i++) {
		int sizePt = (int)(baseFontSizes[i] * scale + 0.5f);
		if (sizePt < 1) {
			sizePt = 1;
		}
		if (sizePt == currentFontSizes[i]) {
			continue;
		}
		if (fuentes[i] != NULL) {
			TTF_CloseFont(fuentes[i]);
			fuentes[i] = NULL;
		}
		fuentes[i] = TTF_OpenFont("Arial-Rounded-MT-Bold.ttf", sizePt);
		if (fuentes[i] == NULL) {
			printf("Fallo al abrir la fuente");
			exit(1);
		}
		TTF_SetFontStyle(fuentes[i], TTF_STYLE_NORMAL);
		currentFontSizes[i] = sizePt;
	}
}

int InitSdl()
{
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		quit("SDL init failed");
	}

	const char* titles[kWindowCount] = { "Particula A", "Particula B", "Particula C" };
	for (int i = 0; i < kWindowCount; i++) {
		const char* title = titles[i];
		if (!SDL_CreateWindowAndRenderer(title, PanWidth, PanHeight, SDL_WINDOW_RESIZABLE, &windows[i], &renderers[i])) {
			SDL_Log("Fallo en SDL_CreateWindowAndRenderer: %s", SDL_GetError());
			return SDL_APP_FAILURE;
		}
		SDL_SetRenderLogicalPresentation(renderers[i], PanWidth, PanHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX);
		windowIds[i] = SDL_GetWindowID(windows[i]);
	}

    if (TTF_Init() == -1)
    {
        printf("Fallo al inicializar SDL_TTF");
        exit(1);
    }

	double ph = (double)PanHeight;
	for (int i = 0; i < 10; i++)
	{
		baseFontSizes[i] = (int)(((double)i * 1.3) * (ph / 256.0)) + 1;
		currentFontSizes[i] = 0;
	}

	renderScales[0] = GetRenderScale(renderers[0]);
	currentRenderScale = renderScales[0];
	UpdateFontsForScale(currentRenderScale);

	char StrTemp[256];
	sprintf(StrTemp,"RELA");
 	SDL_SetAppMetadata(StrTemp, "1.0", StrTemp);


    srand( (unsigned)GetTickCount64() );
    return 0;
}

void DrawSurfText(SDL_Renderer* surf, char* Texto, int X, int Y, TTF_Font* fuente, SDL_Color color)
{
	SDL_Surface* surface = TTF_RenderText_Blended(fuente, Texto, strlen(Texto), color); // TODO: destroy surface?
	SDL_Texture* texture = SDL_CreateTextureFromSurface(surf, surface); // ! Crashed on/before? shutdown from thisline's mem alloc??? 
	SDL_DestroySurface(surface);

	SDL_FRect dst;

	int w, h;
	TTF_GetStringSize(fuente, Texto, strlen(Texto), &w, &h);
	float invScale = (currentRenderScale > 0.0f) ? (1.0f / currentRenderScale) : 1.0f;
	dst.x = X; dst.y = Y; dst.w = w * invScale; dst.h = h * invScale;
	SDL_RenderTexture(surf, texture, NULL, &dst);
	SDL_DestroyTexture(texture); // ? Is this safe to do here ?

}

void DrawSurfText(SDL_Renderer* surf, char* Texto, int X, int Y, TTF_Font* fuente)
{
	SDL_Color White = { 200, 200, 200 };
	DrawSurfText(surf, Texto, X, Y, fuente, White);
}
void DrawGauge(SDL_Renderer* surf, double Pos, double Sep, int PosX, TTF_Font* fuente, bool selected)
{
	char StrTemp[256];

	int w = PanWidth;
	int h = PanHeight;
	if (selected) {
		SDL_SetRenderDrawColor(surf, 255, 200, 0, SDL_ALPHA_OPAQUE);
	} else {
		SDL_SetRenderDrawColor(surf, 0, 255, 255, SDL_ALPHA_OPAQUE);
	}

	int MaxCount = (int)((h / 2) / Sep + 1);
	double Rem = Pos - (int)Pos;
	for (int i = (int)0; i < MaxCount * 1.2; i++)
	{
		int posy = (int)((h / 2) - (i * Sep) + (Rem * Sep));

		SDL_RenderLine(surf, PosX - 3, posy, PosX + 3, posy);

		sprintf(StrTemp, "%d", (int)Pos + i);
		DrawSurfText(surf, StrTemp, PosX + 5, posy - 5, fuente);
	}
	SDL_RenderLine(surf, PosX, 0, PosX, h);
	sprintf(StrTemp, "%f", Pos);
	DrawSurfText(surf, StrTemp, PosX, 5 * h / 6, fuente);

	SDL_SetRenderDrawColor(surf, 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderLine(surf, 0, h / 2, w, h / 2);

}


void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	/* SDL will clean up the window/renderer for us. */
	for (int i = 0; i < 10; i++)
	{
		TTF_CloseFont(fuentes[i]);
	}
	TTF_Quit();
	SDL_Quit();
}



void DrawFactorGauges(SDL_Renderer *surf, int baseIndex, int selectedIndex, int windowIndex)
{
    char StrTemp[256];

	int w = PanWidth;
	int h = PanHeight;

	int PosxLeft = (w / 2) - (w / 5);
	int PosxCenter = (w / 2);
	int PosxRight = (w / 2) + (w / 5);
	int Posx[3] = { PosxLeft, PosxCenter, PosxRight };
	SDL_Color Red = { 220, 40, 40 };
	int labelY = h - (h / 8);
	const char windowLabels[3] = {
		GetLabelForWindowColumn(windowIndex, 0),
		GetLabelForWindowColumn(windowIndex, 1),
		GetLabelForWindowColumn(windowIndex, 2)
	};

	for (int i = 0; i < 3; i++)
	{
		int idx = baseIndex + i;
		int x = Posx[i];
		DrawGauge(surf, Times[idx], 50 * Factors[idx], x, fuentes[5], selectedIndex == i);
		char labelText[2] = { windowLabels[i], '\0' };
		sprintf(StrTemp, "%s", labelText);
		DrawSurfText(surf, StrTemp, x, labelY, fuentes[7], Red);
		sprintf(StrTemp, "%f", Factors[idx]);
		DrawSurfText(surf, StrTemp, x, 4 * h / 6, fuentes[5]);
		sprintf(StrTemp, "v=%0.3f", Velocidades[idx]);
		DrawSurfText(surf, StrTemp, x, 4 * h / 6 + 24, fuentes[4]);
	}

	int idxB = GetIndexForLabelInWindow(windowIndex, 'B');
	int idxA = GetIndexForLabelInWindow(windowIndex, 'A');
	int idxC = GetIndexForLabelInWindow(windowIndex, 'C');
	if (idxB >= 0 && idxA >= 0 && idxC >= 0) {
		int topLineY = h - (h / 16);
		int bottomLineY = h - (h / 56);
		int margin = w / 75;
		int textY = topLineY - (h / 22);

		int xB = Posx[idxB - baseIndex];
		int xA = Posx[idxA - baseIndex];
		int xC = Posx[idxC - baseIndex];

		double dtBA = Times[idxB] - Times[idxA];
		double dtAC = Times[idxA] - Times[idxC];
		double dtBC = Times[idxB] - Times[idxC];

		SDL_Color Green = { 20, 230, 20 };
		SDL_SetRenderDrawColor(surf, Green.r, Green.g, Green.b, SDL_ALPHA_OPAQUE);
		SDL_RenderLine(surf, xB + margin, topLineY, xA - margin, topLineY);
		SDL_RenderLine(surf, xA + margin, topLineY, xC - margin, topLineY);
		SDL_RenderLine(surf, xB + margin, bottomLineY, xC - margin, bottomLineY);

		sprintf(StrTemp, "%0.3f", dtBA);
		DrawSurfText(surf, StrTemp, ((xB + xA) / 2) - (w / 42), textY, fuentes[4], Green);
		sprintf(StrTemp, "%0.3f", dtAC);
		DrawSurfText(surf, StrTemp, ((xA + xC) / 2) - (w / 42), textY, fuentes[4], Green);
		sprintf(StrTemp, "%0.3f", dtBC);
		DrawSurfText(surf, StrTemp, ((xB + xC) / 2) - (w / 42), bottomLineY - (h / 22), fuentes[4], Green);
	}
}




void DrawScene()
{
	for (int i = 0; i < kWindowCount; i++) {
		float newScale = GetRenderScale(renderers[i]);
		if (newScale != renderScales[i]) {
			renderScales[i] = newScale;
			currentRenderScale = newScale;
			UpdateFontsForScale(currentRenderScale);
		}
		currentRenderScale = renderScales[i];
		SDL_SetRenderDrawColor(renderers[i], 0, 0, 0, SDL_ALPHA_OPAQUE);
		SDL_RenderClear(renderers[i]);
		DrawFactorGauges(renderers[i], i * kColumnsPerWindow, SelectedGauge[i], i);
		SDL_RenderPresent(renderers[i]);
	}

	for (auto& ev : eventos) {
		if (ev.type == "pausa") {
			if (!ev.triggered && ColumnReached(ev.column, ev.time)) {
				Pause = true;
				ev.triggered = true;
			}
			continue;
		}
		if (ev.type == "cambio") {
			for (int w = 0; w < kWindowCount; w++) {
				int bit = 1 << w;
				if ((ev.triggeredMask & bit) != 0) {
					continue;
				}
				if (ColumnReachedInWindow(w, ev.column, ev.time)) {
					double signedAmount = (w == 0) ? ev.amount : -ev.amount;
					ApplyDeltaToLabelInWindow(w, ev.column, signedAmount);
					ev.triggeredMask |= bit;
				}
			}
			if (ev.triggeredMask == ((1 << kWindowCount) - 1)) {
				ev.triggered = true;
			}
		}
	}

	if ((Pause==false)||(NextStep==true))
	{
		for (int i = 0; i < kTotalColumns; i++)
		{
			Times[i]+=(1.0/100.0) * (1/Factors[i]);
		}
		NextStep=false;
	}
}


static SDL_AppResult handle_key_event_(SDL_Scancode key_code, int windowIndex)
{
	switch (key_code) 
	{
		/* Quit. */
		case SDL_SCANCODE_ESCAPE:
		case SDL_SCANCODE_Q:
			return SDL_APP_SUCCESS;
		case SDL_SCANCODE_P:
			if (Pause == false)
				Pause = true;
			else
				Pause = false;
			break;
		case SDL_SCANCODE_1:
			break;
		case SDL_SCANCODE_2:
			break;
		case SDL_SCANCODE_3:
			break;
		case SDL_SCANCODE_EQUALS:
		case SDL_SCANCODE_KP_PLUS:
			AdjustSelectedVelocity(windowIndex, 0.05);
			break;
		case SDL_SCANCODE_MINUS:
		case SDL_SCANCODE_KP_MINUS:
			AdjustSelectedVelocity(windowIndex, -0.05);
			break;
		case SDL_SCANCODE_R:
			ResetState();
			break;
	}
	return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	switch (event->type) {
	case SDL_EVENT_QUIT:
		return SDL_APP_SUCCESS;
	case SDL_EVENT_KEY_DOWN:
		{
			int windowIndex = 0;
			for (int i = 0; i < kWindowCount; i++) {
				if (event->key.windowID == windowIds[i]) {
					windowIndex = i;
					break;
				}
			}
			return handle_key_event_(event->key.scancode, windowIndex);
		}
	}
	return SDL_APP_CONTINUE;  /* carry on with the program! */
}


SDL_AppResult SDL_AppIterate(void* appstate)
{
	DrawScene();
	return SDL_APP_CONTINUE;  /* carry on with the program! */
}
void pruebas()
{
	double r = 10;
	double PI = 3.14159265;
	double D = (2*PI*r)/100.0;
	double rps = 450/D;
	double rpm = rps*60;

	int a = 0;
}


SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
	//pruebas();
	unsigned short echoServPort = 1162;     // First arg:  local port

	
	InitSdl();
	LoadEventos();
	ResetState();



	return SDL_APP_CONTINUE;  /* carry on with the program! */

}
static void LoadEventos()
{
	const char* basePath = SDL_GetBasePath();
	if (basePath != NULL) {
		std::string configPath = std::string(basePath) + "config.yaml";
		if (LoadEventosFromYaml(configPath.c_str())) {
			return;
		}
	}
	if (LoadEventosFromYaml("config.yaml")) {
		return;
	}
	LoadEventosFromYaml("../config.yaml");
}
