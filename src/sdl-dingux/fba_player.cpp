/*
 * FinalBurn Alpha for Dingux/OpenDingux
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <SDL/SDL.h>

#include "fba_player.h"
#include "font.h"
#include "snd.h"

#include "burner.h"
#include "sdlgui.h"
#include "sdlvideo.h"
#include "sdlinput.h"
#include "sdlromload.h"

static int frame_count = 0;
bool GameLooping;
int fps=0;

void shutdown()
{
	printf("---- Shutdown Finalburn Alpha plus ----\n\n");
	DrvExit();

	BurnLibExit();

	gui_Exit();

	RomLoadExit();
	VideoExit();
	InpExit();

	SystemExit(config_options.option_frontend);
}

long long get_ticks_us()
{
#ifndef WIN32
	long long ticks;
	struct timeval current_time;
	gettimeofday(&current_time, NULL);

	return (long long)current_time.tv_sec * 1000000 + (long long)current_time.tv_usec;
#else
	return SDL_GetTicks() * 1000;
#endif
}

void sleep_us(int value)
{
#ifndef WIN32
	usleep(value);
#else
	SDL_Delay(value / 1000);
#endif
}

struct timeval start;

unsigned int GetTicks (void)
{
	unsigned int ticks;
	struct timeval now;
	gettimeofday(&now, NULL);
	ticks=(now.tv_sec-start.tv_sec)*1000000 + now.tv_usec-start.tv_usec;
	return ticks;
}

void run_fba_emulator(int drvnum)
{
	atexit(shutdown);

	gui_Init();

	ConfigAppLoad(); // move to main later

	SystemInit();
	VideoInit();

	printf("Attempt to initialise '%s'\n", BurnDrvGetTextA(DRV_FULLNAME));

	RomLoadInit();

	InpInit();
	InpDIP();

	if (DrvInit(drvnum, false) != 0)
	{
		printf ("Driver initialisation failed! Likely causes are:\n- Corrupt/Missing ROM(s)\n- I/O Error\n- Memory error\n\n");
		goto finish;
	}

	if (config_options.option_sense<100)
	{
		nAnalogSpeed=0x100/100*config_options.option_sense;
	}

	RunReset();

	frame_count = 0;
	GameLooping = true;

	bShowFPS = config_options.option_showfps;

	if (BurnDrvGetFlags() & BDF_ORIENTATION_FLIPPED) printf("flipped!\n");

	printf ("Let's go!\n");

	VideoClear();

#ifdef FBA_DEBUG
	gui_RunDebug();
#endif

#ifdef WIN32
	{
		int now, start, lim=0, wait=0, frame_count=0, skipped_frames=0, draw_this_frame=true, fps=0;
		int frame_limit = nBurnFPS/100, frametime = 100000000/nBurnFPS; // 16667 usec
		int skip_limit = 0;

		start = get_ticks_us();
		while (GameLooping)
		{
			RunOneFrame(draw_this_frame, fps);
			SndPlay();

			now = get_ticks_us();
			draw_this_frame = true;
			frame_count++;
			if(now - start >= 1000000) {
				start = now;
				fps = frame_limit - skipped_frames;
				skipped_frames = 0;
				frame_count = 0;
			}

			lim = (frame_count) * frametime;
			if(config_options.option_frameskip == -1) { // auto frameskip
				if(now-start > lim) if(++skipped_frames < frame_limit/2) draw_this_frame = false;
			} else { // manual frameskip 0..10
				if(config_options.option_frameskip > 0) {
					if(skipped_frames < config_options.option_frameskip) { 
						if(--skip_limit < 0) { 
							draw_this_frame = false;
							skip_limit = frame_limit / config_options.option_frameskip;
							skipped_frames++; 
						}
					}
				}
			}

			wait = lim - (now - start);
			if(config_options.option_sound_enable != 2 && wait > 0) sleep_us(wait);

			//printf("%i:, diff: %i, lim: %i, wait: %i\n", frame_count, now-start, lim, wait);
		}
	}
#else
	{
		int now, done=0, timer = 0, ticks=0, tick=0, i=0, fps = 0;
		unsigned int frame_limit = nBurnFPS/100, frametime = 100000000/nBurnFPS;

		gettimeofday(&start, NULL);
		while (GameLooping)
		{
			timer = GetTicks()/frametime;;
			if(timer-tick>frame_limit && bShowFPS)
			{
				fps = nFramesRendered;
				nFramesRendered = 0;
				tick = timer;
			}
			now = timer;
			ticks=now-done;
			if(ticks<1) continue;
			if(ticks>10) ticks=10;
			for (i=0; i<ticks-1; i++)
			{
				RunOneFrame(false,fps);
				SndPlay();
			}
			if(ticks>=1)
			{
				RunOneFrame(true,fps);
				SndPlay();
			}

			done = now;
		}
	}
#endif

	printf ("Finished emulating\n");

finish:
	printf("---- Shutdown Finalburn Alpha plus ----\n\n");
	ConfigAppSave();
}
