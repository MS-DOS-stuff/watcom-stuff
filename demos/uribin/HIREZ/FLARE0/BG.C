#include <stdlib.h>
#include <conio.h>
#include <math.h>
#include <memory.h>
#include "rtctimer.h"
#include "..\..\flexptc\flexptc.c"

#define X_SIZE 640
#define Y_SIZE 350
#define sat(a, l) ((a > l) ? l : a)
#define ee 1.0E-6

unsigned char  *screen = (unsigned char*)0xA0000;
ptc_palette    pal[256];

unsigned char  buffer[X_SIZE * Y_SIZE], buffer2[X_SIZE * Y_SIZE];
unsigned char  blendtab[65536];

// background sprite info
#define TILE_SIZE 16
#define X_GRID    (X_SIZE / TILE_SIZE)
#define Y_GRID    (Y_SIZE / TILE_SIZE)
 
unsigned char  bgspr [TILE_SIZE * TILE_SIZE];
unsigned long  bggrid[X_GRID * Y_GRID];

#define lut_size 65536
short sintab [lut_size], costab [lut_size];
float sintabf[lut_size], costabf[lut_size];

// flares info
#define stardist 192
#define DIST 450
#include "vector.h"

#define flaresnum 256
vertex   p[flaresnum], pt[flaresnum];
vertex2d p2d[flaresnum];

#define SPR_SIZE 24
unsigned char  flspr[SPR_SIZE * SPR_SIZE];

double pi = 3.141592653589793;

void vecdraw (vertex2d *v, vertex *f) {
    int i, y, x, j;
    int px, py, ofs;
    long scrptr = (long)&buffer;
    long sprptr = (long)&flspr;

    px = v->x - (SPR_SIZE >> 1); py = v->y - (SPR_SIZE >> 1);
        
    if ((py<(Y_SIZE - (SPR_SIZE)))&&(py>0)&&(px>0)&&(px<(X_SIZE - (SPR_SIZE)))&&(f->z < 0)) {
            
        scrptr = (long)&buffer + ((py << 9) + (py << 7) + px);
            
        _asm {
            mov    esi, sprptr
            mov    edi, scrptr
            
            mov    ecx, SPR_SIZE
                
            __outer:
            push   ecx
            mov    ecx, SPR_SIZE
                
            __inner:
            mov    al, [esi]
            inc    esi
            mov    ah, [edi]
            inc    edi
                
            add    al, ah
            sbb    dl, dl
            or     al, dl
            mov    [edi-1], al
                
            dec    ecx
            jnz    __inner
                
            pop    ecx
            add    edi, (X_SIZE - SPR_SIZE)
                
            dec    ecx
            jnz    __outer
        }
    }
}

void vecfill() {
    int rat, max, phi, teta, count2, i, lptr = 0;

    for (i = 0; i < flaresnum; i++) {
        p[i].x = stardist * (sintabf[(i << 10) & 0xFFFF]);
        p[i].y = stardist * (sintabf[(i << 9) & 0xFFFF]);
        p[i].z = stardist * (costabf[((i << 8) + (i << 9)) & 0xFFFF]);
    }
}

void initpal() {  
    int i;
    
    pal[0].r = pal[0].g = pal[0].b = 0;
    for (i = 1; i < 256; i++) {
        pal[i].r = sat(4  + (i >> 2) + (i >> 4), 63);
        pal[i].g = sat(12 + (i >> 1) - (i >> 4), 63); 
        pal[i].b = sat(6  + (i >> 3) + (i >> 3), 63);
    }
}

void initspr() {
    int x, y, k=0;
    
    for (y = -(SPR_SIZE / 2); y < (SPR_SIZE / 2); y++) {
        for (x = -(SPR_SIZE / 2); x < (SPR_SIZE / 2); x++) {
            //flspr[k++] = sat((int)(0x600 / ((x*x + y*y) + ee)), 255) & 0xFF;
            flspr[k++] = sat((int)(0x200 / ((x*x + y*y) + ee)), 128) & 0xFF;
        }
    }
}

