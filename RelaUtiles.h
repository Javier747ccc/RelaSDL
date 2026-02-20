#ifndef UTILES_H_INCLUDED
#define UTILES_H_INCLUDED
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef WIN32
#include <windows.h>
#endif
#include "RelaUtiles.h"
#include "SDL.h"
#include "SDL_TTF.h"

#define xorSwap(x,y) {(x)=(x)^(y); (y)=(x)^(y); (x)=(x)^(y);}



void DrawSurfText(SDL_Renderer *isurf,char *Texto,int X,int Y,TTF_Font* fuente);
void setPixel(SDL_Renderer *surf, int x, int y, unsigned int Color);
void lineBresenham(SDL_Renderer *surf, int p1x, int p1y, int p2x, int p2y,unsigned int Color);
void DrawGauge(SDL_Renderer *surf,double Pos,double Sep,int PosX,TTF_Font* fuente);
void circle(SDL_Renderer *surf,int cx, int cy, int radius,unsigned int Color);


#endif // UTILES_H_INCLUDED
