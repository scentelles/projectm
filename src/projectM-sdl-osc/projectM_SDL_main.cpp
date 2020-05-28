/**
* projectM -- Milkdrop-esque visualisation SDK
* Copyright (C)2003-2019 projectM Team
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
* See 'LICENSE.txt' included within this release
*
* projectM-sdl
* This is an implementation of projectM using libSDL2
* 
* main.cpp
* Authors: Created by Mischa Spiegelmock on 6/3/15.
*
* 
*	RobertPancoast77@gmail.com :
* experimental Stereoscopic SBS driver functionality
* WASAPI looback implementation
* 
*
*/
#include <thread>         // std::thread

#include "pmSDL.hpp"

#define OSCPKT_OSTREAM_OUTPUT
#include "oscpkt/oscpkt.hh"
#include "oscpkt/udp.hh"
using namespace oscpkt;
const int PORT_NUM = 7700;


    projectMSDL *app;
    
using std::endl;
using std::cout;
using std::cerr;    

bool changePresetRequest = false;
unsigned int presetRequested = 0;

void runOSCServer() {
  UdpSocket sock; 
  sock.bindTo(PORT_NUM);
  if (!sock.isOk()) {
    cerr << "Error opening port " << PORT_NUM << ": " << sock.errorMessage() << "\n";
  } 
  else 
  {
    cout << "Server started, will listen to packets on port " << PORT_NUM << std::endl;
    PacketReader pr;
    PacketWriter pw;
    while (sock.isOk()) {      
      if (sock.receiveNextPacket(30 /* timeout, in ms */)) {
        pr.init(sock.packetData(), sock.packetSize());
        oscpkt::Message *msg;
        while (pr.isOk() && (msg = pr.popMessage()) != 0) {
          int iarg;
	  cout << "Received OSC. STarting parsing" << msg->address <<endl;
          if (msg->match("/FOG/multipush1/1/1")) {
               cout << "Server: received touchOSC event request " << msg->address << " from " << sock.packetOrigin() << "\n";
		changePresetRequest = true;
		presetRequested +=10;

	   }
//          if (msg->match("/FOG/multipush1/2/1").popInt32(iarg).isOkNoMoreArgs()) {
          if (msg->match("/FOG/multipush1/2/1")) {
               cout << "Server: received touchOSC event request 2 " << iarg << " from " << sock.packetOrigin() << "\n";
		changePresetRequest = true;
		presetRequested -=10;

	   }
          }
        }
      }
    }
  
}





// return path to config file to use
std::string getConfigFilePath(std::string datadir_path) {
    struct stat sb;
    const char *path = datadir_path.c_str();
    
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Looking for configuration file in data dir: %s.\n", path);
    
    // check if datadir exists.
    // it should exist if this application was installed. if not, assume we're developing it and use currect directory
    if (stat(path, &sb) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "Could not open datadir path %s\n", path);
    }
    
    std::string configFilePath = path;
    configFilePath += "/config.inp";
    
    // check if config file exists
    if (stat(configFilePath.c_str(), &sb) != 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_ERROR, "No config file %s found. Using development settings.\n", configFilePath.c_str());
        return "";
    }
    
    return configFilePath;
}