void initbg() {
    int x, y, k=0;
    
    for (y = -(TILE_SIZE / 2); y < (TILE_SIZE / 2); y++) {
        for (x = -(TILE_SIZE / 2); x < (TILE_SIZE / 2); x++) {
            bgspr[k++] = (((0x200 * TILE_SIZE) / (x*x + y*y + ee)) <= (0x10 * TILE_SIZE) ? 0 : 0xFF);
            //bgspr[k++] = sat((0x8000 / (x*x + y*y + ee)), 255);
        }
    }
}

void initsintab() {
    int i, j;
    double r, lut_mul;
    lut_mul = (2 * pi / 65536);
    for (i = 0; i < 65536; i++) {
        r = i * lut_mul;
        sintab [i] = 32767 * sin(r);
        costab [i] = 32767 * cos(r);
        sintabf[i] = sin(r);
        costabf[i] = cos(r);
    }
}

void adjustbuf() {
    int x, y, i, k=0;
    
    for (y = 0; y < Y_GRID; y++) {
        for (x = 0; x < X_GRID; x++) {
            bggrid[k] = (bggrid[k] <= 0x02020202 ? 0x02020202 : bggrid[k] - 0x01010101);
            k++;
        }
    }

    // draw random grid nodes
    for (i = 0; i < 40; i++) {
        k = (rand() % (Y_GRID * X_GRID));
        bggrid[k] = ((rand() & 0x1F) + 8) * 0x01010101;
    }
}

void drawbuf(int i) {
    unsigned int  k=0, c, l, x, y,col;
    unsigned char *buf = &buffer;
    unsigned char *spr = &bgspr;
    
    
    // fill upper gap
    _asm {
        cld
        mov    eax, 0x01010101
        mov    ecx, (X_SIZE * (Y_SIZE - (TILE_SIZE * (Y_SIZE / TILE_SIZE)))) / 8
        mov    edi, buf
        rep    stosd
    }
    
    // adjust pointer
    buf += (X_SIZE * (Y_SIZE - (TILE_SIZE * (Y_SIZE / TILE_SIZE)))) / 2;
    
    // draw background sprites (tiles in fact ;)
    for (y = 0; y < Y_GRID; y++) {
        for (x = 0; x < X_GRID; x++) {
            col = bggrid[k++];
            spr = &bgspr;
            /*
            _asm {
                cld
                mov    esi, spr
                mov    edi, buf
                mov    ebx, TILE_SIZE
                
                __y_loop:
                mov    ecx, (TILE_SIZE / 4)
                rep    movsd
                
                add    edi, (X_SIZE - TILE_SIZE);
                dec    ebx
                jnz    __y_loop
            }
            */
            _asm {
                cld
                mov    edx, col
                mov    esi, spr
                mov    edi, buf
                mov    ebx, TILE_SIZE
                
                __y_loop:
                mov    ecx, (TILE_SIZE / 4)
                
                __x_loop:
                mov    eax, [esi]
                and    eax, edx
                add    esi, 4
                add    eax, 0x01010101
                mov    [edi], eax
                add    edi, 4
                dec    ecx
                jnz    __x_loop
                
                add    edi, (X_SIZE - TILE_SIZE);
                dec    ebx
                jnz    __y_loop
            }
            buf += TILE_SIZE;
        }
        buf += (X_SIZE * (TILE_SIZE-1));
    }
    
    // fill lower gap
    _asm {
        cld
        mov    eax, 0x01010101
        mov    ecx, (X_SIZE * (Y_SIZE - (TILE_SIZE * (Y_SIZE / TILE_SIZE)))) / 8
        mov    edi, buf
        rep    stosd
    }
    
    
    // fill random buffer lines
    for (k = 0; k < 40; k++) {
        c = (rand() & 0xF) * 0x01010101;
        l = ((rand() % (Y_SIZE - 1)) * X_SIZE);
        buf = &buffer[l];
        _asm {
            cld
            mov    eax, c
            mov    ecx, (X_SIZE / 4)
            mov    edi, buf
            
            __loop:
            add    [edi], eax
            add    edi, 4
            dec    ecx
            jnz    __loop            
        }
    }
}

