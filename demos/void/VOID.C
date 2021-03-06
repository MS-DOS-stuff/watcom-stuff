#include <math.h>
#include <strings.h>
#include <stdlib.h>
#include <conio.h>
#include <stdio.h>
#include "usmplay.h"

#define X_SIZE 320
#define Y_SIZE 200
#define DIST   300

typedef struct {
    float x, y, z, d;
} vertex;

typedef struct { float x, y; } vertex2d;

unsigned char  *screen = 0xA0000;
double pi = 3.141592653589793;

int notimer = 0, noretrace = 0, mode13h = 0, glitch = 0;

#include "3dstuff.c"
#include "twirl.c"
#include "twirl2.c"
#include "3dstuff2.c"
#include "lens.c"
#include "tunnel.c"


void setvidmode(char mode) { 
    _asm {
        mov ah, 0
        mov al, mode
        int 10h 
    } 
}

int main(int argc, char *argv[]) {
    USM *module;
    int p;
    
    for (p = 1; p < argc; p++) {
        if (strcmp(strupr(argv[p]), "NOTIMER") == 0)   notimer   = 1;
        if (strcmp(strupr(argv[p]), "NORETRACE") == 0) noretrace = 1;
        if (strcmp(strupr(argv[p]), "70HZ") == 0)      mode13h   = 1;
        if (strcmp(strupr(argv[p]), "GLITCH") == 0)    glitch    = 1;
    }
    
    HardwareInit(_psp);
    USS_Setup();
    if (Error_Number!=0) { Display_Error(Error_Number); exit(0); }

    if (notimer   == 1) {puts("timer sync disabled\0");}
    if (noretrace == 1) {puts("vertical retrace sync disabled\0");}
    if (mode13h   == 1) {puts("320x200 70Hz mode used\0");}
    
    cputs("init .");

    module = XM_Load(LM_File,0x020202020,"VOID.XM");
    if (Error_Number!=0) { puts(".\0"); Display_Error(Error_Number); exit(0); }
    cputs(".");
    fxInit();
    cputs(".");
    w1Init();
    cputs(".");
    w2Init();
    cputs(".");
    gInit();
    cputs(".");
    lInit();
    cputs(".");
    tInit();
    puts(".");
    
    puts("get down! ;)\0");
    
    setvidmode(0x13);
    
    if (mode13h == 0) _asm {

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
    
    USS_SetAmpli(150); // to prevent clipping on sbcovoxpcspeaker                 
    USMP_StartPlay(module);
    USMP_SetOrder(0);
    
    if (Error_Number!=0) { setvidmode(3); Display_Error(Error_Number); exit(0); }
    
    fxMain();
    w2Main();
    lMain();
    gMain();
    tMain();
    w1Main();
    
    USMP_StopPlay();
    USMP_FreeModule(module);
    
    setvidmode(3);
    if (Error_Number!=0) { Display_Error(Error_Number); exit(0); }
    
    puts("void by wbc ^ b-state 05.07.2015\0");
    puts("greets to everyone and everybody (and you of course! :)\0");
    puts("sorry for slowdows and glitches\0");
}
