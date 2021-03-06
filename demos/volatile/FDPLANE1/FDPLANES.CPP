#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <math.h>
#include <i86.h>
#include "..\flexptc\flexptc.h"
#include "tmap.h"

#define X_RES 320
#define Y_RES 200

#define sat(a, l) (a < l ? a : l)
#define sqr(a)    (a * a)
#define ee         1.0E-6
#define bb        (1.2f * 1.0E+2)
#define pi         3.141592653589793

// extern sin\cos tables
extern signed int sintab[65536],  costab[65536];
extern float      sintabf[65536], costabf[65536];
#include "stuff.h"

// class definition
#include "fdplanes.h"

fdplanes::fdplanes() {
    fdu         = new unsigned int[41*26];
    fdv         = new unsigned int[41*26];
    buffer      = new unsigned char[X_RES*Y_RES];
    palxlat     = new unsigned char[65536];
    blendtab    = new unsigned char[65536];
    pal         = new ptc_palette[256];
    
    // allocate memory for texture and align pointer to 64kb boundary
    texbuf      = new unsigned char[2*65536];
    texture     = (unsigned char*)(((unsigned int)texbuf + 0xFFFF) & ~0xFFFF);
    
    // call init stuff
    initpal();
    inittexture();
    
    // additional init
    volatile int p = 0;
    for (int j = 0; j < 16; j++)  for (int i = 0; i < 256; i++) palxlat[p++] = sat(((i * j)) >> 4, 255); p = 0;
    for (int j = 0; j < 256; j++) for (int i = 0; i < 256; i++) blendtab[p++] = sat(((i * 128) >> 8) + ((j * 192) >> 8), 255);
}

fdplanes::~fdplanes() {
    delete[] fdu;
    delete[] fdv;
    delete[] buffer;
    delete[] palxlat;
    delete[] blendtab;
    delete[] pal;
    delete[] texbuf;
}

void fdplanes::interpolate() {
    typedef struct {int sdy, edy, dx, sy, ey, sx;} _fd;
    
    int k, gridptr = 0;
    unsigned char *p = buffer;
    unsigned char *t = texture;
    _fd u, v;
    
    static int xu, xv, xdu, xdv;
    
    for (int j = 0; j < 25; j++) {
        for (int i = 0; i < 40; i++) {
            
            u.sdy = (fdu[gridptr+41] - fdu[gridptr]) >> 3;
            u.sy  = (fdu[gridptr]);
            
            u.edy = (fdu[gridptr+42] - fdu[gridptr+1]) >> 3;
            u.ey  = (fdu[gridptr+1]);
            
            v.sdy = (fdv[gridptr+41] - fdv[gridptr]) >> 3;
            v.sy  = (fdv[gridptr]);
        
            v.edy = (fdv[gridptr+42] - fdv[gridptr+1]) >> 3;
            v.ey  = (fdv[gridptr+1]);
            
            for (int y = 0; y < 8; y++) {
                
                u.dx = (u.ey - u.sy) >> 3;
                u.sx = u.sy;
                
                v.dx = (v.ey - v.sy) >> 3;
                v.sx = v.sy;
                
                xu = u.sy;
                xv = v.sy;
                
                xdu = u.dx;
                xdv = v.dx;
                
                //tmap8_8bpp(p, t, 8, xu, xv, xdu, xdv);
                
                _asm {
                    xor     eax, eax
                    mov     ebx, xu
                    mov     edx, xv
                    mov     ecx, 8
                    mov     esi, t
                    mov     edi, p
                    
                    _loop:
                    mov     ah, bh              // 1
                    inc     edi                 // .
                    
                    mov     al, dh              // 2
                    add     ebx, xdu            // .
                    
                    mov     al, [esi + eax]     // 3
                    add     edx, xdv            // .
                                        
                    mov     [edi - 1], al       // 4
                    dec     ecx                 // .
                    
                    jnz     _loop               // 5
                }
                
                
                p += X_RES;
                u.sy += u.sdy; u.ey += u.edy; v.sy += v.sdy; v.ey += v.edy;
            }
            gridptr++;
            p -= (X_RES * 8) - 8;
        }
        gridptr++;
        p += (X_RES * 7);
    } 
}

