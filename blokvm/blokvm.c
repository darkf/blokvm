/* blokvm -- a darkf implementation of blockeduser's terrible vm
   copyright (c) 2012 darkf */

#include <stdio.h>
#include <stdlib.h>
#include <SDL/SDL.h>

/* 3 * 255^2 */
#define BITMAP_SIZE 195075
#define JMP fseek(fp, PC, SEEK_SET);

typedef unsigned char uchar;

char bbitmap1[BITMAP_SIZE], bbitmap2[BITMAP_SIZE];
int mem[256];
int PC = 0;
SDL_Surface *screen, *bitmap1, *bitmap2;
Uint32 white;

void putpixel(SDL_Surface* dest, int x, int y, int r, int g, int b)
{
	SDL_PixelFormat *pixelFormat = dest -> format;
	Uint32 rectColor = SDL_MapRGB(pixelFormat, r, g, b);
	struct SDL_Rect rectRegion = { x, y, 1, 1 };
	SDL_FillRect(dest, &rectRegion, rectColor);
}

char fetch_rgb(char* bbitmap, int x, int y, char chan)
{
	return bbitmap[3 * (255 * y + x) + chan];
}

int main(int argc, char *argv[])
{
	FILE *fp;
	long flen;
	uchar op;

	if(argc != 2) {
		printf("usage: %s FILE\n", argv[0]);
		return 1;
	}

	/* open file */
	fp = fopen(argv[1], "rb");
	if(!fp) {
		printf("failed to open file %s\n", argv[1]);
		return 1;
	}

	/* get filesize */
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* read bitmaps */
	fread(bbitmap1, BITMAP_SIZE, 1, fp);
	fread(bbitmap2, BITMAP_SIZE, 1, fp);

	SDL_Init(SDL_INIT_VIDEO);
	screen = SDL_SetVideoMode(255, 255, 32, SDL_SWSURFACE);
	if(!screen) {
		printf("couldn't initialize screen\n");
		return 5;
	}

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	bitmap1 = SDL_CreateRGBSurfaceFrom(bbitmap1, 255, 255, 24, 255*3, 0x0000FF, 0x00FF00, 0xFF0000, 0); /* LE machines use BGR instead of RGB */
#else
	bitmap1 = SDL_CreateRGBSurfaceFrom(bbitmap1, 255, 255, 24, 255*3, 0, 0, 0, 0);
#endif
	bitmap2 = SDL_CreateRGBSurfaceFrom(bbitmap2, 255, 255, 24, 255*3, 0, 0, 0, 0);

	if(!bitmap1 || !bitmap2) {
		printf("couldn't load bitmaps\n");
		return 4;
	}

	bitmap1 = SDL_ConvertSurface(bitmap1, screen->format, 0);
	bitmap2 = SDL_ConvertSurface(bitmap2, screen->format, 0);

	white = SDL_MapRGB(screen->format, 255, 255, 255);

	SDL_FillRect(screen, &screen->clip_rect, white); /* clear screen to white */

	while(!feof(fp)) {
		op = fgetc(fp); /* read op */
		SDL_PumpEvents();/* update SDL key/mouse state data */

		if(mem[0] == 240) mem[0] = 123;
		else {
			Uint8 *keymap = SDL_GetKeyState(NULL);
			mem[0] = 0;
			if(keymap[SDLK_UP]) mem[0]     |= 128;
			if(keymap[SDLK_DOWN]) mem[0]   |= 64;
			if(keymap[SDLK_LEFT]) mem[0]   |= 32;
			if(keymap[SDLK_RIGHT]) mem[0]  |= 16;
			if(keymap[SDLK_z]) mem[0]      |= 8;
			if(keymap[SDLK_x]) mem[0]      |= 4;
			if(keymap[SDLK_RETURN]) mem[0] |= 2;
			if(keymap[SDLK_SPACE]) mem[0]  |= 1;
		}

		switch(op)
		{
			case 10: /* Do */
				{
					uchar args[4];
					int data;
					fread(args, sizeof(uchar), sizeof(args), fp); /* read args */
					switch(args[2] /* type */)
					{
						case 1: data = args[3]; break; /* immediate value */
						case 2: data = mem[args[3]]; break; /* memory dereference */
					}

					switch(args[1] /* operation */)
					{
						case 10: mem[args[0]]  = data; break; /* assignment */
						case 20: mem[args[0]] += data; break; /* + */
						case 30: mem[args[0]] -= data; break; /* - */
						case 40: mem[args[0]] *= data; break; /* * */
						case 50: mem[args[0]] /= data; break; /* / */
					}

					break;
				}
			case 12: /* PtrTo */
				{
					uchar addr = fgetc(fp);
					mem[addr] = ftell(fp);
					break;
				}
			case 13: /* PtrFrom */
				{
					uchar addr = fgetc(fp);
					PC = mem[addr];
					JMP
					break;
				}
			case 31: /* bPtrTo */
				{
					uchar args[3];
					fread(args, sizeof(uchar), sizeof(args), fp); /* read args */
					PC = ftell(fp);
					if(mem[args[0]] < 1)
					{
						if     (args[1] == 10) PC += mem[args[2]]; /* jump forward */
						else if(args[1] == 20) PC -= mem[args[2]]; /* jump backward */
						JMP
					}
					break;
				}
			case 35: /* zPtrTo */
				{
					uchar args[3];
					fread(args, sizeof(uchar), sizeof(args), fp); /* read args */
					PC = ftell(fp);
					if(mem[args[0]] > args[1])
					{
						PC += mem[args[2]];
						JMP
					}
					break;
				}
			case 14: /* BoolDie */
				{
					uchar addr = fgetc(fp);
					printf("booldie: %d\n", mem[addr]);
					if(mem[addr] < 0)
						exit(1);
					break;
				}

			case 20: /* Draw */
				{
					uchar args[8];

					SDL_Rect src, dst;
					fread(args, sizeof(uchar), sizeof(args), fp); /* read args */
					src.x = args[4]; src.y = args[5]; src.w = args[6]; src.h = args[7];
					dst.x = args[0]; dst.y = args[1]; dst.w = args[2]; dst.h = args[3];

					/* scaling/clipping algorithm */
					uchar x, y;
					for(y = dst.y; y < dst.y + dst.h; ++y)
						for(x = dst.x; x < dst.x + dst.w; ++x)
						{
							uchar src_x = src.x + src.w * (x - dst.x) / dst.w;
							uchar src_y = src.y + src.h * (y - dst.y) / dst.h;
							putpixel(screen, x, y,
								fetch_rgb(bbitmap1, src_x, src_y, 0),
								fetch_rgb(bbitmap1, src_x, src_y, 1),
								fetch_rgb(bbitmap1, src_x, src_y, 2));
						}

					break;
				}
			case 21: /* vDraw */
				{
					uchar args[8];

					SDL_Rect src, dst;
					fread(args, sizeof(uchar), sizeof(args), fp); /* read args */
					src.x = args[4]; src.y = args[5]; src.w = args[6]; src.h = args[7];
					dst.x = mem[args[0]]; dst.y = mem[args[1]]; dst.w = args[2]; dst.h = args[3];

					/* scaling/clipping algorithm */
					uchar x, y;
					for(y = dst.y; y < dst.y + dst.h; ++y)
						for(x = dst.x; x < dst.x + dst.w; ++x)
						{
							uchar src_x = src.x + src.w * (x - dst.x) / dst.w;
							uchar src_y = src.y + src.h * (y - dst.y) / dst.h;
							putpixel(screen, x, y,
								fetch_rgb(bbitmap1, src_x, src_y, 0),
								fetch_rgb(bbitmap1, src_x, src_y, 1),
								fetch_rgb(bbitmap1, src_x, src_y, 2));
						}

					break;
				}
			case 30: /* OutputDraw */
				SDL_Flip(screen); /* flip backbuffer */
				break;

			case 25:
			case 26: /* mx and my */
				{
					uchar addr = fgetc(fp);
					int x, y; SDL_GetMouseState(&x, &y);
					mem[addr] = (op == 25) ? x : y;
					break;
				}
			case 50:
			case 51: /* cmx and cmy */
				{
					uchar addr = fgetc(fp);
					int x, y;
					if(SDL_GetMouseState(&x, &y) & SDL_BUTTON(1))
						mem[addr] = (op == 50) ? x : y;
					break;
				}

			case 15: /* Wait */
				{
					uchar args[2];
					fread(args, sizeof(uchar), sizeof(args), fp); /* read args */
					SDL_Delay((1000 * args[0]) / args[1]); /* wait n ms */
					break;
				}
			case 11: /* Echo */
				{
					uchar addr = fgetc(fp);
					printf("echo: %d\n", mem[addr]);
					break;
				}
			case 255: /* End */
				printf("end\n");
				break;
			default: /* bad opcode */
				printf("bad opcode: %d | PC=%d\n", op, ftell(fp));
				break;
		}
	}

	fclose(fp);
	return 0;
}