int main(int argc, char *argv[]) {

  std::thread ocsThread (runOSCServer); 


#ifndef WIN32
srand((int)(time(NULL)));
#endif


#if UNLOCK_FPS
    setenv("vblank_mode", "0", 1);
#endif
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    if (! SDL_VERSION_ATLEAST(2, 0, 5)) {
        SDL_Log("SDL version 2.0.5 or greater is required. You have %i.%i.%i", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
        return 1;
    }

    // default window size to usable bounds (e.g. minus menubar and dock)
    SDL_Rect initialWindowBounds;

    // new and better
    SDL_GetDisplayUsableBounds(0, &initialWindowBounds);

  //  int width = initialWindowBounds.w;
  //  int height = initialWindowBounds.h;
   // int width = 384;
   // int height = 96;
    //int width = 128;
    //int height = 64;
    int width = 128;
    int height = 128;


    // use GLES 2.0 (this may need adjusting)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);


    SDL_Window *win = SDL_CreateWindow("projectM", 0, 0, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    
    SDL_GL_GetDrawableSize(win,&width,&height);
    
    SDL_GLContext glCtx = SDL_GL_CreateContext(win);


    SDL_Log("GL_VERSION: %s", glGetString(GL_VERSION));
    SDL_Log("GL_SHADING_LANGUAGE_VERSION: %s", glGetString(GL_SHADING_LANGUAGE_VERSION));
    SDL_Log("GL_VENDOR: %s", glGetString(GL_VENDOR));

    SDL_SetWindowTitle(win, "projectM Visualizer");
    
    SDL_GL_MakeCurrent(win, glCtx);  // associate GL context with main window
    int avsync = SDL_GL_SetSwapInterval(-1); // try to enable adaptive vsync
    if (avsync == -1) { // adaptive vsync not supported
        SDL_GL_SetSwapInterval(1); // enable updates synchronized with vertical retrace
    }

    

    
    std::string base_path = DATADIR_PATH;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Using data directory: %s\n", base_path.c_str());

    // load configuration file
    std::string configFilePath = getConfigFilePath(base_path);

    if (! configFilePath.empty()) {
        // found config file, use it
        app = new projectMSDL(configFilePath, 0);
        SDL_Log("Using config from %s", configFilePath.c_str());
    } else {
        base_path = SDL_GetBasePath();
        SDL_Log("Config file not found, using built-in settings. Data directory=%s\n", base_path.c_str());

        float heightWidthRatio = (float)height / (float)width;
        projectM::Settings settings;
        settings.windowWidth = width;
        settings.windowHeight = height;
        settings.meshX = 128;
        settings.meshY = settings.meshX * heightWidthRatio;
        settings.fps   = 60;
        settings.smoothPresetDuration = 3; // seconds
        settings.presetDuration = 0; // seconds
        settings.beatSensitivity = 0.8;
        settings.aspectCorrection = 1;
        settings.shuffleEnabled = 1;
        settings.softCutRatingsEnabled = 1; // ???
        // get path to our app, use CWD or resource dir for presets/fonts/etc
        settings.presetURL = base_path + "presets";
//        settings.presetURL = base_path + "presets/presets_shader_test";
        settings.menuFontURL = base_path + "fonts/Vera.ttf";
        settings.titleFontURL = base_path + "fonts/Vera.ttf";
        // init with settings
        app = new projectMSDL(settings, 0);
    }
    app->init(win, &glCtx );
//    app->init(win, &glCtx, true);


#if !FAKE_AUDIO && !WASAPI_LOOPBACK
    // get an audio input device
    if (app->openAudioInput())
	    app->beginAudioCapture();
#endif

#if TEST_ALL_PRESETS
    uint buildErrors = 0;
    for(unsigned int i = 0; i < app->getPlaylistSize(); i++) {
        std::cout << i << "\t" << app->getPresetName(i) << std::endl;
        app->selectPreset(i);
        if (app->getErrorLoadingCurrentPreset()) {
            buildErrors++;
        }
    }

    if (app->getPlaylistSize()) {
        fprintf(stdout, "Preset loading errors: %d/%d [%d%%]\n", buildErrors, app->getPlaylistSize(), (buildErrors*100) / app->getPlaylistSize());
    }

    delete app;

    return PROJECTM_SUCCESS;
#endif

#if UNLOCK_FPS
    int32_t frame_count = 0;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    int64_t start = ( ((int64_t)now.tv_sec * 1000L) + (now.tv_nsec / 1000000L) );
#endif

    // standard main loop
    int fps = app->settings().fps;
    printf("fps: %d\n", fps);
    if (fps <= 0)
        fps = 60;
    const Uint32 frame_delay = 1000/fps;
    Uint32 last_time = SDL_GetTicks();
    
    app->resize(width,height);
    

int count = 0;    
    while (! app->done) {

count++;

  if(changePresetRequest == true)
  {
  
  		
                changePresetRequest = false;
		unsigned int index;
		app->selectedPresetIndex(index);
		std::cout << "Current Index : " << index << " / " << app->getPlaylistSize() << "\n";
		std::cout << "Change Index to  : " << presetRequested << "\n";
  		app->selectPreset(presetRequested, false);

  
  }

        app->renderFrame();

#if UNLOCK_FPS
        frame_count++;
        clock_gettime(CLOCK_MONOTONIC_RAW, &now);
        if (( ((int64_t)now.tv_sec * 1000L) + (now.tv_nsec / 1000000L) ) - start > 5000) {
            SDL_GL_DeleteContext(glCtx);
            delete(app);
            fprintf(stdout, "Frames[%d]\n", frame_count);
            exit(0);
        }
#else
        app->pollEvent();
        Uint32 elapsed = SDL_GetTicks() - last_time;
        if (elapsed < frame_delay)
            SDL_Delay(frame_delay - elapsed);
        last_time = SDL_GetTicks();
#endif
    }
    
    SDL_GL_DeleteContext(glCtx);
#if !FAKE_AUDIO && !WASAPI_LOOPBACK
    app->endAudioCapture();
#endif
    delete app;

    return PROJECTM_SUCCESS;
}


