// SDLDrawPluginXBR.cpp
//
/////////////////////////////////////////////////////////////////////////////

#include "SDLVideoPlugins.h"


SDLDrawPluginPixelScaler::~SDLDrawPluginPixelScaler()
{
}

bool SDLDrawPluginPixelScaler::init(const VideoInfo *vi, IPalette *pal)
{
//fprintf(stderr,"thread id %d\n",std::this_thread::get_id());
	resize=false;
	// sobrecargamos el metodo init
	// e inicializamos lo necesario para escalar con XBR
	VideoInfo *_vi=new VideoInfo;
	memcpy(_vi,&vi,sizeof(VideoInfo));
#ifdef __EMSCRIPTEN__
	scaleFactor=2; // de 320x240 a 640x480
#else
	scaleFactor=4; // de 320x240 a 1280x960
#endif

	_vi->width= vi->width*scaleFactor/2;
	_vi->height = vi->height*scaleFactor/2;

// prueba pedir siempre 1280x960 y escalar según configuración
//	_vi->width= vi->width*4;
//	_vi->height = vi->height*4;
	_isInitialized = SDLBasicDrawPlugin<UINT32>::init(_vi,pal);

	if ( _isInitialized )
	{
		inBuffer=(uint8_t *)malloc(vi->width * vi->height * 4);
		outBuffer = (uint8_t*)malloc(vi->width * scaleFactor * vi->height * scaleFactor * 4);

		xbrData = (xbr_data *)malloc(sizeof(xbr_data));
		xbr_init_data(xbrData);

		xbrParams.data = xbrData;
		xbrParams.input = inBuffer;
		xbrParams.output = outBuffer;
		xbrParams.inWidth = vi->width;
		xbrParams.inHeight = vi->height;
		xbrParams.inPitch = vi->width * 4 ; 

		if (screen->pitch!= vi->width * scaleFactor * 4) {
			fprintf(stderr,"screen->pitch!= vi->width * scaleFactor * 4\n");
			return false;
		}
		xbrParams.outPitch = screen->pitch; // vi->width * scaleFactor * 4;
fprintf(stderr,"xbrParams inWidth %d inHeight %d inPitch %d outPitch %d\n",xbrParams.inWidth,xbrParams.inHeight,xbrParams.inPitch,xbrParams.outPitch); 
	}

	return _isInitialized;
}

void SDLDrawPluginPixelScaler::render(bool throttle)
{
//fprintf(stderr,"thread id %d\n",std::this_thread::get_id());
if (resize) {
	resize=false;
//SDL_LockSurface(screen); 
/*
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		fprintf(stderr,
				"Couldn't initialize SDL: %s\n", SDL_GetError());
		return ; // false;
	}
*/

fprintf(stderr,"render resize scaleFactor %d\n",scaleFactor);
	screen = SDL_SetVideoMode(320 *scaleFactor, 200 *scaleFactor, _bpp, _flags);
	if ( screen == NULL ) {
		fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
				666,666,_bpp,SDL_GetError());
//		return false;
	} else
	printf("render resize set %dx%dx%d video mode(%s) format(%X-%X-%X-%X) pitch(%d); ERROR: %s\n",
		screen->w,
		screen->h,
		screen->format->BitsPerPixel,
		screen->flags & SDL_DOUBLEBUF?"DOUBLEBUFF":"No double buffer",
		screen->format->Rmask,screen->format->Gmask,screen->format->Bmask,screen->format->Amask,
		screen->pitch,
		SDL_GetError());

		xbrParams.inWidth = 320; // vi->width;
		xbrParams.inHeight = 200; // vi->height;
		xbrParams.inPitch = 320*4; // vi->width * 4 ; 
//		if (screen->pitch!= vi->width * scaleFactor * 4) {
		if (screen->pitch!= 320 * scaleFactor * 4) {
			fprintf(stderr,"render resize screen->pitch!= vi->width * scaleFactor * 4\n");
//			return false;
		}
 
		xbrParams.outPitch = screen->pitch; // vi->width * scaleFactor * 4;
fprintf(stderr,"xbrParams inWidth %d inHeight %d inPitch %d outPitch %d\n",xbrParams.inWidth,xbrParams.inHeight,xbrParams.inPitch,xbrParams.outPitch); 
//SDL_Delay(5000);
} else {

#ifdef __EMSCRIPTEN__
	/* Lock the screen for direct access to the pixels */
	if ( SDL_MUSTLOCK(screen) ) {
		if ( SDL_LockSurface(screen) < 0 ) {
			fprintf(stderr, "Can't lock screen: %s\n", SDL_GetError());
			return;
		}
	}
#endif

	// TODO: Escalar solo los Rect actualizados
	(*xbr_filter_function)(&xbrParams);
//	memcpy(screen->pixels,outBuffer,xbrParams.inWidth*scaleFactor*xbrParams.inHeight*scaleFactor*4);
	memcpy(screen->pixels,outBuffer,xbrParams.outPitch*screen->h);

	SDL_Flip(screen);

#ifdef __EMSCRIPTEN__
	if ( SDL_MUSTLOCK(screen) ) {
		SDL_UnlockSurface(screen);
	}
#endif
}
}