void blend();
#pragma aux blend = " mov   esi, offset blendtab  " \
                    " mov   ebx, offset buffer    " \
                    " mov   edi, offset buffer2   " \
                    " mov   ecx, 224000" \
                    " xor   edx, edx              " \
                    " nop                         " \
                    " __inner:                     " \
                    " mov   dh,    [ebx]          " \
                    " mov   dl,    [edi]          " \
                    " inc   ebx                   " \
                    " mov   al,    [esi + edx]    " \
                    " mov   [edi], al             " \
                    " inc   edi                   " \
                    " dec   ecx                   " \
                    " jnz   __inner                " \
                    modify [esi edi ebx ecx edx];

volatile int tick = 0;
void timer() { tick++;}

int main() {
    int i, j, k = 0;
    int atick;
    
    srand(inp(0x40));
    initsintab();
    initpal();
    initbg();
    initspr();
    vecfill();
 
    if (ptc_open("", X_SIZE, Y_SIZE, 8, 0)) {return 0;}
    
    if (X_SIZE == 320) {
        _asm {    
            // zpizzheno ;)
            mov dx,3d4h   // remove protection
            mov al,11h
            out dx,al
            inc dl
            in  al,dx
            and al,7fh
            out dx,al

            mov dx,3c2h   // misc output
            mov al,0e3h   // clock
            out dx,al

            mov dx,3d4h
            mov ax,00B06h // Vertical Total
            out dx,ax
            mov ax,03E07h // Overflow
            out dx,ax
            mov ax,0C310h // Vertical start retrace
            out dx,ax
            mov ax,08C11h // Vertical end retrace
            out dx,ax
            mov ax,08F12h // Vertical display enable end
            out dx,ax
            mov ax,09015h // Vertical blank start
            out dx,ax
            mov ax,00B16h // Vertical blank end
            out dx,ax
        }
    }
    for (j = 1; j < 256; j++) for (i = 1; i < 256; i++) blendtab[k++] = sat(((i * 128) >> 8) + ((j * 128) >> 8), 255);
    
    ptc_setpal(&pal[0]);
    
    rtc_initTimer(3);
    rtc_setTimer(&timer, rtc_timerRate / 60);

    while (!kbhit()) {
        i = tick;
        ptc_wait();

        //outp(0x3C8, 0); outp(0x3C9, 63); outp(0x3C9, 0); outp(0x3C9, 0);
        
        ptc_update(&buffer);
        adjustbuf();
        drawbuf(i);
        
        // hujak sprites ;)
        for (j = 0; j < flaresnum; j++) {
            pt[j] = p[j];
            vecrotate   ((i << 4) & (lut_size-1),
                        ((i << 5) + (int)((lut_size >> 1) * sintabf[(i << 5) & (lut_size - 1)])) & (lut_size - 1),
                        ((i << 4) + (int)((lut_size >> 1) * costabf[(i << 6) & (lut_size - 1)])) & (lut_size - 1),
                        &pt[j]);
            vecmove     (((X_SIZE >> 2) * costabf[(i << 7) & (lut_size - 1)]),
                         ((Y_SIZE >> 2) * sintabf[(i << 6) & (lut_size - 1)]),
                         -(stardist << 1) - DIST, &pt[j]);
            
            vecproject2d(&pt[j], &p2d[j]);
            vecdraw     (&p2d[j], &pt[j]);
        }
        //blend();
        //outp(0x3C8, 0); outp(0x3C9, 0); outp(0x3C9, 0); outp(0x3C9, 0); 
    }
    
    getch();
    ptc_close();
    rtc_freeTimer();
    _asm {
        mov  ax, 3
        int  10h
    }
}
