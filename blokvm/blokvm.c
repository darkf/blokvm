/* blokvm -- a darkf implementation of blockeduser's terrible vm
   copyright (c) 2012 darkf */

#include <stdio.h>
#include <stdlib.h>

/* 3 * 255^2 */
#define BITMAP_SIZE 195075
#define JMP fseek(fp, PC, SEEK_SET);

typedef unsigned char uchar;

char filename[] = "fib.BPF";
char bitmap1[BITMAP_SIZE], bitmap2[BITMAP_SIZE];
int mem[256];
int PC = 0;

int main()
{
	FILE *fp;
	long flen;
	uchar op;

	/* open file */
	fp = fopen(filename, "rb");
	if(!fp) {
		printf("failed to open file %s\n", filename);
		return 1;
	}

	/* get filesize */
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	/* read bitmaps */
	fread(bitmap1, BITMAP_SIZE, 1, fp);
	fread(bitmap2, BITMAP_SIZE, 1, fp);

	while(!feof(fp)) {
		fread(&op, sizeof(op), 1, fp); /* read op */
		switch(op)
		{
			case 10: /* Do */
				{
					/*uchar addr, oper, type, data;*/
					uchar args[4];
					int data;
					fread(args, 1, 4, fp);
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
					uchar addr;
					fread(&addr, sizeof(uchar), 1, fp);
					mem[addr] = ftell(fp);
					break;
				}
			case 13: /* PtrFrom */
				{
					uchar addr;
					fread(&addr, sizeof(uchar), 1, fp);
					//printf("ptrfrom: addr=%d mem=%d\n", addr, mem[addr]);
					PC = mem[addr];
					JMP
						break;
				}
			case 31: /* bPtrTo */
				{
					uchar args[3];
					fread(args, 3, 1, fp);
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
					fread(args, 3, 1, fp);
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
					uchar addr;
					fread(&addr, 1, 1, fp);
					printf("booldie: %d\n", mem[addr]);
					if(mem[addr] < 0)
						exit(1);
					break;
				}

			case 20: /* Draw */
				break;
			case 21: /* vDraw */
				break;
			case 30: /* OutputDraw */
				break;

			case 25: /* mx */
				break;
			case 26: /* my */
				break;
			case 50: /* cmx */
				break;
			case 51: /* cmy */
				break;

			case 15: /* Wait */
				break;
			case 11: /* Echo */
				{
					uchar addr;
					fread(&addr, 1, 1, fp);
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