void SDLDrawPluginPixelScaler::setPixel(int x, int y, int color)
{
	uint8_t *p = inBuffer+ y*xbrParams.inWidth*4 + x*4;
	*(UINT32 *)p = _palette[color];
}

bool SDLDrawPluginXBR::init(const VideoInfo *vi, IPalette *pal)
{
#ifndef __EMSCRIPTEN__
	xbr_filter_function=xbr_filter_xbr4x;
#else
	xbr_filter_function=xbr_filter_xbr2x;
#endif
	return SDLDrawPluginPixelScaler::init(vi,pal);
}

bool SDLDrawPluginHQX::init(const VideoInfo *vi, IPalette *pal)
{
#ifndef __EMSCRIPTEN__
	xbr_filter_function=xbr_filter_hq4x;
#else
	xbr_filter_function=xbr_filter_hq2x;
#endif
	return SDLDrawPluginPixelScaler::init(vi,pal);
}


void SDLDrawPluginXBR::setProperty(std::string prop, int data) {
	std::string Scale("Scale");
	if ( prop == Scale )
	{
		switch(data) {
			case 2: xbr_filter_function=xbr_filter_xbr2x; 
				scaleFactor=2; resize=true;
				break;
			case 3: xbr_filter_function=xbr_filter_xbr3x; 
				scaleFactor=3; resize=true;
				break;
//#ifndef __EMSCRIPTEN__
			case 4: xbr_filter_function=xbr_filter_xbr4x;
				scaleFactor=4; resize=true;
				break;
//#endif
			default: resize=false; fprintf(stderr,"No se soporta escalado con factor %d\n",data);
		}
/*
		switch(data) {
			case 2: xbr_filter_function=xbr_filter_xbr2x; 
				screen = SDL_SetVideoMode(320 *2, 200 *2, _bpp, _flags);
				break;
			case 3: xbr_filter_function=xbr_filter_xbr3x; 
				screen = SDL_SetVideoMode(320 *3, 200 *3, _bpp, _flags);
				break;
#ifndef __EMSCRIPTEN__
			case 4: xbr_filter_function=xbr_filter_xbr4x;
				screen = SDL_SetVideoMode(320 *4, 200 *4, _bpp, _flags);
				break;
#endif
			default: fprintf(stderr,"No se soporta escalado con factor %d\n",data);
		}
*/

/*
xbr_filter_function=xbr_filter_xbr4x; 
//		 SDL_LockSurface(screen);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
		fprintf(stderr,
				"Couldn't initialize SDL: %s\n", SDL_GetError());
		return ; // false;
	}


fprintf(stderr,"AAAAA\n");
screen = SDL_SetVideoMode(320 *4, 200 *4, _bpp, _flags);
fprintf(stderr,"BBBB\n");
	if ( screen == NULL ) {
		fprintf(stderr, "Couldn't set %dx%dx%d video mode: %s\n",
				666,666,_bpp,SDL_GetError());
//		return false;
	} else
	printf("set %dx%dx%d video mode(%s) format(%X-%X-%X-%X) pitch(%d); ERROR: %s\n",
		screen->w,
		screen->h,
		screen->format->BitsPerPixel,
		screen->flags & SDL_DOUBLEBUF?"DOUBLEBUFF":"No double buffer",
		screen->format->Rmask,screen->format->Gmask,screen->format->Bmask,screen->format->Amask,
		screen->pitch,
		SDL_GetError());
*/
	} else SDLBasicDrawPlugin<UINT32>::setProperty(prop,data);
};

void SDLDrawPluginHQX::setProperty(std::string prop, int data) {
	std::string Scale("Scale");
	if ( prop == Scale )
	{
		switch(data) {
			case 2: xbr_filter_function=xbr_filter_hq2x; break;
			case 3: xbr_filter_function=xbr_filter_hq3x; break;
#ifndef __EMSCRIPTEN__
			case 4: xbr_filter_function=xbr_filter_hq4x; break;
#endif
			default: fprintf(stderr,"No se soporta escalado con factor %d\n",data);
		}
	} else SDLBasicDrawPlugin<UINT32>::setProperty(prop,data);
};