void fdplanes::inittexture() {
    int x, y, i, k=0;
    
    /*
    for (y = 0; y < 256; y++) {
        for (x = 0; x < 256; x++) {
            //texture[((y << 8) + x)] = sat((x ^ y), 255) & 0xFF;
            texture[((y << 8) + x)] = (x ^ y) & 0xFF;
            //texture[((y << 8) + x)] = (x ^ y) | (rand() % 0x100) & 0xFF;
        }
    }
    */
    
    
    for (y = -128; y < 128; y++) {
        for (x = -128; x < 128; x++) {
            //texture[k++] = sat(((int)(0x10000 / ((x*x ^ y*y) + ee))) + ((int)(0x10000 / ((x*x + y*y) + ee))), 255);
            texture[k++] = sat(((int)(0x10000 / ((x*x ^ y*y) + ee))) - ((int)(0x10000 / ((x*x + y*y) + ee))), 255) & 0xFF;
            //texture[((y << 8) + x)] = (x ^ y) & 0xFF;
            //texture[((y << 8) + x)] = (rand() % 0x100) & 0xFF;
        }
    }
    
    
    // blur our texture
    for (k = 0; k < 2; k++)
    for (i = 0; i < 65536; i++) 
        texture[i] = (texture[(i-1)&0xFFFF] + texture[(i+1)&0xFFFF] + 
                      texture[(i-256)&0xFFFF] + texture[(i+256)&0xFFFF]) >> 2; 
    
}

void fdplanes::drawgrid(int ax, int ay, int az, int rx, int ry, int rz) {

    float FOV = (1.0f / 60);
    enum {PlaneSize = 128};
    
    int x, y, z, tptr = 0;
    unsigned int u, v;
    
    vec3f origin, direction, intersect;
    float t, l, fx, fy, delta, a, b, c, t1, t2;
    float tsq, _2a;

    // ASSUMING origin.y = 0!

    for (y = 0; y < 26; y++) {
        for (x = 0; x < 41; x++) {
        
            origin.x = az;// + (sintab[((x << 11) + (ax << 2)) & 0xFFFF] >> 10);
            origin.y = ay;// + (64 * sintabf[(ax << 5) & 0xFFFF]);  
            origin.z = ax;// + (costab[((y << 11) + (ax << 2)) & 0xFFFF] >> 10);
            
            direction.x = (float)((x * 8) - 160) * FOV;
            direction.z = 1;
            direction.y = (float)((y * 8) - 100) * FOV;
            
            normalize(&direction);
            rotate3d(rx, ry, rz, &direction);
            
            t = (PlaneSize) / fabs(direction.y);

            intersect.x = origin.x + (direction.x * t);
            intersect.z = origin.z + (direction.z * t);

            u = (unsigned int)((intersect.x) * 256);
            v = (unsigned int)((intersect.z) * 256);
            
            fdu[tptr] = u;// & 0xFFFFFFFF;
            fdv[tptr] = v;// & 0xFFFFFFFF;
            
            tptr++;
        }
    }
}

void fdplanes::initpal() {
    for (int i = 0; i < 256; i++) {
        pal[i].r = sat(((i >> 3) + (i >> 4)), 63);
        pal[i].g = sat(((i >> 3) + (i >> 3) + (i >> 4)), 63);
        pal[i].b = sat(((i >> 2) + (i >> 3)), 63);
    }
}

int fdplanes::main(int timedemo) {
    int i, j, p = 0;
    
    ptc_setpal(pal);
    
    i = 0;
    while (timedemo ? (i < 1024) : !kbhit()) {
        if (timedemo) i++; else {i = *tick; ptc_wait();}
        
        //outp(0x3C8, 0); outp(0x3C9, 63); outp(0x3C9, 0); outp(0x3C9, 0);
        
        ptc_update(buffer);
        
        drawgrid((i << 2), 0, 0, 
                (((i << 8) + (int)(0x8000 * costabf[(i << 7) & 0xFFFF])) & 0xFFFF),
                (((i << 8) + (int)(0x0000 * sintabf[(i << 6) & 0xFFFF])) & 0xFFFF),
                (((i << 8) + (int)(0x8000 * sintabf[(i << 7) & 0xFFFF])) & 0xFFFF)
                );
        
        //outp(0x3C8, 0); outp(0x3C9, 63); outp(0x3C9, 63); outp(0x3C9, 0);
        
        interpolate();
        
        outp(0x3C8, 0); outp(0x3C9, 0); outp(0x3C9, 0); outp(0x3C9, 0); 
    }
    if (!timedemo) getch();
    
    if (timedemo) printf("timed %d timerticks in %d frames - %f fps", *tick, i, (double)(60 * i) / *tick);
    
    return 0;
}
