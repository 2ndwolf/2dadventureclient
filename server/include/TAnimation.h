#ifndef GS2EMU_TANIMATION_H
#define GS2EMU_TANIMATION_H

#include "TImage.h"
#include "TPlayer.h"
#include "TClient.h"

class TPlayer;

class TAnimationSprite
{
	public:
		TAnimationSprite(int pSprite, std::string pImage, int pX, int pY, int pW, int pH, std::string desc);
		~TAnimationSprite();

		inline void render(CGaniObjectStub * player, TClient * server, int pX, int pY);

	private:
		std::string img, description;
		int sprite, x, y, w, h;
};

class TAnimationAni
{
	public:
		TAnimationAni(TAnimationSprite *pImg, float pX, float pY);

		float x, y;

		TAnimationSprite *img;

		inline void render(CGaniObjectStub * player, TClient * server, int pX, int pY);
};

class TAnimation
{
	public:
		explicit TAnimation(CString pName, TClient * theServer);
		~TAnimation();

		bool loaded;
		CString name, real;

		bool load();
		void render(CGaniObjectStub* player, TClient * server, int pX, int pY, int pDir, int *pStep, float time);

		static TAnimation *find(const char *pName, TClient * theServer);
		TImage *findImage(char *pName, TClient * theServer);
	private:
		bool isLoop = false, isContinuous = false, isSingleDir = false;
		CString setBackTo;
		std::unordered_map<std::string, TImage *> imageList;
		std::unordered_map<int, TAnimationSprite *> animationSpriteList;
		std::map<int, std::map<int, TAnimationAni *>> animationAniList;
		TClient *server;
		float currentWait{};
		float wait = 0.05f;
		int max{};
		SDL_Thread *thread{};
};



#include <SDL.h>
#include <SDL_image.h>
#include <CString.h>
#include "TImage.h"
#include "TClient.h"

#endif //GS2EMU_TANIMATION_H
